
use anyhow::{anyhow, Context, Result};
use libnspire::{Handle, PID_CX2, VID};
use reqwest::blocking::Client;
use rusb::{Context as UsbContext, UsbContext as _};
use serde_json::json;
use std::{env, thread, time::Duration};

const CANDIDATE_DIRS: &[&str] = &["/NspireAI", "/documents/NspireAI", "NspireAI"];
const UPLINKS: &[&str] = &[
    "/NspireAI/uplink.tns",
    "/documents/NspireAI/uplink.tns",
    "NspireAI/uplink.tns",
];
const DOWNLINKS: &[&str] = &[
    "/NspireAI/downlink.tns",
    "/documents/NspireAI/downlink.tns",
    "NspireAI/downlink.tns",
];

fn open_calc() -> Result<Handle<UsbContext>> {
    let ctx = UsbContext::new()?;
    for dev in ctx.devices()?.iter() {
        let d = dev.device_descriptor()?;
        if d.vendor_id() == VID && d.product_id() == PID_CX2 {
            return Handle::new(dev.open()?).context("could not initialize TI-Nspire USB");
        }
    }
    Err(anyhow!("TI-Nspire CX II not found over USB"))
}

fn ensure_dirs(h: &Handle<UsbContext>) {
    for dir in CANDIDATE_DIRS {
        if h.file_attr(dir).is_err() {
            let _ = h.create_dir(dir);
        }
    }
}

fn try_read(h: &Handle<UsbContext>, path: &str) -> Result<Vec<u8>> {
    let size = h.file_attr(path)?.size() as usize;
    if size > 65536 { return Err(anyhow!("remote file too large")); }
    let mut buf = vec![0u8; size];
    let n = h.read_file(path, &mut buf, &mut |_| {})?;
    buf.truncate(n);
    Ok(buf)
}
fn first_existing_uplink(h: &Handle<UsbContext>) -> Option<&'static str> {
    for p in UPLINKS {
        if h.file_attr(p).is_ok() { return Some(*p); }
    }
    None
}
fn chosen_downlink_path(h: &Handle<UsbContext>) -> &'static str {
    if let Some(up) = first_existing_uplink(h) {
        if up.starts_with("/documents/") { return "/documents/NspireAI/downlink.tns"; }
        if up.starts_with("NspireAI/") { return "NspireAI/downlink.tns"; }
    }
    "/NspireAI/downlink.tns"
}
fn write_remote(h: &Handle<UsbContext>, path: &str, data: &[u8]) -> Result<()> {
    let _ = h.delete_file(path);
    h.write_file(path, data, &mut |_| {})?;
    Ok(())
}

fn parse_uplink(bytes: &[u8]) -> Result<(String, String)> {
    let s = String::from_utf8_lossy(bytes).replace("\r\n", "\n");
    let mut it = s.splitn(3, '\n');
    if it.next() != Some("NSPIREAI_UPLINK_V1") { return Err(anyhow!("bad uplink header")); }
    let seq = it.next()
        .ok_or_else(|| anyhow!("missing seq line"))?
        .strip_prefix("seq=")
        .ok_or_else(|| anyhow!("bad seq line"))?
        .trim()
        .to_string();
    let prompt = it.next().unwrap_or("").trim().to_string();
    if prompt.is_empty() { return Err(anyhow!("empty prompt")); }
    Ok((seq, prompt))
}

fn write_downlink(h: &Handle<UsbContext>, path: &str, seq: &str, state: &str, text: &str) -> Result<()> {
    let payload = format!("NSPIREAI_DOWNLINK_V1\nseq={}\nstate={}\n{}\n", seq, state, text);
    write_remote(h, path, payload.as_bytes())
}

fn gemini(prompt: &str, key: &str) -> Result<String> {
    let url = format!(
        "https://generativelanguage.googleapis.com/v1beta/models/gemini-3.6-flash:generateContent?key={}",
        key
    );
    let body = json!({
        "system_instruction": {
            "parts": [{
                "text": "You are a concise tutor on a TI-Nspire calculator. Keep answers under 1200 characters, use plain text math, short steps, and no markdown tables."
            }]
        },
        "contents": [{"role":"user","parts":[{"text":prompt}]}],
        "generationConfig": {"maxOutputTokens": 420, "temperature": 0.4}
    });
    let v: serde_json::Value = Client::new()
        .post(url)
        .json(&body)
        .send()?
        .error_for_status()?
        .json()?;
    let text = v["candidates"][0]["content"]["parts"][0]["text"]
        .as_str()
        .ok_or_else(|| anyhow!("Gemini returned no text"))?;
    Ok(text.trim().to_string())
}

fn main() -> Result<()> {
    let key = env::var("GEMINI_API_KEY").context("GEMINI_API_KEY not set")?;
    println!("NspireAI bridge v0.6");
    println!("Plug in calculator and open NspireAI. Leave this window running.");

    let mut last_seq = String::new();

    loop {
        match open_calc() {
            Ok(h) => {
                ensure_dirs(&h);
                println!("Calculator connected.");
                loop {
                    let Some(up_path) = first_existing_uplink(&h) else {
                        thread::sleep(Duration::from_millis(250));
                        continue;
                    };
                    let down_path = chosen_downlink_path(&h);

                    match try_read(&h, up_path).and_then(|b| parse_uplink(&b)) {
                        Ok((seq, prompt)) if seq != last_seq => {
                            println!("Prompt: {}", prompt);
                            let _ = write_downlink(&h, down_path, &seq, "thinking", "Thinking...");

                            match gemini(&prompt, &key) {
                                Ok(answer) => {
                                    let chars: Vec<char> = answer.chars().collect();
                                    let mut i = 0usize;
                                    while i < chars.len() {
                                        let end = (i + 90).min(chars.len());
                                        let part: String = chars[..end].iter().collect();
                                        let _ = write_downlink(&h, down_path, &seq, "streaming", &part);
                                        i = end;
                                        thread::sleep(Duration::from_millis(70));
                                    }
                                    let _ = write_downlink(&h, down_path, &seq, "done", &answer);
                                    last_seq = seq;
                                    println!("Answer sent.");
                                }
                                Err(e) => {
                                    let msg = format!("Gemini error: {}", e);
                                    let _ = write_downlink(&h, down_path, &seq, "error", &msg);
                                    last_seq = seq;
                                    eprintln!("{msg}");
                                }
                            }
                        }
                        Ok(_) => {}
                        Err(_) => {}
                    }

                    thread::sleep(Duration::from_millis(180));
                }
            }
            Err(_) => thread::sleep(Duration::from_secs(1)),
        }
    }
}
