#!/usr/bin/env python3
"""Run a Python snippet inside UE via VibeUE MCP (execute_python_code).
Usage: vibe_py.py <file.py>   OR   echo 'code' | vibe_py.py -
Reads code, POSTs to 127.0.0.1:8088/mcp, prints success/output/result cleanly."""
import sys, json, urllib.request

def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "-"
    code = sys.stdin.read() if src == "-" else open(src).read()
    payload = {"jsonrpc":"2.0","id":1,"method":"tools/call",
               "params":{"name":"execute_python_code","arguments":{"code":code}}}
    req = urllib.request.Request(
        "http://127.0.0.1:8088/mcp",
        data=json.dumps(payload).encode(),
        headers={"Content-Type":"application/json",
                 "Accept":"application/json, text/event-stream"})
    raw = urllib.request.urlopen(req, timeout=120).read().decode()
    # streamable-HTTP: payload is on 'data: ' lines (may be multi-line)
    data = "".join(l[6:] for l in raw.splitlines() if l.startswith("data: "))
    d = json.loads(data)
    res = d.get("result", {})
    print("isError:", res.get("isError"))
    for c in res.get("content", []):
        txt = c.get("text", "")
        try:
            inner = json.loads(txt)
            print("success:", inner.get("success"))
            if inner.get("output"): print("--- output ---\n" + inner["output"])
            if inner.get("result") not in (None,"None"): print("--- result ---\n"+str(inner["result"]))
            if inner.get("error_message"): print("--- error ---\n"+str(inner.get("error_code"))+"\n"+str(inner["error_message"]))
        except Exception:
            print(txt)

if __name__ == "__main__":
    main()
