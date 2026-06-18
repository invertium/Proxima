# PROGRESS LOG (append-only)

Terse, factual, newest at bottom. Update at end of every step/session.

---

## 2026-06-17 — Phase 0: Environment audit

**Done & verified (hard facts from system probes):**
- OS: CachyOS Linux (Arch-based), kernel 7.0.12-1-cachyos, x86_64
- Host: julian-gaming-cachy
- CPU: 12 logical cores
- RAM: 31 GiB total, ~25 GiB available
- Disk: /home on nvme0n1p2, 607 GB free of 928 GB
- GPU: AMD Radeon RX 9070 XT (RDNA4, gfx1201) — RADV/radeonsi, Mesa 26.1.2
- Vulkan: 1.4.348 present (vulkaninfo OK) — UE Linux RHI = Vulkan, so this is the critical capability ✅
- Display: Wayland session (wayland-0) + XWayland (DISPLAY=:0) — real display present ✅
- Toolchain present: git 2.54, clang 22.1.6 (system; UE uses its own bundled toolchain), cmake, dotnet, python3 (miniconda 3.13)
- Unreal Engine: NOT installed (searched /opt, $HOME, common locations) ❌

**Decisions this session:**
- Target UE **5.7** (not 5.8) — user directive. Reason: both required plugins officially support 5.7; 5.8 (released today 2026-06-17) is unverified for these plugins.
- Project root: /home/julian/gitrepos/spaceGame

**Blocked on user (GUI/manual actions):**
- Install UE 5.7 on Linux
- Install + build UnrealClaude and VibeUE plugins
- Paste VibeUE API key, enable MCP

**Next:** After UE 5.7 + plugins installed, run the MCP smoke test (connect → list tools → PIE on empty level → 1 screenshot → read back → stop PIE). Do NOT start game work until that loop passes.

---

## 2026-06-18 — UE 5.7 installed (binary) & verified

- User installed **precompiled binary** UE **5.7.4** at `/home/julian/UnrealEngine/UE_5.7` (65 GB). Source zip: `~/Downloads/Linux_Unreal_Engine_5.7.4.zip` (30 GB).
- Went binary route → **GitHub↔Epic source-access gate is now moot** (only needed for source build).
- Verified runnable:
  - `UnrealEditor` + `UnrealEditor-Cmd` present & executable; `ldd` → all libs resolved ✅
  - Headless engine-init probe: reached `Engine Version: 5.7.4-51494982`, ran Trace Server + clock benchmark + shader-compiler init, clean shutdown ✅. (Probe exit=1 only because of intentional bogus `-run=None`; harmless.)
- Plugins (UnrealClaude=Natfii, VibeUE=kevinpbuckley) are **public** repos → I can clone without Epic access.

**Next:** (1) Create blank UE project at project root, (2) clone both plugins into `Plugins/`, (3) build & launch editor, (4) USER: enable plugins + paste VibeUE API key + enable MCP servers, (5) MCP smoke test.

---

## 2026-06-18 — Project created, plugins built & load-verified

**Project:** `SpaceGame` C++ project created from engine `TP_Blank` template at project root. `SpaceGame.uproject` enables ModelingToolsEditorMode + UnrealClaude + VibeUE. (EngineAssociation left "" — we build/launch via explicit engine path.)

**Plugins cloned into `Plugins/`:**
- UnrealClaude (Natfii) v1.5.0 — nested at `Plugins/UnrealClaude/UnrealClaude/` (UE discovers recursively, OK). Has git submodule `mcp-bridge` (Node MCP server `ue5-mcp-server`) → initialized + `npm install` done (136 deps).
- VibeUE (kevinpbuckley) v4.0.

**Compat check:** both `.uplugin` declare `"EngineVersion": "5.7.0"`, both list Linux. ✅

**Build:** `Build.sh SpaceGameEditor Linux Development`.
- GOTCHA: VibeUE `Source/VibeUE/Private/Tools/SkillsTools.cpp` lines 880-881 had `/*` sequences inside a `/* */` block comment → fatal under engine's `-Wcomment -Werror`. **Patched** (reworded comment, removed `/*`). Local edit to plugin source — re-applies if plugin updated.
- Result: **Succeeded**. Built `libUnrealEditor-{SpaceGame,VibeUE,UnrealClaude}.so`.

**Load verification (headless `-run=pythonscript -nullrhi`):** both plugins mount; `VibeUE Module has started`; UnrealClaude found `claude` CLI and registered MCP tools incl. `capture_viewport, spawn_actor, move_actor, get_level_actors, set_property, blueprint_query, blueprint_modify, execute_script`. Python ran, clean exit, no Fatal/assert/crash. ✅

**MCP endpoints (from plugin READMEs):**
- UnrealClaude MCP server: `http://localhost:3000` (auto-starts on editor load, `/mcp/status`). **No API key.** Owns `capture_viewport` + actor control. Bridge = `Plugins/UnrealClaude/UnrealClaude/Resources/mcp-bridge/index.js` (stdio MCP).
- VibeUE MCP server: `http://127.0.0.1:8088/mcp`, needs header `Authorization: Bearer <API_KEY>`. Optional Python proxy on 8089. Registration: `claude mcp add --transport stdio VibeUE-Claude -- npx -y mcp-remote http://127.0.0.1:8088/mcp --transport http-only --allow-http --header "Authorization:Bearer <KEY>"`.

