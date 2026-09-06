#!/usr/bin/env python3
"""
NspireAI Windows bridge v0.1

Transports:
  folder   - watches a normal Windows folder for request.tns (v0.1)
Providers:
  mock     - no internet/API key; proves the pipeline
  openai   - OpenAI Responses API
  ollama   - local Ollama HTTP API

The request/response files are intentionally plain UTF-8 text despite the .tns
extension. WebTILP can move them as files; the Ndless app reads/writes them as
ordinary files.

Request format:
  NSPIREAI_REQUEST_V1
  id=12345
  <prompt>

Response format:
  NSPIREAI_RESPONSE_V1
  id=12345
  <answer>
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

SYSTEM_PROMPT = (
    "You are a concise tutor displayed on a TI-Nspire CX II-T calculator. "
    "Prefer short step-by-step explanations. Use plain text math that displays "
    "well on a small screen. Avoid Markdown tables. Keep answers under about "
    "1400 characters unless the user explicitly asks for detail."
)

def load_config(path: Path) -> dict:
    if not path.exists():
        raise SystemExit(f"Config not found: {path}")
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)

def parse_request(path: Path):
    text = path.read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")
    lines = text.split("\n")
    if not lines or lines[0].strip() != "NSPIREAI_REQUEST_V1":
        raise ValueError("Not an NspireAI v1 request file.")
    if len(lines) < 3 or not lines[1].startswith("id="):
        raise ValueError("Request is missing id= line.")
    req_id = lines[1][3:].strip()
    prompt = "\n".join(lines[2:]).strip()
    if not prompt:
        raise ValueError("Prompt is empty.")
    return req_id, prompt

def write_response(path: Path, req_id: str, answer: str):
    # Small-screen safety: normalize weird whitespace and cap runaway output.
    answer = answer.replace("\r\n", "\n").strip()
    max_chars = 1800
    if len(answer) > max_chars:
        answer = answer[:max_chars - 24].rstrip() + "\n[response truncated]"
    body = f"NSPIREAI_RESPONSE_V1\nid={req_id}\n{answer}\n"
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_text(body, encoding="utf-8")
    temp.replace(path)

def ask_mock(prompt: str, cfg: dict) -> str:
    return (
        "PC bridge is working.\n\n"
        f"You asked: {prompt}\n\n"
        "Next: switch provider in config.json to openai or ollama."
    )


def ask_gemini(prompt: str, cfg: dict) -> str:
    key = os.environ.get("GEMINI_API_KEY", "").strip()
    if not key:
        raise RuntimeError(
            "GEMINI_API_KEY is not set. Run SET_GEMINI_KEY.bat first. "
            "Do not paste the key into config.json."
        )

    model = cfg.get("gemini_model", "gemini-3.6-flash")
    url = (
        "https://generativelanguage.googleapis.com/v1beta/models/"
        f"{model}:generateContent?key={key}"
    )

    payload = {
        "system_instruction": {
            "parts": [{"text": cfg.get("system_prompt", SYSTEM_PROMPT)}]
        },
        "contents": [
            {
                "role": "user",
                "parts": [{"text": prompt}],
            }
        ],
        "generationConfig": {
            "maxOutputTokens": int(cfg.get("gemini_max_output_tokens", 450)),
            "temperature": float(cfg.get("gemini_temperature", 0.4)),
        },
    }

    out = http_json(url, payload)

    candidates = out.get("candidates", [])
    if not candidates:
        feedback = out.get("promptFeedback")
        if feedback:
            raise RuntimeError(f"Gemini returned no answer: {feedback}")
        raise RuntimeError("Gemini returned no candidates.")

    parts = candidates[0].get("content", {}).get("parts", [])
    chunks = []
    for part in parts:
        if isinstance(part, dict) and isinstance(part.get("text"), str):
            chunks.append(part["text"])

    if not chunks:
        raise RuntimeError("Gemini returned no text.")
    return "\\n".join(chunks).strip()

def http_json(url: str, payload: dict, headers: dict | None = None, timeout=120):
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        headers={"Content-Type": "application/json", **(headers or {})},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8"))

def ask_openai(prompt: str, cfg: dict) -> str:
    key = os.environ.get("OPENAI_API_KEY", "").strip()
    if not key:
        raise RuntimeError(
            "OPENAI_API_KEY is not set. Run SET_OPENAI_KEY.bat or set it in "
            "Windows Environment Variables. Do not paste it into config.json."
        )
    model = cfg.get("openai_model", "gpt-5.6-luna")
    payload = {
        "model": model,
        "instructions": cfg.get("system_prompt", SYSTEM_PROMPT),
        "input": prompt,
        "max_output_tokens": int(cfg.get("max_output_tokens", 450)),
    }
    out = http_json(
        "https://api.openai.com/v1/responses",
        payload,
        headers={"Authorization": f"Bearer {key}"},
    )
    # Responses API returns output_text in SDKs, but raw REST is a structured
    # output array. Extract text defensively.
    chunks = []
    for item in out.get("output", []):
        for content in item.get("content", []) if isinstance(item, dict) else []:
            if isinstance(content, dict) and content.get("type") in ("output_text", "text"):
                txt = content.get("text")
                if isinstance(txt, str):
                    chunks.append(txt)
    if chunks:
        return "\n".join(chunks).strip()
    # Some compatible gateways may expose output_text directly.
    if isinstance(out.get("output_text"), str):
        return out["output_text"].strip()
    raise RuntimeError("OpenAI returned no text output.")

def ask_ollama(prompt: str, cfg: dict) -> str:
    base = cfg.get("ollama_url", "http://127.0.0.1:11434").rstrip("/")
    model = cfg.get("ollama_model", "qwen3:4b")
    payload = {
        "model": model,
        "prompt": f"{cfg.get('system_prompt', SYSTEM_PROMPT)}\n\nUser: {prompt}\nAssistant:",
        "stream": False,
        "options": {"num_predict": int(cfg.get("ollama_num_predict", 450))},
    }
    out = http_json(f"{base}/api/generate", payload)
    text = out.get("response")
    if not isinstance(text, str):
        raise RuntimeError("Ollama returned no response text.")
    return text.strip()

PROVIDERS = {
    "mock": ask_mock,
    "gemini": ask_gemini,
    "openai": ask_openai,
    "ollama": ask_ollama,
}

def process_once(workdir: Path, cfg: dict, quiet=False) -> bool:
    req = workdir / "request.tns"
    resp = workdir / "response.tns"
    done = workdir / ".last_request_id"

    if not req.exists():
        return False

    req_id, prompt = parse_request(req)
    last = done.read_text(encoding="utf-8").strip() if done.exists() else ""
    if req_id == last:
        return False

    provider_name = str(cfg.get("provider", "mock")).lower()
    if provider_name not in PROVIDERS:
        raise RuntimeError(f"Unknown provider: {provider_name}")

    if not quiet:
        print(f"\n[{req_id}] {prompt}")
        print(f"Provider: {provider_name}")
        print("Thinking...")

    answer = PROVIDERS[provider_name](prompt, cfg)
    write_response(resp, req_id, answer)
    done.write_text(req_id, encoding="utf-8")

    if not quiet:
        print("\n--- response.tns written ---")
        print(answer)
        print("----------------------------")
    return True

def cmd_test(workdir: Path, cfg: dict, prompt: str):
    workdir.mkdir(parents=True, exist_ok=True)
    req_id = str(int(time.time() * 1000))
    (workdir / "request.tns").write_text(
        f"NSPIREAI_REQUEST_V1\nid={req_id}\n{prompt}\n",
        encoding="utf-8",
    )
    process_once(workdir, cfg)
    print(f"\nFiles are in: {workdir.resolve()}")

def cmd_watch(workdir: Path, cfg: dict, interval: float):
    workdir.mkdir(parents=True, exist_ok=True)
    print("NspireAI bridge running.")
    print(f"Watching: {workdir.resolve()}")
    print("Press Ctrl+C to stop.")
    while True:
        try:
            process_once(workdir, cfg)
        except Exception as e:
            print(f"[bridge error] {e}", file=sys.stderr)
        time.sleep(interval)

def main():
    here = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default=str(here / "config.json"))
    parser.add_argument("--workdir", default=str(here / "exchange"))
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_test = sub.add_parser("test", help="Create a request and process it once")
    p_test.add_argument("prompt", nargs="?", default="Explain why the derivative of x^2 is 2x.")

    p_once = sub.add_parser("once", help="Process request.tns once if it is new")

    p_watch = sub.add_parser("watch", help="Watch the exchange folder")
    p_watch.add_argument("--interval", type=float, default=0.75)

    args = parser.parse_args()
    cfg = load_config(Path(args.config))
    wd = Path(args.workdir)

    try:
        if args.cmd == "test":
            cmd_test(wd, cfg, args.prompt)
        elif args.cmd == "once":
            changed = process_once(wd, cfg)
            if not changed:
                print("No new request.")
        elif args.cmd == "watch":
            cmd_watch(wd, cfg, args.interval)
    except KeyboardInterrupt:
        print("\nStopped.")
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", errors="replace")
        print(f"HTTP error {e.code}: {body}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
