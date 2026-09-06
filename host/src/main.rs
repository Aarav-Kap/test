
use anyhow::{anyhow,Context,Result};
use libnspire::{Handle,PID_CX2,VID};
use reqwest::blocking::Client;
use rusb::{Context as UsbContext,UsbContext as _};
use serde_json::json;
use std::{env,thread,time::Duration};

const DIR:&str="/NspireAI";
const REQ:&str="/NspireAI/request.tns";
const RESP:&str="/NspireAI/response.tns";

fn open_calc()->Result<Handle<UsbContext>>{
    let ctx=UsbContext::new()?;
    for dev in ctx.devices()?.iter(){
        let d=dev.device_descriptor()?;
        if d.vendor_id()==VID && d.product_id()==PID_CX2 {
            return Handle::new(dev.open()?).context("could not initialize TI-Nspire USB");
        }
    }
    Err(anyhow!("TI-Nspire CX II not found over USB"))
}
fn ensure_dir(h:&Handle<UsbContext>){
    if h.file_attr(DIR).is_err(){let _=h.create_dir(DIR);}
}
fn read_remote(h:&Handle<UsbContext>,path:&str)->Result<Vec<u8>>{
    let size=h.file_attr(path)?.size() as usize;
    if size>65536{return Err(anyhow!("remote file too large"));}
    let mut buf=vec![0u8;size];
    let n=h.read_file(path,&mut buf,&mut |_|{})?;
    buf.truncate(n); Ok(buf)
}
fn write_remote(h:&Handle<UsbContext>,path:&str,data:&[u8])->Result<()>{
    let _=h.delete_file(path);
    h.write_file(path,data,&mut |_|{})?;
    Ok(())
}
fn parse_request(bytes:&[u8])->Result<(String,String)>{
    let s=String::from_utf8_lossy(bytes).replace("\r\n","\n");
    let mut it=s.splitn(3,'\n');
    if it.next()!=Some("NSPIREAI_REQUEST_V2"){return Err(anyhow!("bad request header"));}
    let id=it.next().ok_or_else(||anyhow!("missing id"))?.strip_prefix("id=").ok_or_else(||anyhow!("bad id"))?.to_string();
    let prompt=it.next().unwrap_or("").trim().to_string();
    if prompt.is_empty(){return Err(anyhow!("empty prompt"));}
    Ok((id,prompt))
}
fn gemini(prompt:&str,key:&str)->Result<String>{
    let url=format!("https://generativelanguage.googleapis.com/v1beta/models/gemini-3.6-flash:generateContent?key={}",key);
    let body=json!({
      "system_instruction":{"parts":[{"text":"You are a concise tutor on a TI-Nspire calculator. Keep answers under 1200 characters, use plain text math, short steps, no markdown tables."}]},
      "contents":[{"role":"user","parts":[{"text":prompt}]}],
      "generationConfig":{"maxOutputTokens":420,"temperature":0.4}
    });
    let v:serde_json::Value=Client::new().post(url).json(&body).send()?.error_for_status()?.json()?;
    let t=v["candidates"][0]["content"]["parts"][0]["text"].as_str().ok_or_else(||anyhow!("Gemini returned no text"))?;
    Ok(t.trim().to_string())
}
fn main()->Result<()>{
    let key=env::var("GEMINI_API_KEY").context("GEMINI_API_KEY not set")?;
    println!("NspireAI bridge v0.5");
    println!("Plug in calculator and open NspireAI. Leave this window running.");
    let mut last_id=String::new();
    loop {
        match open_calc(){
            Ok(h)=>{
                ensure_dir(&h);
                println!("Calculator connected.");
                loop{
                    match read_remote(&h,REQ).and_then(|b|parse_request(&b)){
                        Ok((id,prompt)) if id!=last_id=>{
                            println!("Prompt: {}",prompt);
                            match gemini(&prompt,&key){
                                Ok(ans)=>{
                                    let payload=format!("NSPIREAI_RESPONSE_V2\nid={}\n{}\n",id,ans);
                                    write_remote(&h,RESP,payload.as_bytes())?;
                                    last_id=id;
                                    println!("Answer sent automatically.");
                                }
                                Err(e)=>eprintln!("Gemini error: {e:#}")
                            }
                        }
                        _=>{}
                    }
                    thread::sleep(Duration::from_millis(250));
                }
            }
            Err(_)=>thread::sleep(Duration::from_secs(1))
        }
    }
}