**GUI readiness:** all UE/CEF runtime libs present on CachyOS (nss, nspr, gbm, atk, gtk3, X libs, alsa, pango, cairo, gdk-pixbuf). Missing only `wl-clipboard`/`xclip` (UnrealClaude clipboard-image-paste convenience; NOT needed for launch or screenshots).

**De-risk insight:** the high-risk screenshot loop can be proven with **UnrealClaude alone** (capture_viewport, no API key). VibeUE key deferred until after smoke test.

**Next:** Launch editor GUI → register UnrealClaude MCP in this Claude Code session → MCP smoke test (start PIE on empty level → capture_viewport → read back → stop PIE). NOT YET DONE. No game work until it passes.

---

## 2026-06-18 — MCP smoke test (in progress): screenshot loop works; fixed stale-frame bug

**Connection method chosen:** talk to UnrealClaude's HTTP MCP server **directly via curl** (no Claude Code MCP client registration / no session restart needed). API: `GET /mcp/tools`, `POST /mcp/tool/<name>` (JSON body) on `http://localhost:3000`. Learned from bridge `lib.js`.

**Verified working:**
- Connect + list tools: `/mcp/status` + `/mcp/tools` → 28 tools incl `capture_viewport`. ✅
- `open_level {action:new}` → created blank map. ✅
- `capture_viewport` → 1024x576 JPEG, base64 decoded to file, **viewed a real rendered frame**. ✅ → **the high-risk Linux screenshot readback loop WORKS** (Vulkan→JPEG→base64→HTTP→decode).

**BUG FOUND + FIXED — stale viewport frames:**
- Symptom: after switching to an empty level, capture still returned byte-identical old-terrain frame. Editor level viewport only redraws on demand (unfocused / non-realtime), so `Viewport->ReadPixels()` read a stale backbuffer.
- Root cause: `MCPTool_CaptureViewport.cpp` called `ReadPixels()` with no redraw.
- Fix: added `Viewport->InvalidateDisplay(); Viewport->Draw();` before ReadPixels (ReadPixels flushes render thread, so Draw completes first).
- GOTCHA: first attempt also added `FlushRenderingCommands()` → link error `undefined symbol` (needs RenderCore module, not linked). Removed it as redundant. Rebuild then **Succeeded**.
- Live Coding hot-patch attempt (`run_console_command LiveCoding.Compile`) did nothing — Live Coding not active in this session. Fix requires editor restart to load new `.so`.

**State:** patched `libUnrealEditor-UnrealClaude.so` rebuilt & on disk (21:02). Editor still running old `.so`.

**PIE note:** UnrealClaude exposes NO dedicated start/stop-PIE tool (28 tools are editor-scope). `capture_viewport` prefers PIE viewport if active, else editor viewport. With the redraw fix, editor-viewport capture is fresh, which covers the verification loop. Driving actual PIE (play/stop) will need VibeUE Python or input automation — validate when first needed for gameplay.

**Next:** USER restart editor → re-run capture on empty level, expect FRESH frame (void, not terrain) → smoke test gate PASSED → then Phase 1 game design questions.

---

## 2026-06-18 — ✅ MCP SMOKE TEST PASSED (Phase 0 gate cleared)

After editor restart (new pid loaded patched `.so`):
- Connect + 28 tools ✅
- `open_level new` → empty level; `capture_viewport` → **FRESH frame: black void + editor grid + origin gizmo** (15950 bytes), NOT the stale 43396-byte terrain. **Stale-frame fix verified.** ✅
- Capture-reflects-scene-state proven by terrain-map frame vs empty-map frame (two distinct live renders). ✅

**=> The Linux screenshot loop is solid: Vulkan render (RX 9070 XT) → force redraw → ReadPixels → JPEG → base64 → HTTP → decode → view. This was the highest-risk item; it works.**

**Tooling quirks noted (NOT blockers, revisit in Phase 2):**
- `spawn_actor` param is `class` (not `class_path`).
- `set_property` on `StaticMeshComponent.StaticMesh` (asset path) and `RelativeScale3D` reported success but did NOT apply (cube stayed mesh-less, scale 1). Editor camera also not auto-framing origin. → For reliable asset/mesh/transform authoring, prefer **VibeUE Python API** over UnrealClaude `set_property`; validate per-milestone. UnrealClaude is solid for: capture_viewport, spawn_actor, get_level_actors, delete_actors, run_console_command, get_output_log.
- No dedicated PIE start/stop tool in UnrealClaude (editor-viewport capture covers the loop; PIE via VibeUE later).

**VibeUE:** server live on 8088 (needs Bearer API key for use). NOT yet wired into this session — deferred; UnrealClaude alone cleared the gate. Set up when first needed.

**Connection model for the dev loop:** drive UnrealClaude via `curl POST http://localhost:3000/mcp/tool/<name>` from Bash (no Claude Code MCP-client registration needed). Helper pattern: capture → base64-decode to /tmp/*.jpg → Read the image.

**Editor state:** running (pid 27574), empty unsaved Untitled level, lit viewmode. Nothing saved yet.

**NEXT = Phase 1:** ask user 3 game-design questions, then write PROJECT_PLAN milestones + DECISIONS (BP/C++ split, folders, asset strategy). Remind user to git-commit the plugin patch + docs at this boundary.
