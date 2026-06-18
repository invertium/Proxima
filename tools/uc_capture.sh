#!/usr/bin/env bash
# Capture editor/PIE viewport via UnrealClaude MCP -> writes a JPEG, prints path+size.
out="${1:-/tmp/uc_capture.jpg}"
curl -s --max-time 15 -X POST http://localhost:3000/mcp/tool/capture_viewport \
  -H 'Content-Type: application/json' -d '{}' \
| python3 -c "import sys,json,base64; d=json.load(sys.stdin); raw=base64.b64decode(d['base64']); open('$out','wb').write(raw); print('$out', len(raw), 'bytes |', d.get('message'))"
