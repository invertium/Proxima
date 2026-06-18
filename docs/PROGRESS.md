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

---

## 2026-06-18 — Phase 1 approved; M1 start hit tooling gotcha

- Phase 1 design locked (DECISIONS D5-D11). Plan approved by user ("ok go"). Committed earlier (`427a115`).
- Began M1. Probed UnrealClaude `execute_script` (python) — schema: requires `script_type`, `script_content`, AND `description` (or @Description header). Async: returns task_id → poll `task_status`/`task_result`.
- **GOTCHA (important):** `execute_script` pops a **modal ScriptPermissionDialog** in-editor per call → blocks the game thread → the :3000 HTTP server goes unresponsive until the user clicks. **Unusable for an automated build loop.** An ssh disconnect mid-call left the editor frozen on this dialog (editor pid 27574 alive, ports listening, but HTTP timing out).
- **DECISION shift:** do NOT author content via UnrealClaude `execute_script`. Use **VibeUE Python API** (built for MCP Python w/o modal nags) → need user's VibeUE API key sooner than M4. Use **C++** for sim core. UnrealClaude kept for capture_viewport / spawn_actor / get_level_actors / run_console_command / get_output_log.

**State:** editor frozen on permission dialog — USER must dismiss (Deny). Then: wire VibeUE (API key), proceed M1.

---

## 2026-06-18 — VibeUE chosen as primary authoring tool; key setup pending

- Editor unblocked (user dismissed dialog); :3000 responsive again.
- Probed VibeUE MCP on `http://127.0.0.1:8088/mcp` (streamable-HTTP, SSE `data:` lines). `tools/list` works without auth. 11 tools incl **`execute_python_code`** (no modal dialog!), `editor_control` (screenshots/actions), `manage_asset`, `read_logs`, discover_python_*.
- `tools/call execute_python_code` → clean error: **"valid VibeUE API key required"** (no dialog, no hang). Server validates the key configured in editor settings.
- **DECISION:** VibeUE = primary content/Python authoring tool (modal-free). UnrealClaude = capture/spawn/console/log helpers. C++ = sim core.
- **curl pattern for VibeUE:** `POST /mcp` body `{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":<tool>,"arguments":{...}}}`, Accept `application/json, text/event-stream`; parse `sed -n 's/^data: //p' | tr -d '\n'` → JSON → `result.content[].text`.
- M1 starfield decision: procedural emissive star-dome material via VibeUE Python (user choice).
- **BLOCKED on USER:** paste free vibeue.com API key into VibeUE settings (gear) in editor, then I re-test execute_python_code and start M1.

---

## 2026-06-18 — VibeUE API key configured via config file

- VibeUE key field is **Tools > VibeUE > AI Chat > ⚙️ gear** (NOT the Tools menu root; user couldn't find it).
- Key is read at startup from `Saved/Config/LinuxEditor/EditorPerProjectUserSettings.ini` → `[VibeUE] VibeUEApiKey=` (`GEditorPerProjectIni`), validated by `FMCPServer::ValidateVibeUEApiKeyAsync`. Saved/ is gitignored → key never committed.
- **Wrote the key directly to that ini while editor closed** (must be closed, else editor overwrites file from in-memory GConfig on exit, dropping the hand-added section). Done.
- **GOTCHA (process detection):** `pgrep -f '<editor path>'` matches MY OWN bash command lines (they contain the path) → false "editor running". Use `pgrep -x UnrealEditor` and/or check listening ports 3000/8088 instead. Wasted several rounds on this.

**Next:** USER relaunch editor → VibeUE validates key at startup → I test `execute_python_code` → start M1 (procedural starfield space arena).

---

## 2026-06-18 — ✅ M1 COMPLETE: starfield space arena (verified in PIE)

**VibeUE key validated** — `execute_python_code` returns `VIBEUE_PY_OK 5.7.4`. Both MCPs live (UnrealClaude :3000 / VibeUE :8088), driven by curl; not registered in Claude Code's MCP client (so `/mcp` shows none — expected).

**Built `/Game/Maps/VSlice_Arena`** (4 actors): `Sun` (DirectionalLight), `SkyLight_Fill` (movable, cool blue, 0.6), `Starfield_Dome` (engine Sphere scaled 400 = ~20000uu radius, two-sided unlit `M_Starfield`, no collision/shadow), `GlobalPP` (unbound PostProcessVolume: locked exposure min=max=1.0, bloom 0.6).

**`/Game/Materials/M_Starfield`** — Unlit, two-sided. After several failed node-graph attempts (see gotchas) the reliable solution is a **single `MaterialExpressionCustom` HLSL node** computing a cellular-hash starfield from `VertexNormalWS` (density 70, ~10% of cells lit, per-star brightness variance, tint 0.85/0.92/1.0 ×8). Stars sit at "infinity" (driven by vertex normal direction, not world pos → no parallax/pinching).

**Verified:** PIE started via `LevelEditorSubsystem.editor_request_begin_play()` → `capture_viewport` reported **"Captured PIE viewport"** → sparse white stars on black, no grid. `editor_request_end_play()` → `is_in_play_in_editor()=False`. **PIE play/stop loop now proven** (was the deferred Phase-0 unknown). Screenshots: /tmp/m1_pie.jpg, m1_gameview.jpg.

**TOOLING GOTCHAS (important for all future content milestones):**
- **VibeUE Python is the content workhorse** — modal-free. Helper `tools/vibe_py.py <file.py>` (curl→8088, parses SSE `data:` lines, prints success/output/error_message). Capture helper `tools/uc_capture.sh [out.jpg]` (curl→3000, decodes `base64` key).
- **`connect_material_expressions(from,"",to,IN)`**: the **default/Position input pin name is `""`** (empty string), NOT `"Position"`. Sweep pin names if a connect returns `False` (it returns bool, not raising).
- **Noise node `function` & `position` properties are PROTECTED** (can't set/read). Default noise output range is **-1..1** (`output_min/max` readable). Single-octave Noise threshold/power was unreliable for sparse stars → Custom node won.
- **`MaterialExpressionPower.const_exponent`** DOES apply (verified readback) but `const_b` on some Multiply/Add nodes silently didn't → don't trust node const defaults; a Custom HLSL node sidesteps all of it.
- **Material recreate:** `tools.create_asset` returns **None** if an in-memory pkg of that name lingers → use `load_asset` then **`MaterialEditingLibrary.delete_all_material_expressions(mat)`** to rebuild idempotently.
- **Light color** wants `LinearColor`, not `Color`.
- **Editor camera** resets to default between some ops → set each time via `UnrealEditorSubsystem.set_level_viewport_camera_info(loc,rot)`. `LevelEditorSubsystem.editor_set_game_view(True)` hides grid/icons for clean editor shots.
- **Capture parse:** UnrealClaude `capture_viewport` response → image is the **`base64`** top-level key (1024x576 JPEG).

**Next = M2:** `ASpaceship` (C++) + placeholder mesh + 3rd-person follow cam. Done-when: [S] ship in 3rd-person vs space; [L] spawn logged. (Remind user to git-commit M1: docs + Content/ + tools/.)
