#!/usr/bin/env bash
# OS-level screenshot of the full screen via spectacle (KDE/Wayland, headless).
# This is the ONLY UI-inclusive capture path on Linux: UnrealClaude capture_viewport
# and HighResShot grab the scene BEFORE the Slate/UMG layer, and VibeUE's
# ScreenshotService / editor_control screenshot are Windows-only. So to screenshot
# any UMG HUD/console, the Unreal Editor window must be the visible foreground
# window, then run this. Usage: tools/os_capture.sh [out.png]
OUT="${1:-/tmp/os_capture.png}"
spectacle -b -n -f -o "$OUT" >/dev/null 2>&1
echo "$OUT ($(stat -c %s "$OUT" 2>/dev/null || echo 0) bytes)"
