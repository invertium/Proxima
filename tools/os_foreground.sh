#!/usr/bin/env bash
# Bring the Unreal Editor window to the foreground on KDE Plasma 6 Wayland.
# No kdotool/wmctrl/xdotool on this box — use KWin's D-Bus scripting via qdbus6.
# Needed before tools/os_capture.sh, since spectacle grabs whatever is on screen.
set -euo pipefail

JS="$(mktemp /tmp/activate_ue.XXXXXX.js)"
cat > "$JS" <<'KWIN'
const wins = (typeof workspace.windowList === "function")
    ? workspace.windowList()
    : workspace.clientList();
for (let i = 0; i < wins.length; i++) {
    const w = wins[i];
    const cap = (w.caption || "").toString();
    const cls = (w.resourceClass || "").toString();
    if (cls.toLowerCase().indexOf("unreal") !== -1 ||
        cap.indexOf("Unreal") !== -1 ||
        cap.indexOf("SpaceGame") !== -1) {
        w.minimized = false;
        workspace.activeWindow = w;
        break;
    }
}
KWIN

PLUGIN="ueFg$$"
qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$JS" "$PLUGIN" >/dev/null
qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.start >/dev/null
qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript "$PLUGIN" >/dev/null
rm -f "$JS"
sleep 1
echo "editor foregrounded"
