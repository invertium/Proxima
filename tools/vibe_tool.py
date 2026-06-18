#!/usr/bin/env python3
"""Call an arbitrary VibeUE MCP tool. Usage: vibe_tool.py <tool> '<json-args>' [img_out.png]
If a result content item carries base64 image data, decode it to img_out."""
import sys, json, base64, urllib.request
tool = sys.argv[1]
args = json.loads(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2] else {}
img_out = sys.argv[3] if len(sys.argv) > 3 else None
payload = {"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":tool,"arguments":args}}
req = urllib.request.Request("http://127.0.0.1:8088/mcp",
    data=json.dumps(payload).encode(),
    headers={"Content-Type":"application/json","Accept":"application/json, text/event-stream"})
raw = urllib.request.urlopen(req, timeout=180).read().decode()
data = "".join(l[6:] for l in raw.splitlines() if l.startswith("data: "))
d = json.loads(data)
res = d.get("result", {})
print("isError:", res.get("isError"))
for c in res.get("content", []):
    t = c.get("type")
    if t == "image" and c.get("data") and img_out:
        open(img_out,"wb").write(base64.b64decode(c["data"]))
        print("IMG ->", img_out, "(", len(base64.b64decode(c["data"])), "bytes )")
    elif t == "text":
        txt = c.get("text","")
        # text may itself reference a saved file path or be JSON
        print(txt[:1500])
    else:
        print("content-keys:", list(c.keys()))
