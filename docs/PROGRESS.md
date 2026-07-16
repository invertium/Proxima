# PROGRESS LOG (append-only)

Terse, factual, newest at bottom. Update at end of every step/session.

---

## 2026-06-17 — Phase 0: Environment audit

**Done & verified (hard facts from system probes):**
- OS: CachyOS Linux (Arch-based), kernel 7.0.12-1-cachyos, x86_64
- Host: dev-host
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
- Project root: ~/spaceGame

**Blocked on user (GUI/manual actions):**
- Install UE 5.7 on Linux
- Install + build UnrealClaude and VibeUE plugins
- Paste VibeUE API key, enable MCP

**Next:** After UE 5.7 + plugins installed, run the MCP smoke test (connect → list tools → PIE on empty level → 1 screenshot → read back → stop PIE). Do NOT start game work until that loop passes.

---

## 2026-06-18 — UE 5.7 installed (binary) & verified

- User installed **precompiled binary** UE **5.7.4** at `~/UnrealEngine/UE_5.7` (65 GB). Source zip: `~/Downloads/Linux_Unreal_Engine_5.7.4.zip` (30 GB).
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

M1 committed `440a4e6`.

---

## 2026-06-18 — ✅ M2 COMPLETE: ASpaceship pawn + follow cam (verified in PIE)

**Code:** `Source/SpaceGame/Ships/Spaceship.{h,cpp}` — `APawn` with `ShipRoot` (unrotated USceneComponent, keeps actor +X forward decoupled from mesh), `ShipMesh` (engine Cone placeholder, scaled 1/1/2), `SpringArm` (len 900, pitch -12, no collision test, camera + rotation lag) + `FollowCamera`. `AutoPossessPlayer = Player0` so PIE views through the follow cam. `BeginPlay` logs spawn. `SpaceGame.Build.cs`: added `PublicIncludePaths.Add(ModuleDirectory)` so `"Ships/..."`-qualified includes work across the D8 layout.

**Verified:** Built (Build.sh SpaceGameEditor) → **Succeeded**. New UCLASS needs editor reload → user restarted editor → class visible (`/Script/SpaceGame.Spaceship`). Placed `PlayerShip` in VSlice_Arena, saved. PIE → ship rendered 3rd-person vs starfield through its follow cam (auto-possess works); log `LogTemp: ASpaceship spawned: Spaceship_0 at X=0 Y=0 Z=0`. PIE stopped clean.

**TOOLING — two important findings this session:**
- **:3000 (UnrealClaude) DEAD this session — port bind race.** New editor tried to bind `127.0.0.1:3000` while the *old* editor was still shutting down and holding it → `LogHttpListener: Error: HttpListener unable to bind to 127.0.0.1:3000`; UnrealClaude does NOT retry. So `capture_viewport` is unavailable until a clean restart (old fully dead first). VibeUE :8088 won the race and is fine. **Mitigation for future restarts:** fully quit the editor and confirm `pgrep -x UnrealEditor` empty + `ss -ltn | grep 3000` empty BEFORE relaunching.
- **Linux screenshot WITHOUT :3000 → `HighResShot`.** `unreal.SystemLibrary.execute_console_command(world, "HighResShot 1280x720")` writes a PNG to `Saved/Screenshots/LinuxEditor/HighresScreenshotNNNNN.png` (works in editor AND PIE; captures the active/PIE viewport). Async — can take up to ~15s to flush; poll for the file. This is now the **primary, :3000-independent screenshot path** for the dev loop. VibeUE `editor_control screenshot` is **Windows-only** (errors `CAPTURE_FAILED ... only supported on Windows`) — do NOT rely on it on Linux. Helper added: `tools/vibe_tool.py <tool> '<json>' [img_out]`.
- VibeUE `editor_control` also exposes `start_pie`/`stop_pie`/`pie_status` (alternative to `LevelEditorSubsystem.editor_request_begin_play/end_play`, which is what M1/M2 used).

**Known placeholders to revisit:** cone points up/teardrop (orientation = visual only; fix the constructor `SetRelativeRotation` to point +X when M3 adds movement & forward matters); hull renders default checker (no material — give it a basic emissive in M13 polish); follow-cam frames the ship low (feel → hand-test).

**Next = M3:** `UShipMovementComponent` (impulse throttle + turn) + temp test input. Done-when: [L] speed/heading change on input; [S] before/after show movement. (Will rebuild C++ → fix cone orientation then. Remind user to commit M2: Source/ + docs + Content/ map update + tools/vibe_tool.py.)

M2 committed `ce2fad2`.

---

## 2026-06-19 — ✅ M3 COMPLETE: ship impulse movement (verified in PIE)

**Code:** `Source/SpaceGame/Components/ShipMovementComponent.{h,cpp}` — `UActorComponent` (BlueprintSpawnable, ClassGroup SpaceGame). `SetThrottle(0..1)` eases `CurrentSpeed` toward `Throttle*MaxSpeed` via `FInterpConstantTo(Acceleration)`; `SetTurn(-1..1)` yaws the owner at `MaxTurnRate`; per-tick translate along owner `+X` (`AddActorWorldOffset`, bSweep). Tunables `MaxSpeed=1800`, `Acceleration=1200`, `MaxTurnRate=60` (Engineering power will scale these at M7). State is plain UPROPERTYs (replication-ready, D7); `SetThrottle/SetTurn` log. `ASpaceship` ctor now `CreateDefaultSubobject<UShipMovementComponent>("MovementComp")`; **cone reoriented `FRotator(90,0,0)` so apex points actor +X (forward)** — fixes the M2 deferred orientation note.

**Verified:** Built `Build.sh SpaceGameEditor Linux Development` → **Succeeded**. Editor restarted → `/Script/SpaceGame.ShipMovementComponent` loaded; placed `Spaceship_0` auto-gained `MovementComp`. PIE → `mc.set_throttle(1.0); mc.set_turn(1.0)`. Log: `BEFORE loc=(0,0,0) yaw=0 speed=0` → `[ShipMovement] Throttle set to 1.00 (target speed 1800)` / `Turn set to 1.00` → samples: speed 1800, yaw stepping ~20°/sample (≈60°/s), ship on a curved arc out to ~3000 uu. **[L] speed + heading change confirmed.** **[S]** /tmp/m3_before.jpg vs m3_after.jpg: starfield panned/rotated + ship reorientation (follow cam tracks the moving ship → VSlice ship is auto-possessed). PIE stopped clean.

**GOTCHA — fresh editor boots into a throwaway `Untitled_1` Open-World level, NOT your last map.** On launch the editor opened a *new* Open World template level (landscape + VolumetricCloud + WorldPartition HLODs, world path `/Temp/Untitled_1`) — looked like our arena content had vanished. **`LevelEditorSubsystem.load_level("/Game/Maps/VSlice_Arena")` SILENTLY NO-OP'd** (returned, printed, but editor world stayed `Untitled_1`) — likely refused because the temp level was dirty. **Fix: `unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/VSlice_Arena")`** force-loads (discards the temp level) → editor world = `VSlice_Arena`, all 5 saved actors present (Sun/SkyLight/Starfield dome StaticMeshActor/GlobalPP/Spaceship_0). Nothing was lost; M1/M2 content is persisted. **At session start always `load_map` the working level before inspecting/PIE.**

**Possession note:** in VSlice_Arena (no PlayerStart) the placed ship's `AutoPossessPlayer=Player0` wins → follow cam is the view. In a map *with* a PlayerStart (e.g. the Untitled template) the GameMode spawns/possesses a `DefaultPawn` instead. (Real input → M6 Helm.)

**Next = M4:** `StationManager` + station switcher (keys 1/2/3) + HUD shell. Done-when: [S] station-select bar; switching changes active console label. (Remind user to commit M3: Source/Components/ + Source/Ships/ + docs.)

M3 committed `acd8480`.

---

## 2026-06-19 — ✅ M4 COMPLETE: station switcher + HUD shell (verified in PIE)

**Code (Source/SpaceGame/Core/, D8):** `EStation{Helm,Weapons,Engineering}` (StationTypes.h); `ASpaceGameMode` (PlayerControllerClass=ABridgePlayerController, DefaultPawnClass=null so placed ship still auto-possesses); `ABridgePlayerController` (BeginPlay creates HUD + `AddToViewport`; legacy `InputComponent->BindKey(One/Two/Three)` → `SelectHelm/Weapons/Engineering` → `SetStation`; logs `[Bridge] Active station -> X`; resolves HUD class via `LoadClass("/Game/UI/Common/WBP_BridgeHUD.WBP_BridgeHUD_C")` at play-time so no extra restart); `UBridgeHUDWidget` (`meta=(BindWidget)` TextBlocks HelmTab/WeaponsTab/EngineeringTab/ActiveConsoleLabel; `SetActiveStation` tints active tab cyan + sets big label). Build.cs += UMG/Slate/SlateCore.

**UMG:** `/Game/UI/Common/WBP_BridgeHUD` reparented to `UBridgeHUDWidget`, authored via `unreal.WidgetService`. GameMode override set on VSlice_Arena WorldSettings (`default_game_mode`) + map saved.

**Verified:** PIE → PC=`BridgePlayerController` possessing `Spaceship_0`; `SetStation(Weapons/Engineering/Helm)` → log `[Bridge] Active station -> …` + `GetStation()` agrees [L]. Composited [S] (Helm/Weapons/Engineering): active tab cyan, big label tracks station — /tmp/m4_{helm,weapons,engineering}_live.png.

**🔴 CRITICAL TOOLING — UMG/HUD screenshots on Linux need an OS capture.** Every in-engine capture path grabs the scene BEFORE the Slate/UMG layer or is Windows-only:
- UnrealClaude `capture_viewport` (uc_capture.sh) → scene only, NO UMG/`stat` overlays.
- `HighResShot` (console) → scene only, NO UMG.
- VibeUE `ScreenshotService.capture_editor_window/active_window` and `editor_control screenshot` → return `success=False "only supported on Windows platform"`.
- VibeUE `WidgetService.capture_preview` → renders the widget alone (white bg), AND runs `NativeConstruct` so it always shows the construct-default state (can't show a switched state) + returns empty path while PIE is running.
- **WORKING PATH:** `spectacle -b -n -f -o out.png` (headless full-screen) → `tools/os_capture.sh`. Requires the **Unreal Editor window be the visible foreground window** (no window-activation CLI on this KDE/Wayland box — kdotool/wmctrl/xdotool absent; only qdbus6/gdbus). So: ask the user to foreground+maximize the editor once, run PIE in the active viewport, then drive state changes + `os_capture` repeatedly (no further user action while it stays focused). Use this for all UI milestones (M5 radar, M6–M8 consoles).

**Other gotchas:** `WidgetService` doesn't create the WBP — use `unreal.WidgetBlueprintFactory` + `AssetTools.create_asset(..., WidgetBlueprint, factory)` with `parent_class` = the C++ widget. **`HorizontalBox` children rendered overlapping in preview → switched to CanvasPanel absolute positioning (Position/Size X-Y aliases) for deterministic layout.** Canvas point-anchored slots default to a fixed 100×30 box → must set `Size X/Y` or big text clips. `unreal.WidgetBlueprintLibrary` / `PlayerController.input_key` are NOT exposed in this Python (can't enumerate live widgets or inject keys from script).

**Next = M5:** Tactical radar widget (2D): player-centered, range rings, heading, world-actor blips. Done-when: [S] radar w/ player blip+ring; enemy present → blip at correct relative pos. (Commit M4: Source/Core/ + Build.cs + Content/UI/ + Content/Maps/VSlice_Arena.umap + tools/os_capture.sh + docs.)

## 2026-06-20 — ✅ M5 COMPLETE: tactical radar widget (verified in PIE)

**Code (D5/D8):** `URadarContactComponent` (Source/Components/, marker tag; any actor carrying it draws as a blip; `FLinearColor BlipColor` default hostile red — player ship omits it = radar centre). `URadarWidget` (Source/Core/, `UUserWidget` subclass) draws everything in `NativePaint` via `UWidgetBlueprintLibrary::DrawLines` (no textures, fully deterministic): 3 range rings (R / 0.66R / 0.33R), cross axes, a player marker + heading arrow that rotates with the ship yaw, and a cross-blip per contact. North-up, player-centred: world **+X → up** (−screenY), world **+Y → right** (+screenX); contacts gathered via `TObjectIterator<URadarContactComponent>` filtered to the player's `World`, scaled by `RadarRangeUU` (20000uu = outer ring), clamped to the edge ring if out of range. No new module deps (UMG/Slate already added at M4).

**UMG:** `/Game/UI/Common/WBP_Radar` (root `SizeBox` 440×440, reparented to `URadarWidget`) embedded into `WBP_BridgeHUD` RootCanvas as `TacticalRadar`, anchored **bottom-left** (Anchors min/max (0,1), Alignment (0,1), Offsets Left=40 Top=−40, size 440×440) so it's resolution-independent (matters on the 3440×1440 ultrawide).

**Test rig:** `/Game/Ships/BP_RadarContact` (Actor parent + `StaticMeshComponent` Body=Engine Cube ×3 + `URadarContactComponent`), authored via `unreal.BlueprintService` (create_blueprint → add_component → set_component_property → compile). Instance `RadarTestContact` placed at world **(3000,0,0)** = 3000uu due north of the ship (at origin, yaw 0); saved into VSlice_Arena.

**Verified:** PIE → OS-capture (/tmp/m5_hud2.png): radar bottom-left shows rings + axes + cyan player marker with heading arrow pointing **up** (yaw 0 → forward = world +X = north); **red hostile blip directly north**, ~15% of radius from centre = 3000/20000 ✓ — bearing AND range correct [S]. Scene shot (uc_capture, /tmp/m5_scene.jpg) confirms the cube floats ahead of the ship.

**Gotchas:** Editor-placed actors lack `add_component_by_class`/instance-component API in this Python → bake the component into a Blueprint (`BlueprintService.add_component`) and place that instead (also reusable for M9 enemies). `StaticMeshActor`'s native root mesh isn't easily settable → parent the BP from `Actor` and add your own `StaticMeshComponent`. `WidgetComponentSnapshot` structs are NOT subscriptable — use `.get_editor_property("widget_name")` etc. `WidgetBlueprintGeneratedClass` has no `is_child_of`/`get_super_class` — check parentage via `isinstance(get_default_object(gen), unreal.RadarWidget)`.

**Next = M6:** Helm console — wire throttle/turn into a dedicated full-screen console (folds the radar in). Done-when: [S]+[L] Helm console drives the ship (throttle/turn readouts track UShipMovementComponent). (Commit M5: Source/Core/RadarWidget.* + Source/Components/RadarContactComponent.h + Content/UI/Common/WBP_Radar + WBP_BridgeHUD + Content/Ships/BP_RadarContact + Content/Maps/VSlice_Arena.umap + docs.)

## 2026-06-20 — ✅ M6 COMPLETE: Helm console (input + live readouts), verified in PIE

**Code (D5/D10):** `ASpaceship::GetMovementComp()` accessor (BlueprintPure). `ABridgePlayerController` gains Helm input via legacy key binds (D9): `W`/`S` step `ThrottleLevel` ±0.2 → `MovementComp->SetThrottle`; `A`/`D` IE_Pressed → `SetTurn(∓1)`, IE_Released → `SetTurn(0)`. **All Helm input gated to `CurrentStation == EStation::Helm`**; leaving Helm calls `SetTurn(0)` (stop steering) but keeps throttle (ship coasts). `GetShipMovement()` resolves `Cast<ASpaceship>(GetPawn())->GetMovementComp()`. New `UHelmConsoleWidget` (Source/Core/) polls the possessed ship in `NativeTick` → `THR %d%%` / `SPD %4.0f` / `HDG %03d` (yaw wrapped 0..360) + `ThrottleBar->SetPercent`; all binds `BindWidgetOptional`. `UBridgeHUDWidget` gains `BindWidgetOptional UWidget* HelmConsole` and toggles its visibility in `SetActiveStation` (SelfHitTestInvisible at Helm, Collapsed otherwise). No new module deps.

**UMG:** `/Game/UI/Stations/WBP_HelmConsole` (parent `UHelmConsoleWidget`): root CanvasPanel; `ThrottleText`/`SpeedText`/`HeadingText` (font 40, cyan, right-justified) + `ThrottleBar` anchored **top-right** (anchor (1,0), align (1,0), Offsets Left=−60, Top 120/184/248/312); the `WBP_Radar` instance `Radar` anchored **bottom-left**. Radar **moved out of** WBP_BridgeHUD into the console (per "embeds radar") — HUD's old `TacticalRadar` removed; `WBP_HelmConsole` embedded full-screen into RootCanvas as `HelmConsole` (anchor stretch 0,0..1,1).

**Verified (PIE):** drove via `MovementComp->SetThrottle(0.6)/SetTurn(0.5)` (same downstream the W/A/S/D binds call; keys hand-confirmable in focused PIE like 1/2/3 at M4). Live reads: speed ramped 0→**1080** uu/s (=0.6×1800), yaw swept continuously at ~30°/s (turn works; the −180..180 wrap explains transient negative yaw), forward = +X at yaw 0, ship flew a full loop from origin [L]. OS-capture /tmp/m6_console.png: console shows **THR 60% / SPD 1080 / HDG 344** + 60% throttle bar + radar bottom-left [S]. /tmp/m6_weapons.png: switching to Weapons **collapses** the whole console (gating works) [S].

**Gotchas:** `WidgetService.set_font(path, comp, font_info, ...)` needs a `WidgetFontInfo` struct — to just change size use `set_property(path, comp, "Font.Size", "40")`. PIE pawn/PC from script: `unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()` → `GameplayStatics.get_player_controller(world,0)` → `get_controlled_pawn()`. Key injection still not scriptable → drive the sim component directly. The big M4 center station label (HELM/WEAPONS) still shows over the console — harmless station indicator, revisit at polish.

**Next = M7:** Engineering console — power distribution that scales system stats (D11: power caps MaxSpeed/turn etc.). Done-when: [S]+[L] reallocating power changes ship performance (e.g. more engine power → higher MaxSpeed reflected in Helm SPD ceiling). (Commit M6: Source/Ships/Spaceship.h + Source/Core/{BridgePlayerController.*,BridgeHUDWidget.*,HelmConsoleWidget.*} + Content/UI/Stations/WBP_HelmConsole + Content/UI/Common/WBP_BridgeHUD + docs.)

## 2026-06-20 — ✅ M7 COMPLETE: Engineering console + power model, verified in PIE

**🟢 TOOLING WIN — the OS-capture loop is now fully self-service.** Two blockers gone: (1) I **launch the editor myself** from a background shell (`nohup …/UnrealEditor <uproject> &`; DISPLAY=:0 / WAYLAND_DISPLAY=wayland-0 inherited; MCP 3000/8088 up in seconds — poll the VibeUE Python API until new UCLASSes load). (2) I **foreground the editor myself** via KWin D-Bus scripting (`tools/os_foreground.sh` → qdbus6 loadScript/start a tiny KWin JS that sets `workspace.activeWindow`). So no user action is needed for screenshots anymore: launch → begin_play → set state via Python → `os_foreground.sh` → `os_capture.sh`. See [[linux-umg-screenshot-workflow]].

**Code (D5/D11):** new `UPowerComponent` (Source/Components/) with `UENUM EShipSystem{Engine,Weapons,Shields,Count}`; per-system power in a `TArray<float>` (init nominal 1.0×Count in ctor), `GetSystemPower`/`SetSystemPower`(clamp 0..`MaxPerSystem`=2.0)/`AdjustSystemPower`/`GetTotalPower`; `ReactorBudget`=3.0 (soft, display-only). `UShipMovementComponent` now scales target speed by engine power: lazily caches the sibling `UPowerComponent` (`FindComponentByClass`), `GetEffectiveMaxSpeed() = MaxSpeed * enginePower` (1.0 if no power comp), used for `TargetSpeed`. `ASpaceship` creates `PowerComp` + `GetPowerComp()` accessor. `ABridgePlayerController`: Engineering input via **arrow keys gated to the Engineering station** — Left/Right cycle `SelectedSystem`, Up/Down `AdjustSystemPower(±0.1)`; `GetSelectedSystem()` for the console highlight. New `UEngineeringConsoleWidget` (Source/Core/) polls the power comp in `NativeTick` → per-system `NAME %d%%` label + `ProgressBar` (percent = power/MaxPerSystem) with the **selected** row tinted amber, plus a `REACTOR LOAD x%/y%` line. `UBridgeHUDWidget` gains `BindWidgetOptional UWidget* EngineeringConsole`, toggled visible at Engineering.

**UMG:** `/Game/UI/Stations/WBP_EngineeringConsole` (parent `UEngineeringConsoleWidget`): center-anchored rows — `EngineText`/`WeaponsText`/`ShieldsText` (font 34, centered) each over `EngineBar`/`WeaponsBar`/`ShieldsBar`, + `ReactorText`. Embedded full-screen into WBP_BridgeHUD as `EngineeringConsole`. **The big M4 center station label (`ActiveConsoleLabel`) was COLLAPSED** (Visibility=Collapsed) — it overlapped console content and the cyan tab already marks the active station.

**Verified (PIE):** at nominal engine power, full throttle → effMax 1800, speed ramped to 1800. Set `EnginePower=2.0` → `GetEffectiveMaxSpeed()` 1800→**3600**, ship speed climbed 1800→**3600** [L]. /tmp/m7_eng_final.png: Engineering console shows ENGINE 200% (amber/selected, full bar), WEAPONS 140%, SHIELDS 60%, REACTOR LOAD 400%/300% [S]. /tmp/m7_helm_boosted.png: Helm reads **SPD 3600** at THR 100% — the boosted ceiling, proving Engineering→ship-performance [S].

**Gotchas:** `WidgetService.set_property` returns **False (silently)** for nested struct paths (`Slot.LayoutData.Anchors.Minimum`, `Font.Size`) on some pre-existing widgets, even with PIE stopped — simple props (`Text`, `Visibility`, `RenderOpacity`) still work. So to neutralise the M4 label I collapsed it rather than re-anchor/resize it. (Authoring anchors/fonts reliably works at *add_component* time on freshly-added components; retrofitting an old one is the flaky case.)

**Next = M8:** Weapons console + firing (D11 weapon power → beam recharge). Done-when: [S]+[L] Weapons console fires a beam/projectile at a target; recharge gated by weapon power. (Commit M7: Source/Components/PowerComponent.* + Source/Components/ShipMovementComponent.* + Source/Ships/Spaceship.{h,cpp} + Source/Core/{BridgePlayerController.*,BridgeHUDWidget.*,EngineeringConsoleWidget.*} + Content/UI/Stations/WBP_EngineeringConsole + Content/UI/Common/WBP_BridgeHUD + tools/os_foreground.sh + docs.)

## 2026-06-20 — ✅ M8 COMPLETE: Weapons console + beam firing (verified in PIE)

**Code (D5/D10/D11):** new `UWeaponComponent` (Source/Components/) — a forward beam weapon. `Charge` (0..1, `VisibleInstanceOnly`) recharges per-tick: `Charge += BaseRechargeRate(0.4) * WeaponPowerScale() * DeltaTime` clamped 0..1, where `WeaponPowerScale()` lazily caches the sibling `UPowerComponent` and reads `GetSystemPower(EShipSystem::Weapons)` (D11 — weapon power gates recharge). `CycleTarget()` gathers all `URadarContactComponent` owners in the player's world (excludes the player ship), sorts by name for stable order, and steps an index → `CurrentTarget`. `FireBeam()` blocks (returns false) if `Charge < 1`, no target, or target out of `BeamRange(15000)`; otherwise zeroes `Charge`, logs `[Weapon] BEAM FIRED at %s (range %.0f uu)`, and arms a `DrawDebugLine` beam (`BeamDrawTime` 0.35s) drawn each tick from muzzle to target. Accessors `GetCharge`/`GetCurrentTarget`/`GetTargetRange`/`IsTargetInRange`. `ASpaceship` creates `WeaponComp` + `GetWeaponComp()`. `ABridgePlayerController`: Weapons input gated to the Weapons station — **Right cycles target** (shared with Engineering's Right; each handler early-outs off-station), **Space fires**. New `UWeaponsConsoleWidget` (Source/Core/) polls in `NativeTick` → `TARGET %s` / `RANGE %.0f` / `IN RANGE`|`OUT OF RANGE`|`NO TARGET` / `BEAM READY`|`BEAM %d%%` + `ChargeBar` (percent, green when ready / amber while charging). `UBridgeHUDWidget` gains `BindWidgetOptional UWidget* WeaponsConsole`, toggled visible at Weapons. No new module deps.

**UMG:** `/Game/UI/Stations/WBP_WeaponsConsole` (parent `UWeaponsConsoleWidget`): target readout block top-right (anchor (1,0), Left=−60, Tops 120/178/236/320, font 34, right-justified) `TargetNameText`/`TargetRangeText`/`TargetStatusText`/`ChargeText`, the `ChargeBar` (Top 378, 520×26), and the `WBP_Radar` instance `Radar` bottom-left. Embedded full-screen into WBP_BridgeHUD as `WeaponsConsole`. Test rig: a second `RadarTestContact2` placed at world (0,18000,0) in VSlice_Arena so target-cycling has a near (3000uu, in range) and a far (18000uu, out of range) contact.

**Verified (PIE):** Weapons console showed **TARGET BP_RadarContact_C_0 / RANGE 3000 / IN RANGE / BEAM READY** with a green full charge bar [S /tmp/m8_console.png]. Cycling (Right) → **BP_RadarContact_C_1 / RANGE 18000 / OUT OF RANGE**, and `FireBeam()` returned false (range-gated) [S /tmp/m8_oor.png]. Firing the in-range target logged `[Weapon] BEAM FIRED at BP_RadarContact_C_0 (range 3000 uu)` and dropped `Charge` to 0 [L]. Power-gated recharge confirmed: with weapon power 0 the charge held at 0.00 over 3s; restoring weapon power to 2.0 refilled it to 1.00 — recharge speed tracks weapon power [L].

**Gotchas:** `WeaponComponent.Charge` is `VisibleInstanceOnly` → read-only via reflection (couldn't force-set from Python; drove it by adjusting weapon power and ticking). The `DrawDebugLine` beam did **not** appear in the `uc_capture` scene shot — the test target sat directly ahead so the beam was nearly end-on to the follow cam, and `capture_viewport` may exclude debug draws; relied on `FireBeam()==True` + the log line + the console [S] instead. Right is bound for both Engineering (select-next) and Weapons (cycle-target) — fine because both handlers early-out unless their station is active.

**Next = M9:** enemy AI + health/damage — the beam should land damage on a target with hit-points; targets need an `UHealthComponent` and the weapon should apply damage on fire. Done-when: [S]+[L] firing reduces a target's health to destruction. (Commit M8: Source/Components/WeaponComponent.* + Source/Core/WeaponsConsoleWidget.* + Source/Ships/Spaceship.{h,cpp} + Source/Core/{BridgePlayerController.*,BridgeHUDWidget.*} + Content/UI/Stations/WBP_WeaponsConsole + Content/UI/Common/WBP_BridgeHUD + Content/Maps/VSlice_Arena.umap + docs.)

## 2026-06-20 — ✅ M9 COMPLETE: enemy ship + simple AI + health components (verified in PIE)

**Code (D5/D7/D8):** new `UHealthComponent` (Source/Components/) — hull + shield pool, `ApplyDamage(Amount)` spends shields-first then hull (clamped ≥0), logs `[Health] X took N dmg -> shield a/b hull c/d`, fires a `FOnHealthDeath` dynamic multicast the first time hull hits 0 (guarded against re-broadcast); `MaxHull=100`/`MaxShield=50`, runtime `Hull`/`Shield` mirror them at BeginPlay; getters + `IsAlive()`. (Despawn/explosion on death = M10; player-side damage = M11.) New `AEnemyShip` (Source/Ships/, APawn) — self-contained Tick AI (no AIController, `AutoPossessAI=Disabled`): resolves the player pawn via `GameplayStatics::GetPlayerPawn`, yaw-only `RInterpConstantTo` toward it, flies forward at `MoveSpeed`(1100) until inside `StandoffDistance`(6000) then holds, and once within `EngageRange`(12000) fires every `FireInterval`(2.5s) — logged `[EnemyAI] X FIRE at Y (range R)` + a hostile-orange `DrawDebugLine` (player damage lands M11). Coarse `EEnemyAIState{Idle,Approach,Engage}` surfaced via `GetAIState()` and logged on transition. Carries a `URadarContactComponent` (shows on radar + makes it a valid player weapon target) and a `UHealthComponent` (player beam destroys it at M10). Cube mesh (scaled 2,2,1) to read distinct from the player's cone. No new module deps (GameplayStatics/DrawDebugHelpers are in Engine).

**Map:** placed `EnemyShip_1` at world (20000,0,0) facing −X (toward the player at origin) in VSlice_Arena, and **removed the two static M8 `RadarTestContact` rig actors** so the live enemy is the sole radar contact (the weapon's CycleTarget now picks up the enemy directly). Map saved.

**Verified (PIE):** [L] `/tmp/ue_editor.log` — `EnemyShip_0 spawned at X=20000` → `state -> Approach` → (~7.2s later, range ≤12000) `state -> Engage` → `FIRE at Spaceship_0` repeating at exact 2.5s intervals; range closed 9000→6067→5994 and held at standoff (≈6000) — proves spawn + approach + interval firing + the AI state machine. [S] before/after approach (single contact): /tmp/m9_before.png (Approach, dist 19841 — red blip at radar **outer edge**, enemy off-screen) vs /tmp/m9_after.png (Engage, dist 5999 — red blip pulled **in toward centre**, enemy cube now visible in-scene above the player).

**🔴 TOOLING GOTCHAS (this session):** (1) **Don't run actor ops / save immediately after `editor_request_end_play()`** — doing `get_all_level_actors`/`destroy_actor`/`save_dirty_packages` in the same instant PIE was tearing down **segfaulted the editor (Signal 11 in Core)**. Let teardown settle (sleep ~3-4s) or do edits before starting PIE. Also `Actor.get_component_by_class()` returns None for everything *during* PIE teardown — only reliable once the editor world is settled. (2) **After a fresh editor launch, `spectacle -f` (fullscreen) grabs a blank white frame** (19443 B) even though the editor window is the active foreground window (KWin confirms `active=true`) — a Wayland/Vulkan surface issue. **`spectacle -b -n -a` (active-window) captures correctly** (~200-530 KB). Switch os_capture to `-a` if `-f` returns a tiny/blank PNG. (3) Frequent per-frame Python sampling **starves the PIE game thread** (enemy crawled ~145 uu/s instead of 1100) — sample sparsely; to let the sim run at speed, `sleep` in bash without Python calls. (4) `unreal.Vector` has `.length()`, not `.size()`.

**Next = M10:** Weapons→damage — the player beam calls `UHealthComponent::ApplyDamage` on the hit target (shields→hull), recharge stays gated by Beams power, add beam FX; enemy at 0 hull → despawn + explosion. Done-when: [L] enemy hull drops on fire; [S] beam FX + enemy destroyed. (Commit M9: Source/Components/HealthComponent.* + Source/Ships/EnemyShip.* + Content/Maps/VSlice_Arena.umap + docs.)

## 2026-06-20 — ✅ M10 COMPLETE: weapons→damage + enemy destruction (verified in PIE)

**Code (D5/D11):** `UWeaponComponent::FireBeam()` now lands damage — on a confirmed in-range hit it resolves the target's `UHealthComponent` (`FindComponentByClass`) and calls `ApplyDamage(BeamDamage)` (new `UPROPERTY BeamDamage=25`); shields absorb first, then hull (the health comp owns that split + death). Recharge stays gated by Beams power (unchanged from M8). Beam FX enhanced: the fired beam draws a brighter/thicker `DrawDebugLine` (14u) **plus a cyan impact flare `DrawDebugSphere`** (140u) at the hit point for `BeamDrawTime`. `AEnemyShip` binds its `HealthComp->OnDeath` (in BeginPlay) to a new `HandleDeath(AActor*)` `UFUNCTION` that draws a **primitive-first explosion** (three concentric orange→red debris shells, 300/650/1000u, persisting ~2s so the kill reads on-screen) then `Destroy()`s the ship — the radar contact + pawn vanish. No new module deps, no map change.

**Verified (PIE):** drove via Python (key inject still not scriptable) — set station Weapons, Beams power 2.0 (fast recharge), `CycleTarget`→EnemyShip, then fired 6 times as it recharged. **[L]** clean shields→hull progression, one hit each: `[Health] took 25 dmg -> shield 25/50`, `shield 0/50`, then `hull 75/100`, `50`, `25`, `0` → `OnDeath` → `Destroy()`; the 6th shot's fire-step returned **`DESTROYED(0 enemies)`** (actor gone). 2 hits drained the 50 shield, 4 drained the 100 hull = 6 total at 25 dmg. **[S]** /tmp/m10_step_4.png — Weapons console (TARGET EnemyShip_0 / RANGE 5998 / IN RANGE / BEAM recharging) with the **cyan beam + impact flare** striking the enemy cube; /tmp/m10_step_6.png — the **orange/red explosion burst** at the kill point with the **radar blip gone** (enemy despawned).

**Gotchas:** the editor's stdout log is **block-buffered when redirected to a file** (`nohup > log`), so the last ~2s of `UE_LOG` lines (here the killing `hull 0/100` + despawn) can lag behind real events — trust the live Python return value / screenshot for the final frame rather than waiting on the tail. `unreal.Station.WEAPONS` / `unreal.ShipSystem.WEAPONS` are the Python names for the `EStation`/`EShipSystem` enums; `PowerComponent.set_system_power(unreal.ShipSystem.WEAPONS, 2.0)` boosts recharge for scripted firing.

**Next = M11:** player damage + shields + lose — enemy beams damage the player (give the player ship a `UHealthComponent`), shield power scales mitigation (D11), 0 hull → defeat screen. Done-when: [L] player hull drops; [L] shield-power changes mitigation; [S] defeat screen. (Commit M10: Source/Components/WeaponComponent.* + Source/Ships/EnemyShip.* + docs.)

## 2026-06-20 — ✅ FEATURE: mouse/touch-interactable consoles + tooltips

**Request:** make the Weapons + Engineering consoles clickable (mouse/touch), with tooltips — they were keyboard-only (D9 primitive-first).

**Code (D5/D10):** `ABridgePlayerController::BeginPlay` now shows the cursor and switches to **`FInputModeGameAndUI`** (DoNotLock, don't hide on capture) so UMG buttons get clicks while the Helm WASD / Engineering+Weapons arrow+Space binds still reach the controller — the console buttons are authored **non-focusable** (`IsFocusable=false`) so they never steal keyboard focus. New public `EngAdjustSystem(EShipSystem, bIncrease)` (BlueprintCallable) — the mouse path for the per-row ±: it selects the clicked row (highlight follows) and steps its power by the same `PowerStep` the arrows use. `UEngineeringConsoleWidget` gained `NativeConstruct` + six `BindWidgetOptional UButton`s (`{Engine,Weapons,Shields}{Minus,Plus}Btn`) bound to six `UFUNCTION` OnClicked handlers → `PC->EngAdjustSystem(sys, +/-)`. `UWeaponsConsoleWidget` gained `NativeConstruct` + `CycleTargetBtn`/`FireBtn` → `WeaponComp->CycleTarget()` / `FireBeam()` (same calls as Right/Space). All binds optional, so a key-only layout still runs.

**UMG (authored via VibeUE Python):** WBP_EngineeringConsole — a `[-]`/`[+]` button flanking each of the three power rows (center-anchored, aligned to each bar's Y: −143/−13/117, X ±430, 68×58), label TextBlock font 40 centered, per-button `ToolTipText` ("Boost/Reduce X power (or select with Left/Right then Up/Down)"). WBP_WeaponsConsole — a blue **CYCLE TARGET** (top 450, 520×72) and red **FIRE** (top 540, 520×96) button under the charge bar (anchor 1,0 matching the readout column), tinted via `BackgroundColor`, tooltips ("Select the next radar contact (or Right arrow)" / "Fire the beam — needs full charge + in-range target (or Space)"). Both compiled + saved.

**Verified (PIE):** [S] /tmp/btn_eng.png — Engineering shows all six −/+ buttons by their rows + visible cursor; /tmp/btn_wpn.png — Weapons shows CYCLE TARGET + FIRE under "BEAM READY"; /tmp/btn_eng2.png — after driving the buttons' exact action, ENGINE 130% / SHIELDS 90% (selected, amber) / REACTOR 320%. [L] `EngAdjustSystem(ENGINE,+)×3` → engine 1.00→1.30, `(SHIELDS,−)` → 1.00→0.90, selected system followed to SHIELDS — the identical call each `OnEngine/Shields…` handler makes. Weapons buttons call the M8/M10-proven `CycleTarget`/`FireBeam`.

**Limitation / hand-test note:** this headless Wayland box has **no input-injection tool** (no ydotool/wtype/xdotool) and the live PIE widget isn't reachable via reflection (HUDWidget + BindWidget pointers are protected/non-script-readable), so the literal Slate `OnClicked`→handler hop couldn't be triggered by automation — it's standard `BindWidget` + `OnClicked.AddDynamic` wiring (compiled, names match the WBP exactly), and the actual mouse-down + tooltip-hover + the keyboard-still-works regression are for the user to confirm by hand (same status the key binds have always had). Tooltips set via `set_property("ToolTipText", …)` (succeeded) but only show on real hover.

## 2026-06-20 — ✅ M11 COMPLETE: player damage + shield mitigation + defeat screen (verified in PIE)

**Code (D5/D7/D11):** `UHealthComponent` gained **shield-power mitigation** — `GetShieldMitigation()` reads a lazily-cached sibling `UPowerComponent`'s Shields power and returns `clamp(power * ShieldMitigationScale(0.35), 0, MaxMitigation(0.8))`; `ApplyDamage` now scales incoming by `(1 - mitigation)` before the shield-pool/hull split, and logs the mitigated figure. `ASpaceship` carries a `HealthComp` (`MaxHull=100`, `MaxShield=0` — the player's "shields" are pure engineering-power mitigation, D11) + `GetHealthComp()`. `AEnemyShip::FireAtPlayer` now lands `EnemyBeamDamage(8)` on the target's `UHealthComponent` (+impact flare) instead of just logging. `ABridgePlayerController` overrides `OnPossess` to bind the player ship's `OnDeath` → `HandlePlayerDeath` → `ShowEndScreen("DEFEAT"…)`, which builds the overlay (LoadClass `WBP_EndScreen` at play time), `AddToViewport(100)`, shows the cursor, `FInputModeUIOnly`, and `SetGamePaused(true)`. New reusable `UEndScreenWidget` (`SetOutcome(Title, Subtitle, Color)` — one WBP serves M11 defeat + M12 victory). `UBridgeHUDWidget` gained a `NativeTick` driving an always-on `HullText` ("HULL n%", green>50% / amber>25% / red).

**UMG:** new `/Game/UI/Common/WBP_EndScreen` (parent `UEndScreenWidget`): full-screen dark `Bg` image (82% black) + center `TitleText` (font 110) + `SubtitleText` (font 40). `WBP_BridgeHUD` gained `HullText` (top-center, anchor 0.5,0, Top 86, font 34). No map change.

**Verified (PIE):** **[L] player hull drops from enemy beams** — let the enemy approach + engage; it fired at Spaceship_0 and hull fell 100 → 63.6 (live, with 0.35 nominal mitigation), and the HUD `HULL` readout tracked it down (48% → 38%). **[L] shield power changes mitigation (clean atomic assert)** — same 40 raw dmg: at Shields power **0.0** → mitigation 0.00 → hull 100→60 (**LOSS 40**); at power **2.0** → mitigation 0.70 → hull 60→48 (**LOSS 12**). **[S]** /tmp/m11_defeat.png — the red **DEFEAT** / "Your ship was destroyed." overlay over the frozen (paused) game with `HULL 0%` (red); /tmp/m11_hud.png — the in-play `HULL 38%` amber readout under the station bar.

**Notes:** the atomic mitigation assert must run in one Python call (the game thread doesn't tick mid-script) so the enemy's concurrent fire can't contaminate the before/after reads — the first attempt was muddied because the enemy had already dropped the player to 22 hull; re-running it within the first ~3s of a fresh PIE (before the enemy engages, hull full) gave the clean 40-vs-12 result. `is_game_paused`/`set_game_paused` via `GameplayStatics`; unpause before `editor_request_end_play` for a clean stop.

**Next = M12:** win condition + encounter flow — destroy the enemy → victory screen (reuse `UEndScreenWidget` with "VICTORY"/green); full loop in one PIE session. Done-when: [S] sequence engage→manage power→destroy→victory. (Commit M11: Source/Components/HealthComponent.* + Source/Ships/{Spaceship,EnemyShip}.* + Source/Core/{BridgePlayerController.*,BridgeHUDWidget.*,EndScreenWidget.*} + Content/UI/Common/{WBP_EndScreen,WBP_BridgeHUD} + docs.)

## 2026-06-21 — ✅ M12 COMPLETE: win condition + full encounter loop (verified in PIE)

**Code (D5):** `ABridgePlayerController::BeginPlay` now calls `BindEnemyDeaths()` — gathers every placed `AEnemyShip` (`GetAllActorsOfClass`) and subscribes to each one's `HealthComp->OnDeath` (`AddUniqueDynamic`) → `HandleEnemyDeath`. On each hostile death it recounts survivors (an enemy reads `IsAlive()==false` once its hull hits 0, even before `Destroy()`), and when **zero remain** it calls the M11 `ShowEndScreen` with a green **"VICTORY" / "All hostiles destroyed."** — same overlay + pause + UIOnly path as defeat, so `UEndScreenWidget`/`WBP_EndScreen` serve both outcomes. Logs `[Bridge] Tracking N hostile(s)` and `[Bridge] VICTORY`. No UMG/map change (reused the M11 end screen).

**Verified (PIE) — full loop in one session [S sequence]:** **engage** /tmp/m12_engaged.png (Weapons console, TARGET EnemyShip_0 / RANGE 5724 / IN RANGE / BEAM READY, HULL 48%) → **manage power** (Engineering: Weapons 200% for fast recharge, Shields 150% for survivability) → **destroy** /tmp/m12_fire_3.png (cyan beam + impact flare on the enemy, BEAM recharging, player taking return fire HULL 41%); shields 50→25→0 then hull 100→75→50→25→0 over 6 shots → **victory** /tmp/m12_victory.png (green **VICTORY** overlay, target cleared to NO TARGET, game paused, player survived at HULL 29%). [L] `fire_beam` returned `ALL DEAD` on the 6th shot; `is_game_paused` True after the kill (ShowEndScreen ran). This closes the core combat loop (M9–M12): enemy AI + health → beam damage + destruction → player damage + defeat → win condition + victory.

**Next = M13:** visual polish (emissive hull, Niagara thrusters, beam/explosion FX replacing debug draws, backdrop, HUD styling). Done-when: [S] side-by-side stylized vs primitive. (Commit M12: Source/Core/BridgePlayerController.* + docs.)

## 2026-06-21 — ✅ M13 (partial): stylized ships + emissive FX (verified in PIE)

**Materials (authored via VibeUE `MaterialEditingLibrary`):** `/Game/Art/Materials/` — `M_PlayerHull` / `M_EnemyHull` (metallic dark base + faction-tinted emissive trim, cyan / red), and unlit high-emissive glow mats `M_GlowCyan` / `M_GlowOrange` / `M_GlowRed` for engines + FX.

**Ship models (C++ composited primitives + emissive, replaces the bare cone/cube):** `ASpaceship` is now a fighter — sleek cone fuselage (`M_PlayerHull`) + two swept wings + a bright cyan engine-exhaust cylinder (`M_GlowCyan`). `AEnemyShip` is a menacing cruiser — wide blocky hull (`M_EnemyHull`) + twin forward prongs + a glowing red sensor "eye" (`M_GlowRed`) + an orange stern engine glow. Materials loaded via `FObjectFinder` (created first so the paths resolve at build time).

**FX (real emissive meshes, replaces DrawDebug):** new reusable `ABeamFx` (Source/FX/) — a glowing cylinder stretched/oriented between two points that thins out over its short life and self-destructs; `static Spawn()`. New `AExplosionFx` — an emissive sphere that expands then collapses (sin(t·π)) and self-destructs. The player weapon fires a cyan `ABeamFx`; the enemy fires an orange one; enemy death spawns an `AExplosionFx`. The old `DrawDebugLine/Sphere` beam/explosion code is gone.

**Verified (PIE):** [S] /tmp/m13_ships.png — the player as a glowing cyan fighter (swept wings + engine exhaust) vs the old grey cone (cf. m12_engaged.png); /tmp/m13_enemy2.png — the red emissive enemy cruiser; /tmp/m13_beam.png — a bright cyan **beam-mesh FX** connecting the fighter to the enemy (the standout shot). Materials + meshes load and render correctly; faction emissive reads clearly (cyan player / red enemy).

**Not captured / follow-ups:** the **explosion FX** plays in-game but resisted a clean screenshot here — it's a 0.65s flash, and the MCP→capture round-trip (~1.5s) outruns it; killing the lone enemy also triggers the victory pause, which freezes the flash at scale 0 (spawned that same frame). A clean shot needs a second hostile (no victory pause) + a longer-lived flash, and a spawn-into-PIE-world path Python doesn't expose; an attempt to add a temp 2nd enemy + re-PIE **segfaulted the editor** (the known spawn/teardown instability). Deferring the explosion screenshot + the rest of M13 polish (Niagara thrusters — impractical via MCP; backdrop/nebula; HUD font styling) to a follow-up. The visual upgrade (ships + beam) is committed and PIE-verified.

**Next:** finish M13 polish (explosion shot, backdrop, HUD styling) or proceed to M14 (audio/feedback). (Commit: Source/Ships/{Spaceship,EnemyShip}.* + Source/Components/WeaponComponent.* + Source/FX/{BeamFx,ExplosionFx}.* + Content/Art/Materials/* + docs. No map change.)

## 2026-06-21 — ✅ M13 (models): real ship meshes (Quaternius CC0) swapped in (verified in PIE)

**Assets:** imported the **Quaternius "Ultimate Spaceships" pack (CC0, public domain)** — fetched the Google-Drive folder via `gdown` into the gitignored `_assets_dl/`, then imported two ships into `/Game/Art/Meshes/` with their palette textures (`/Game/Art/Textures/`) and per-ship lit materials (`M_Insurgent` blue, `M_Imperial` red, built from the color-variant palette PNG → base color). Player = **Insurgent** (sleek fighter), enemy = **Imperial** (bulky cruiser). Import is scripted in `tools/_import_ships.py`.

**Import gotcha (important):** FBX import through the **live MCP Python path crashes the editor** — UE 5.7's Interchange importer does an async task-graph wait that re-enters the MCP server's worker thread and trips a `RecursionGuard` assertion (`TaskGraph.cpp:689`). The headless `-run=pythonscript` commandlet ALSO crashes, because the VibeUE plugin assumes a Slate app (`SlateApplication.h:321`). **Fix that works:** in the GUI editor, disable Interchange's importers via CVar first (`Interchange.FeatureFlags.Import.FBX/PNG/GLTF 0`) so imports fall back to the **legacy synchronous factories** — those run on the calling thread with no task-graph wait, so MCP import succeeds. (Saved to memory.)

**Ships (C++):** `ASpaceship`/`AEnemyShip` constructors now load the imported `UStaticMesh` + material via `FObjectFinder` and drop the M13 composited-primitive hulls (wings/prongs/sensor/engine-glow members removed from both pawns). The models' length runs along local **Y**, so `ShipMesh` gets `SetRelativeRotation(yaw -90)` to point the nose at actor **+X** (forward), `SetRelativeScale3D(0.6)` (assets are ~1100–1850uu long), and the player spring-arm grew 900→1600 for the larger hull. Beam/explosion FX (`M_GlowCyan`/`M_GlowOrange`) unchanged.

**Verified (PIE):** [S] /tmp/ship_player.png — blue Insurgent fighter from the follow-cam; /tmp/twoship3.png — Insurgent + the red Imperial cruiser framed together (nose-forward, correct scale). Throttle confirmed the ship travels along its visual nose (+X). Combat loop still live — flying unshielded into the enemy triggered the **DEFEAT** screen as expected.

**Follow-ups:** still open from M13-partial — explosion screenshot, backdrop/nebula, HUD font styling. Old unused materials `M_PlayerHull`/`M_EnemyHull`/`M_GlowRed` left in place (harmless). (Commit: Source/Ships/{Spaceship,EnemyShip}.* + Content/Art/{Meshes,Textures,Materials}/* + tools/_import_ships.py + .gitignore + docs.)

## 2026-06-21 — ✅ M13 (backdrop): nebula behind the starfield (verified in PIE)

**What:** the arena already had a `Starfield_Dome` (Sphere + unlit two-sided `M_Starfield`) on a black background. Added a space **nebula** behind it without touching the stars: (1) flipped `M_Starfield` to **additive** blend so its black background reads as transparent (stars unchanged, now add over the nebula); (2) authored `/Game/Materials/M_Nebula` (unlit, two-sided) — a dark deep-space base plus three soft directional colour clouds (magenta/teal/blue) via `Normalize(WorldPosition)` → `Dot` against fixed directions → `Saturate` → `Power` → tinted `Multiply`, summed into Emissive; (3) placed a `Nebula_Dome` sphere (scale 600 ≈ r30000) just outside the starfield dome (≈r20000). All scripted in `tools/_make_nebula.py`.

**Verified (PIE):** [S] /tmp/nebula1.png — rich magenta/purple nebula fills the scene behind the stars, with the blue Insurgent fighter front-and-centre; far cry from the prior flat black.

**Process note:** calling `EditorLoadingAndSavingUtils.load_map(...)` over the **live MCP path crashes the editor** (same `TaskGraph.cpp:689` RecursionGuard family as the Interchange import — the map load does a task-graph wait that re-enters MCP's worker thread). Workaround: pass the map as a **launch argument** (`UnrealEditor <uproject> /Game/Maps/VSlice_Arena`) so it opens on the game thread, then operate on the already-loaded level via MCP (no `load_map` call).

**Follow-ups remaining (M13):** explosion screenshot, HUD font styling. (Commit: Content/Maps/VSlice_Arena.umap + Content/Materials/{M_Starfield,M_Nebula} + tools/_make_nebula.py + docs.)

## 2026-06-21 — ✅ Earth planet backdrop (user's Fab model, verified in PIE)

**Goal (user):** add an Earth the player can fly around. The user added an Earth to their **Fab** library and dropped the `earth.blend` into `_assets_dl/`.

**Pipeline (`tools/_import_earth.py`, two stages):** legendary can't reach Fab assets, and UE can't import `.blend`, so — Stage A: installed the `bpy` pip module (Blender headless, no full install) and exported the Earth mesh to **OBJ** + saved its packed **8K equirectangular texture** to PNG. Stage B (editor via MCP): imported the OBJ as a static mesh, the PNG as `T_Earth`, built `M_Earth` (albedo + faint self-fill emissive so the night side isn't black), and placed an `Earth_Backdrop` StaticMeshActor — scale 260 (mesh radius ~36uu → ~9360uu planet), parked at (13000,12000,-1500) within the nebula dome, collision off. Level saved.

**Gotchas (saved to memory [[fbx-import-skeletal-mesh]]):** (1) the Fab mesh sits under empties, so **every FBX export imported as a SkeletalMesh** regardless of `FbxImportUI.import_as_skeletal=False` — switching to **OBJ** (no skeleton concept) imports static every time. (2) Deleting a stray skeletal mesh needs dependency order (PhysicsAsset → mesh → Skeleton) or `replace_existing` silently reuses the skeletal slot.

**Verified (PIE):** [S] /tmp/earth_pie2.png — the detailed 8K Earth fills the view as a planet (continents/clouds/oceans, correct equirectangular mapping via the mesh's own UVs), Insurgent in front, enemy engaging, nebula + stars behind.

**Note:** `T_Earth.uasset` is ~58 MB (8K source). Fine for now; could be dropped to 4K if repo size matters. (Commit: Content/Art/{Meshes/Earth, Textures/T_Earth, Materials/M_Earth} + Content/Maps/VSlice_Arena.umap + tools/_import_earth.py + docs.)

## 2026-06-21 — ⚠️ HUD custom font (Orbitron): attempted, reverted (UMG/Python limitation)

Tried giving the bridge HUD a sci-fi font (Orbitron, OFL). Imported the TTF, but applying it to UMG TextBlocks failed: an `AssetImportTask` produces a **FontFace**, and pointing `Font.FontObject` at a FontFace renders **tofu/garbled glyphs** — UMG needs a `UFont` composite, which **can't be assembled in Python** (`FontData`/`Typeface` structs aren't exposed). The offline `TrueTypeFontFactory` only imports by **system-font name**, not a file path. Variable-font instancing (`fonttools`) and clearing `TypefaceFontName` didn't change the FontFace rendering. **Reverted** the HUD to stock (git-checkout `WBP_BridgeHUD`, deleted the font assets) and restarted the editor — verified the full scene renders clean (`/tmp/final_scene.png`: normal HUD + Insurgent + Imperial + nebula + Earth). Reusable details saved to memory [[umg-custom-font-needs-ufont]]. To pursue later: install the TTF system-wide for the offline factory, or wire the FontFace into a UFont by hand in the editor UI, then assign via MCP. Default-font styling (size/colour/letter-spacing) remains adjustable via `WidgetService` if a lighter restyle is wanted.

## 2026-06-21 — ✅ M14 (start): camera shake (trauma model), verified in PIE

**What:** game-feel camera shake driven by a self-contained **trauma** model on `ASpaceship` — no engine camera-shake classes or extra module deps. `AddCameraTrauma(float)` accumulates trauma (0..1, clamped); `Tick` offsets the follow camera by random pitch/yaw/roll scaled by **trauma²** (a punchy, fast-settling curve), decaying at `TraumaDecayPerSec` (1.6) back to neutral. Tunables exposed under `Ship|Feel`.

**Triggers:** (1) `UHealthComponent` gained an `OnDamaged` delegate (broadcast per damaging hit); the ship binds it → trauma ∝ effective damage (`HitTraumaPerDamage`). (2) `UWeaponComponent::FireBeam` adds a recoil kick (`FireTrauma` 0.28) on the firing ship — covers both keyboard- and console-button fire. (3) `ABridgePlayerController::HandleEnemyDeath` adds a distance-scaled thump (full ≤2000uu, 0 by 12000uu) when a hostile explodes.

**Verified (PIE):** numeric proof rather than a screenshot (shake is motion). After `AddCameraTrauma(1.0)`: same-frame cam rotation 0/0/0 (Tick not yet run) → mid-decay `pitch 0.978 / yaw -0.615 / roll 0.269` → later `-0.115 / 0.903 / 0.296` (per-frame jitter, decaying). Weapon fire returned `fired:True` with a small recoil offset; editor stable (no crash). Damage path is the same proven `OnDamaged → AddCameraTrauma` mechanism.

**Notes:** single-enemy arena means the *explosion* thump coincides with the victory pause (may freeze) — reads clearly with multiple hostiles. Next M14: SFX/audio (asset-sourcing needed) + hit/impact FX. (Commit: Source/Components/{HealthComponent,WeaponComponent}.* + Source/Ships/Spaceship.* + Source/Core/BridgePlayerController.cpp + docs.)

## 2026-06-21 — ✅ M14 (audio): combat SFX wired (builds; in-PIE playback deferred)

**Assets:** Kenney **Sci-Fi Sounds** pack (CC0). Downloaded to gitignored `_assets_dl/audio`, converted four OGGs → mono 16-bit WAV via `ffmpeg`, imported as SoundWaves into `/Game/Audio`: `S_BeamFire` (laserLarge), `S_EnemyFire` (laserSmall), `S_Hit` (impactMetal), `S_Explosion` (explosionCrunch). Import note: audio import via MCP worked, but `EditorAssetLibrary.load_asset` returned None right after (registry not yet scanned) — `unreal.load_asset("/Game/Audio/X.X")` + an `AssetRegistry` scan confirmed all four as valid `SoundWave`.

**Wiring (C++, `FObjectFinder` in constructors + `UGameplayStatics`):** `UWeaponComponent::FireBeam` → `PlaySound2D(S_BeamFire)` (player beam, both fire paths); `ASpaceship::HandleDamaged` → `PlaySound2D(S_Hit)` (player takes damage); `AExplosionFx::Activate` → `PlaySoundAtLocation(S_Explosion)` (enemy death, spatialized); `AEnemyShip::FireAtPlayer` → `PlaySoundAtLocation(S_EnemyFire)` (enemy beam, spatialized).

**Verified:** compiles clean (Build.sh Succeeded); the four SoundWaves load + are valid; `FObjectFinder` paths match the asset paths. **In-PIE audio playback NOT yet confirmed** — the user was gaming on the same machine, so I avoided launching PIE (it would output sound + steal window focus). Confirm playback in a PIE session when free. (Commit: Source/Components/WeaponComponent.* + Source/Ships/{Spaceship,EnemyShip}.* + Source/FX/ExplosionFx.* + Content/Audio/* + docs.)

## 2026-06-21 — ✅ M14 (impact FX): beam-hit flashes (builds; PIE-deferred)

Added a small `AExplosionFx` flash at the point each beam lands — cyan where the player's beam hits the enemy (`WeaponComponent::FireBeam`), orange where an enemy beam hits the player (`AEnemyShip::FireAtPlayer`). Reuses the existing expand/collapse flash at small radius (~220–260uu) + short life (~0.2s). `AExplosionFx::Activate`/`Spawn` gained a `bPlaySound` flag (default true) so these impact flashes stay silent — only the death explosion booms. Builds clean; visual confirmation deferred to a PIE pass (machine in use for gaming). (Commit: Source/FX/ExplosionFx.* + Source/Components/WeaponComponent.cpp + Source/Ships/EnemyShip.cpp + docs.)

## 2026-06-21 — ✅ M14 (death FX): multi-burst enemy explosion (builds; PIE-deferred)

`AEnemyShip::HandleDeath` now spawns the main boom (with SFX) plus four smaller silent flashes at random offsets (`FMath::VRand` × 200–650uu, radius 240–460, life 0.3–0.5s) for a richer, scattered death burst. Pure C++, reuses `AExplosionFx` (the new `bPlaySound=false` flag keeps the extra pops silent). Builds clean; visual check deferred to the same pending PIE pass.

**Pending verification backlog (one PIE pass clears all):** camera-shake feel, the 4 combat SFX actually playing, beam-impact flashes, and this multi-burst death. All build clean and reuse proven systems; none seen/heard in PIE yet (machine in use for gaming).

## 2026-06-25 — ✅ M14 (audio polish + verification pass): per-shot pitch jitter; PIE backlog cleared

**Polish (C++, editor-free):** added subtle per-event pitch variation to all combat SFX so repeated sounds don't read machine-stamped — beam fire `0.94–1.06×` (`UWeaponComponent::FireBeam`), enemy fire `0.94–1.06×` (`AEnemyShip::FireAtPlayer`), player hit `0.92–1.08×` (`ASpaceship::HandleDamaged`), explosion boom `0.9–1.1×` (`AExplosionFx::Activate`). Just the existing `PlaySound2D`/`PlaySoundAtLocation` calls with the pitch arg + `FMath::FRandRange`. Builds clean (Result: Succeeded).

**Verification pass (PIE, gaming constraint lifted):** drove the deferred backlog programmatically in the arena —
- **Beam fire:** `FireBeam()` → `FIRED:True`, charge `1.0→0.0`; beam SFX (`PlaySound2D`) + cyan impact flash + recoil trauma all fire.
- **Camera shake:** after `AddCameraTrauma(1.0)`, next-frame cam relative-rotation sampled non-zero (`pitch 0.02 / yaw -0.52 / roll -1.77`), decaying — trauma model live after the audio changes.
- **Multi-burst death:** lethal damage to the enemy → `0` enemies, **6** `AExplosionFx` actors in-world (main boom + scatter), boom SFX + distance-scaled explosion trauma fired.
- All `PlaySound*` calls executed through the editor audio device without error; the four SoundWaves are valid. (Audible playback now confirmable by ear at the machine.)

Editor stable throughout; ended PIE cleanly. **M14 backlog cleared.** (Commit: Source/Components/WeaponComponent.cpp + Source/Ships/{Spaceship,EnemyShip}.cpp + Source/FX/ExplosionFx.cpp + docs.)

## 2026-06-25 — ✅ M14 (ambient audio): engine hum + low-hull alarm

Two looping `UAudioComponent`s on `ASpaceship` (M14 nice-to-haves). **Engine hum** — Kenney `spaceEngineLow` (CC0) → mono WAV `S_EngineHum` (looping); auto-plays at idle volume, and `UpdateAmbientAudio()` (called each Tick) rides its volume `0.12→0.5` and pitch `0.9→1.25×` on `MovementComp->GetThrottle()` so the drive spools up under power. **Low-hull alarm** — a synthesized two-tone klaxon `S_Alarm` (ffmpeg sine 780/560 Hz, 0.44s loop); edge-triggered `Play()`/`Stop()` when hull crosses `LowHullFraction` (0.3). Both built clean. Audible check folded into the next PIE pass. (Commit: Source/Ships/Spaceship.{h,cpp} + Content/Audio/{S_EngineHum,S_Alarm} + tools + docs.)

## 2026-06-25 — ✅ M16 (LAN web-stations): browser bridge consoles over the network

The main machine is now **server + viewscreen**; other devices on the LAN drive the bridge stations from a browser. New `UStationServerSubsystem : UWorldSubsystem` (Game/PIE worlds only) stands up an in-process HTTP server on **port 8080** in `OnWorldBeginPlay` and tears it down (unbinds routes + stops listeners) in `Deinitialize`, so repeated PIE runs rebind cleanly. Module deps added: `HTTPServer`, `Sockets`, `Networking`.

**Routes** (handlers run on the game thread, so they touch the ship's components directly):
- `GET /stations` — touch-friendly station picker (root `/` is rejected by `FHttpPath::IsValidPath`, so the landing lives at `/stations`).
- `GET /helm` `/weapons` `/engineering` — embedded dependency-free HTML/JS consoles; JS polls `/api/state` every 250ms.
- `GET /api/state` — hand-built JSON snapshot (speed/throttle/maxSpeed, charge/target/range/in-range, per-system power + reactor load, hull).
- `GET /api/helm?throttle=&turn=`, `/api/weapons?action=fire|cycle`, `/api/power?system=&delta=` — commands. (GET, not POST: UE's HTTP server 411s empty browser `fetch` POSTs that omit `Content-Length`.)

Ship resolved via `GetPlayerPawn(world,0)` → `ASpaceship`, then its existing component setters (`SetThrottle/SetTurn`, `CycleTarget/FireBeam`, `AdjustSystemPower`) — same entry points the local 1/2/3 console uses, which stays as a single-machine fallback (all input is event-driven, so web + local don't fight per-tick). LAN IP discovered via `ISocketSubsystem::GetLocalAdapterAddresses` (skips `127.*`) and logged at startup.

**PIE verification:** server came up clean (`http://192.168.178.71:8080/stations` logged). Over curl: `throttle=0.7` → throttle 0→0.7, speed climbing to 1230; `power?system=0&delta=0.1` → engine power 1.0→1.1, maxSpeed 1800→1980; `weapons?action=cycle` → target `EnemyShip_0` @ 6389uu; `action=fire` → charge 1.0→0.204. All endpoints 200; pages serve correct titles/links. Ended PIE cleanly. (Commit: Source/Net/StationServerSubsystem.{h,cpp} + SpaceGame.Build.cs + docs.)

## 2026-06-25 — ✅ M16 fixes (real-browser verification: Firefox over LAN)

Drove the stations from an actual browser (Playwright MCP, switched to Firefox) and fixed three bugs the curl pass had missed:
- **Only Helm rendered.** Exact-matched HTTP routes don't carry the path in `Request.RelativePath`, so the shared page handler always fell through to Helm. Now each route binds a station-id payload (`/helm`=0,`/weapons`=1,`/engineering`=2) and switches on it. (Commit `f2ac8fb`.)
- **Loopback-only bind.** The listener bound `127.0.0.1`, so LAN devices couldn't reach it — the whole point. Set a per-port `HTTPServer` config override (`BindAddress=any`, `ReuseAddressAndPort=true`) via `GConfig` before `GetHttpRouter`, so it binds `0.0.0.0`. Confirmed reachable + driveable from Firefox at `http://192.168.178.71:8080`. (Commit `8262044`.)
- **Dead port after a PIE restart.** `Deinitialize` called the *global* `StopAllListeners()`, which stopped other modules' listeners (e.g. the editor's `:3000`) and left `:8080` unable to rebind (TIME_WAIT). Now we only unbind our own routes and leave the listener for the next PIE to reclaim; address-reuse covers any forced rebind. Verified `:8080` stays reachable across a PIE stop/start with no bind errors. (Commit `8262044`.)

Browser-verified end-to-end over the LAN IP: distinct consoles, 250ms live polling (Weapons cycle locked `EnemyShip_0` with live range), Helm throttle slider drove `throttle→1.0`, world ticking (speed integrated 10→1230→1440). `.playwright-mcp/` scratch gitignored.

## 2026-06-25 — ✅ M16 game-state controls + per-station status footer

Surfaced the encounter outcome to the whole crew and added New Game / Restart controls over the LAN.
- **`EGamePhase {Playing,Victory,Defeat}`** (StationTypes.h) lives on `ASpaceGameMode` (`GetPhase`/`SetPhase`); the bridge controller sets it on the existing defeat (player `OnDeath`) and victory (last hostile down) paths. `ASpaceGameMode::RestartEncounter()` unpauses and `OpenLevel`s the current map — a fresh encounter that respawns ship + hostiles and recreates the world subsystems (the LAN server rebinds cleanly per the earlier lifecycle fix). New Game and Restart both map to it for this single-encounter slice.
- **Server:** `/api/state` JSON gains `"phase"`; new `GET /api/game?action=restart|new` calls `RestartEncounter()`. Every station page now has a fixed **status footer** (green ● ENCOUNTER LIVE / ✔ VICTORY / ✖ DEFEAT) with a guarded `↻ RESTART`. The `/stations` landing page shows the same live phase banner plus **NEW GAME / RESTART** buttons (both `confirm()`-guarded so one device doesn't wipe the run for everyone).

**Verified (Firefox over the LAN IP + curl):** `phase` tracked playing→defeat on player death and reset to playing on restart; a browser-issued `/api/game?action=restart` reloaded the level (hull 100, server rebound); screenshots confirmed the ENCOUNTER LIVE banner/controls on `/stations` and the red ✖ DEFEAT footer on the Engineering console. (Commit: Core/{StationTypes.h,SpaceGameMode.{h,cpp},BridgePlayerController.cpp} + Net/StationServerSubsystem.{h,cpp} + docs.)

## 2026-06-26 — ✅ M16 polish: fire-readiness feedback + editor background-throttle fix

Chasing "fire beam over the web doesn't trigger the animations":
- **Root cause #1 (the big one): editor background CPU throttle.** By default the UE editor drops to a near-zero framerate (and stops rendering) when it isn't the foreground window (`UEditorPerformanceSettings::bThrottleCPUWhenNotForeground`, "Use Less CPU when in Background"). Driving the ship from a phone leaves the editor unfocused, so PIE crawls and the 0.2s beam FX barely renders (this also explains the earlier "frozen world"). Disabled it in `EditorSettings.ini` (`[/Script/UnrealEd.EditorPerformanceSettings] bThrottleCPUWhenNotForeground=False`). Verified live: with the editor in the background, the enemy AI kept closing 23872→16463uu at full speed over a couple seconds. (Per-user editor pref, not in the repo — re-apply via Editor Preferences → General → Performance, or the `frame-rate` VibeUE skill.)
- **Root cause #2: no feedback when a shot is blocked.** `UWeaponComponent::FireBeam()` silently returns false when out of range / not charged / no target, and the web Weapons console gave no hint, so it looked dead (enemies spawn beyond the 15000uu beam range, so the first fire is usually blocked). The FIRE button now reflects readiness: green **● FIRE BEAM** only when a target is locked, in range, and fully charged; otherwise dimmed **OUT OF RANGE / CHARGING n% / NO TARGET**. (Closing to range is the Helm's job — the intended cross-station loop.) Confirmed both states in Firefox; an in-range web fire spawns the BeamFx + impact flash and empties the charge.

(Commit: Net/StationServerSubsystem.cpp + docs. The throttle setting is a machine-local editor pref.)

## 2026-06-26 — ✅ M17.1 Helm tactical map + heading

Gave Helm a real top-down tactical map instead of bare speed numbers.
- **Server:** `/api/state` JSON gains `heading` (player yaw), `radarRange` (20000uu, mirrors `URadarWidget::RadarRangeUU`), `px`/`py` (player world XY), and a `contacts` array (`{x,y,color}`) built by iterating `TObjectIterator<URadarContactComponent>` in the play world (skips the player, hostile blip colour → `#rrggbb`).
- **Helm page:** a dependency-free `<canvas>` map (north-up, player-centred) mirroring `RadarWidget`'s `WorldToScreen` (`screen = (Δy·scale, −Δx·scale)`, clamped to the outer ring), drawing range rings, cross axes, an enemy blip per contact, and a cyan player heading arrow at centre — plus a numeric **HEADING** readout. Throttle/turn controls kept below.

**Verified (Firefox over the LAN IP):** map drew the hostile blip above the player at heading 0°; after a turn+burn the readout showed 151°, the heading arrow rotated to match, and the blip repositioned as the ship moved — confirming the world→screen mapping. (Commit: Net/StationServerSubsystem.cpp + docs.)

## 2026-06-26 — ✅ M17.2 Weapons torpedoes (limited ammo, bypass shields)

Gave Weapons a second weapon type to complement the instant beam.
- **`ATorpedoProjectile`** (FX/) — a slow glowing round (orange `M_GlowOrange`) that homes on the locked target each tick and, on arrival (or lifetime expiry), detonates: applies its payload to the target and spawns an orange `AExplosionFx`. Travels visibly so the crew watches it close in, unlike the hitscan beam.
- **`UHealthComponent::ApplyDamage(Amount, bBypassShield)`** — new bypass path that ignores both shield-power mitigation *and* the shield pool, landing the full payload straight on hull (torpedoes). Existing beam callers use the default `false`. Also added **`RepairHull(Amount)`** (caps at MaxHull, won't revive a dead hull) for Engineering next.
- **`UTorpedoLauncherComponent`** on `ASpaceship` (sibling of the beam `UWeaponComponent`, shares its locked target): `MaxAmmo=4`, `ReloadTime=4s`, `TorpedoDamage=60`, `TorpedoSpeed=5000`. `Fire()` spends a round, starts the reload, launches the projectile. `ASpaceship::GetTorpedoComp()` added.
- **Server/UI:** `/api/state` gains `ammo`/`maxAmmo`/`torpedoReady`/`torpedoReload`; `/api/weapons?action=torpedo` fires it. The Weapons page shows a **TORPEDOES n/m** readout and a **FIRE TORPEDO** button with its own readiness (dimmed `NO TORPEDOES` / `NO TARGET` / `RELOADING n%`, green `◎ FIRE TORPEDO` when armed).

**Verified (MCP actor inspection + Firefox over the LAN IP):** with `EnemyShip_0` locked (100 hull / 50 shield), one web-fired torpedo flew across and dropped it to **40 hull with shield still 50** — the full 60 bypassed shields, confirming the difference from the beam (which hits shields first). Ammo decremented 4→3 and the reload ran; the Weapons console showed `TORPEDOES 3/4` + `RELOADING 92%` mid-reload and both buttons green when armed. (Commit: Components/{HealthComponent,TorpedoLauncherComponent}.{h,cpp} + FX/TorpedoProjectile.{h,cpp} + Ships/Spaceship.{h,cpp} + Net/StationServerSubsystem.cpp + docs.)

## 2026-06-26 — ✅ M17.3 Engineering repair minigame (timing sweep)

Gave Engineering a way to actively repair hull, as a skill-timed minigame.
- **Server:** new `GET /api/engineering?action=repair` restores a fixed chunk (+8 hull) via `UHealthComponent::RepairHull` (added in M17.2; caps at MaxHull, never revives a dead hull). A **0.25s sim-time guard** (`LastRepairTime`) rate-limits accepted welds so a fast/scripted client can't snap the ship to full in one burst — repairs stay paced like the on-screen sweep.
- **Engineering page:** a colour-coded **HULL INTEGRITY** bar (green→amber→red as it drops) + numeric readout, and a **DAMAGE CONTROL** timing-sweep minigame — a marker ping-pongs across a bar (`requestAnimationFrame`, ~1.2s/sweep) and the **WELD** button only POSTs a repair (with a green flash) when the marker is inside the central green zone (else a red miss flash). The power -/+ controls stay.

**Verified (MCP actor inspection + Firefox over the LAN IP):** from a clean hull of 20 (enemies removed to isolate), three spaced welds restored exactly **+8 each (20→28→36→44)**; a burst of **20 rapid welds added only +8** — the rate-limit rejected the rest (not +160), proving it can't be spammed to full. The console showed the amber hull bar at 52% and the animated sweep (marker, green zone, WELD). (Commit: Net/StationServerSubsystem.{h,cpp} + docs.)

## 2026-06-26 — ✅ M17.4 Science station (scan enemy hull/shield)

Added a brand-new fourth bridge station — web-only, decoupled from the `EStation` enum.
- **`UScienceComponent`** on `ASpaceship`: cycles a scan target through the radar contacts (independent of the Weapons lock), and a **timed scan** (`ScanDuration=2.5s`). While scanning, values stay hidden; on completion the target is "scanned" and its hull/shield/max are read **live** off the target's `UHealthComponent` (so the readout tracks ongoing damage). Switching targets clears the scan. `ASpaceship::GetScienceComp()` added.
- **Server:** `/api/state` gains a science block (`sciTarget`/`sciProgress`/`sciScanning`/`sciScanned` + `sciHull`/`sciMaxHull`/`sciShield`/`sciMaxShield`, `−1` until revealed); new `/api/science?action=cycle|scan` (`HandleScience`); new `SciencePage()` wired as StationId 3 (`/science`, `BindStation`), plus a **SCIENCE** link on the `/stations` index. The page shows the contact, a scan-progress bar + status, CYCLE/SCAN buttons (SCAN green when ready), and — once scanned — live HULL + SHIELD bars.

**Verified (MCP actor inspection + Firefox over the LAN IP):** cycling locked `EnemyShip_0`; mid-scan the values stayed hidden (`sciHull −1`, progress 0.4) and the bar filled; on completion it revealed **100/100 hull, 50/50 shield**. Dealing 40 damage to the enemy (shields-first) immediately moved the science readout to **shield 10/50** with hull unchanged — confirming the live read. The `/stations` index now lists all four consoles. (Commit: Components/ScienceComponent.{h,cpp} + Ships/Spaceship.{h,cpp} + Net/StationServerSubsystem.{h,cpp} + docs.)

---

# M18 — Campaign shell (menus, save game, more ships, story comms)

Growing the vertical slice into a small campaign: a host menu + in-game pause, a save system,
ship variety, and a story told as comms on the Science station. Built one milestone per commit.

## 2026-06-26 — ✅ M18.1 GameInstance + SaveGame (campaign persistence)

The backbone for the menu's Continue and the pause menu's Save.
- **`USpaceGameInstance`** (`Core/SpaceGameInstance.{h,cpp}`) survives level loads and holds the live
  campaign state — `MissionIndex` + `PlayerShip` (`EPlayerShipType {Interceptor,Cruiser}`, added to
  `StationTypes.h`). API: `SaveCampaign`/`LoadCampaign`/`HasSave`/`ResetCampaign`/`AdvanceMission`
  (clamped to `GetMissionCount()`=3) + getters/setters. Registered as `GameInstanceClass` in
  `DefaultEngine.ini`.
- **`USpaceSaveGame`** (`Core/SpaceSaveGame.h`) — the on-disk record (slot "campaign"), persisted via
  `UGameplayStatics::SaveGameToSlot/LoadGameFromSlot/DoesSaveGameExist`. Progress-only (mission +
  ship); encounters start fresh, so there's no mid-fight snapshot.

**Verified (MCP, in PIE):** the running game instance is `SpaceGameInstance`; set mission 2 + Cruiser →
`SaveCampaign` (true) → `ResetCampaign` (back to 0/Interceptor) → `LoadCampaign` restored **mission 2 /
Cruiser** off disk; `AdvanceMission` clamped at the last index (2). (Commit: Core/SpaceGameInstance.{h,cpp}
+ Core/SpaceSaveGame.h + Core/StationTypes.h + Config/DefaultEngine.ini + docs.)

## 2026-06-26 — ✅ M18.2 Main Menu map + front-end

A proper front door on the host viewscreen.
- **`UMainMenuWidget`** (`Core/MainMenuWidget.{h,cpp}`) — built entirely in C++ via `WidgetTree`
  (no WBP): title + **NEW GAME / CONTINUE / QUIT**. New Game `ResetCampaign()`s and opens the
  encounter; Continue `LoadCampaign()`s and resumes (button **greyed when `!HasSave()`**); Quit exits.
- **`AMenuGameMode` + `AMenuPlayerController`** (`Core/`) — the controller builds the menu, adds it to
  the viewport, and switches to UI-only input with the cursor shown; no pawn is spawned.
- **`MainMenu` map** (`Content/Maps/MainMenu.umap`, created via MCP) with its World Settings GameMode
  override = `AMenuGameMode`; set as `EditorStartupMap` + `GameDefaultMap` in `DefaultEngine.ini`.

**Verified (PIE + OS screenshot + MCP):** the menu renders (SPACEGAME / BRIDGE SIMULATOR + 3 buttons);
**Continue is bright with a save present and greys out after deleting the save** (HasSave gating both
ways); simulating New Game (`OpenLevel`) transitioned MainMenu → `VSlice_Arena` with `Spaceship_0`
possessed and unpaused. (Commit: Core/{MainMenuWidget,MenuGameMode,MenuPlayerController}.{h,cpp} +
Content/Maps/MainMenu.umap + Config/DefaultEngine.ini + docs.)

## 2026-06-26 — ✅ M18.3 In-game pause overlay (ESC)

ESC now brings up a pause overlay over the live encounter.
- **`UPauseMenuWidget`** (`Core/PauseMenuWidget.{h,cpp}`, C++ `WidgetTree`) — a dimming panel with
  **RESUME / SAVE / RESTART / MAIN MENU / QUIT** and a transient *CAMPAIGN SAVED* toast. Buttons call
  back into the owning `ABridgePlayerController`.
- **`ABridgePlayerController`** — `EKeys::Escape` (bExecuteWhenPaused) toggles the overlay:
  `ShowPauseMenu`/`HidePauseMenu` pause/unpause + swap input mode (UI-only ↔ game-and-UI). Actions:
  Save → `GameInstance->SaveCampaign()` (+ toast); Restart → `RestartEncounter()`; Main Menu →
  `OpenLevel(MainMenu)`; Quit → `QuitGame`. Guarded so it never fights the end-of-encounter screen.
  `TogglePause` is BlueprintCallable so it's testable (PIE eats a real ESC to stop play).

**Verified (PIE + OS screenshot + MCP):** driving `TogglePause` paused the game and the **PAUSED**
overlay (5 buttons) rendered over the dimmed encounter; toggling again removed it and the live scene
(ship + HUD, HULL 74%) resumed. (Commit: Core/PauseMenuWidget.{h,cpp} + Core/BridgePlayerController.{h,cpp}
+ docs.)

## 2026-06-26 — ✅ M18.4 Enemy types + mission-driven spawning

Encounters became data-driven missions with a varied fleet, plus a real campaign loop.
- **Enemy archetypes** — `EEnemyType {Gunship,Scout,Cruiser}` on `AEnemyShip`, applied in `BeginPlay`
  via `ApplyTypePreset()` (runtime `StaticLoadObject` for mesh/material): Scout = small/fast/fragile
  Insurgent hull (cyan, 50/20), Gunship = the baseline Imperial (orange, 100/50), Cruiser = big/slow/
  tanky Imperial (red, 220/110, hits hard). Reuses the two existing meshes — no new art.
  `UHealthComponent::ResetPools()` re-fills the pools after a preset changes the Max values.
- **`UMissionSubsystem`** (`Core/MissionSubsystem.{h,cpp}`, world subsystem) — on world begin play reads
  `MissionIndex` from the game instance, **clears any level-placed hostiles**, and **spawns the mission's
  fleet** (deferred-spawn with the archetype) in a fan ahead of the player, then re-binds the controller's
  win condition (subsystems begin play after the PC). Campaign = a 3-mission in-code table (`FMissionDef`:
  name + enemy list + comms beats) of escalating fleets.
- **Campaign loop** — on victory the controller advances + saves the campaign and shows a C++
  `UOutcomeMenuWidget` (replacing the static WBP end screen): **SECTOR CLEARED / NEXT MISSION** (reloads
  the encounter at the new index) or **CAMPAIGN COMPLETE** on the last mission; defeat shows **RETRY /
  MAIN MENU**. Shared C++-UMG button helpers moved to `Core/MenuUI.h` (fixes a unity-build name clash).

**Verified (MCP + OS screenshot):** mission 0 spawned **Scout(50/20) + Gunship(100/50)**; setting the
index to 1 + reloading spawned **Gunship + Gunship + Cruiser(220/110)** — escalation + distinct presets
confirmed, placed enemies cleared. Clearing the fleet advanced the campaign (0→1, saved) and showed the
green **SECTOR CLEARED / NEXT MISSION** overlay (player survived at 92% hull); a player death showed the
red **DEFEAT / RETRY** overlay. (Commit: Ships/EnemyShip.{h,cpp} + Components/HealthComponent.{h,cpp} +
Core/{MissionSubsystem,OutcomeMenuWidget,MenuUI}.* + Core/BridgePlayerController.{h,cpp} + the menu
widgets + docs.)

## 2026-06-26 — ✅ M18.5 Player-ship choice

The player picks a ship at New Game; it persists in the save and shapes how the ship plays.
- **`ASpaceship::ApplyShipPreset()`** (run before `Super::BeginPlay`, so components init pools/ammo from
  the variant) reads `EPlayerShipType` off the game instance and applies a preset: **Interceptor** =
  fast/agile/light (MaxSpeed 2100, turn 75, hull 80, beam 20, 3 torps, scale 0.6, Insurgent skin);
  **Cruiser** = slow/tanky/heavy-hitting (MaxSpeed 1300, turn 42, hull 160, beam 34, 6 torps, scale 0.95,
  `M_PlayerHull` skin).
- **Main menu ship-select** — NEW GAME now opens a **SELECT YOUR SHIP** panel (INTERCEPTOR / CRUISER /
  BACK); the pick sets `GameInstance->PlayerShip`, resets the campaign, and opens mission 0. Shared
  C++-UMG button helper promoted to `MenuUI::AddFlatButton` (a second unity-build collision fix).

**Verified (MCP, in PIE):** with the game instance set to **Cruiser** then **Interceptor** and the arena
reloaded, the possessed ship's live stats matched each preset exactly (Cruiser 1300/turn42/hull160/beam34/
scale0.95; Interceptor 2100/turn75/hull80/beam20/scale0.6) — confirming the choice flows GameInstance →
ship. The ship-select panel reuses the same WidgetTree button pattern as the four already-screenshotted
menus. (Commit: Ships/Spaceship.{h,cpp} + Core/MainMenuWidget.{h,cpp} + Core/MenuUI.h + the refactored
overlay widgets + docs.)

## 2026-06-26 — ✅ M18.6 Story comms on the Science station

The campaign now tells its story through transmissions on the Science console.
- **`UMissionSubsystem`** drives each mission's comms script (`FCommsBeat`: sender + text + an
  `AtSeconds` time trigger or an `OnKill` count trigger). A repeating 0.25s timer (paused with the game)
  fires time-beats; binding each spawned hostile's `OnDeath` advances a kill count that fires kill-beats.
  Fired beats accumulate in a `CommsLog` (`FCommsMessage`), exposed via `GetComms()`.
- **Server** — `/api/state` gains `mission` (name) + a `comms` array (`{sender,text}`, JSON-escaped),
  read from the world's mission subsystem. `SciencePage` gains a **MISSION** readout and a scrolling
  **COMMS** log (sender + text, newest highlighted, auto-scrolled).

**Verified (MCP + Firefox over the LAN IP):** on mission 0 the timed briefing fired (~1s: *COMMAND —
"two unknowns just dropped in…"*) and killing one hostile fired the kill-beat (*SCOUT-7 — "the scout is
down…"*); both appeared in `/api/state.comms` and rendered on the Science **COMMS** panel (newest in
green, mission "First Contact"). (Commit: Core/MissionSubsystem.{h,cpp} + Net/StationServerSubsystem.cpp
+ docs.)

**M18 complete** — the vertical slice is now a small campaign: main menu + ship select, in-game pause +
save/load, three escalating missions of varied enemy ships, and a story told over the Science comms.

---

## 2026-06-27 — 🐞 Bug fixes: reactor budget enforcement + web NEW GAME

Two player-reported issues from the M18 campaign shell.

- **Engineering power had no downside** — every system could be cranked to 200% at once because
  `ReactorBudget` was only a display readout. `UPowerComponent::SetSystemPower` now treats the budget
  as a *hard cap on the total*: a system can draw at most the headroom the others leave
  (`min(MaxPerSystem, ReactorBudget − OthersTotal)`), so power is zero-sum — boosting one system
  requires taking it from another. `AdjustSystemPower` routes through this, so both the web Engineering
  page and the local console obey it. The Engineering page tints the REACTOR LOAD readout amber when
  it's at capacity to surface the trade-off.
- **Web NEW GAME did nothing meaningful** — it was identical to RESTART (both just reloaded the current
  mission). `HandleGame` now distinguishes them: `action=new` calls `ResetCampaign()` + `SaveCampaign()`
  (mission → 0, persisted) before reloading; `action=restart` retries the current mission unchanged.

**Verified (PIE + LAN web API + MCP):** at full reactor (load 3.0/3.0) `engine+0.5` was a no-op; after
`weapons−0.5` (load 2.5) the same `engine+0.5` lifted engine to 1.5 (load back to 3.0) — load never
exceeded the budget. With the campaign advanced to mission 2, `action=restart` reloaded "Last Stand"
(progress kept) while `action=new` reset to "First Contact" with `GetMissionIndex()==0`.
(Commit: Components/PowerComponent.{h,cpp} + Net/StationServerSubsystem.cpp + docs.)

---

## 2026-06-27 — ⚙️ Engage grace period + forward firing arc

Two combat-feel tweaks from playtest feedback.

- **Enemies hold fire briefly at mission start** — `AEnemyShip` gained an `EngageDelay` (6 s, set in
  `BeginPlay` as a `GraceTimer`). During the grace the ship still closes in but won't shoot, giving the
  crew a beat to orient before incoming fire. The Tick fire-gate now also requires `GraceTimer <= 0`.
- **Phaser + torpedo require pointing at the target** — `UWeaponComponent::IsTargetInArc()` checks the
  yaw-plane angle between the bow and the line to the locked target against a `FireArcDeg` (70°, i.e.
  ±35° off the bow). `FireBeam()` and the torpedo's `Fire()`/`IsReady()` both reject a target outside
  the arc, so forward-mounted weapons can't shoot sideways/behind — you have to fly the ship onto the
  enemy. `/api/state` gains `inArc`; the Weapons console shows an **ON TARGET** readout and the FIRE
  buttons display **TURN TO TARGET** when the bow isn't on the enemy.

**Verified (PIE + log + MCP):** both enemies spawned at 09:44:07 and didn't fire until 09:44:14–15
(~7–8 s, grace + closing). With a target locked 35° off the bow (`inArc=false`), `FireBeam` and
`Torpedo.Fire` both returned false and dealt no damage / spent no ammo; widening the arc so the same
target fell inside flipped `inArc=true` and both fired (enemy shield 20→0, torpedo ammo 3→2). The arc
field surfaced on `/api/state` and the Weapons page. (Commit: Ships/EnemyShip.{h,cpp} +
Components/WeaponComponent.{h,cpp} + Components/TorpedoLauncherComponent.cpp + Net/StationServerSubsystem.cpp + docs.)

---

## 2026-06-27 — 🎯 Firing-arc indicators on the Helm radar

The helm operator can now see, on the tactical map, where each weapon can shoot — so they know which
way to fly the ship to bring a target into a firing solution.

- **Separate phaser / torpedo arcs** — the torpedo got its own `FireArcDeg` (110°, wider than the
  beam's 70°: it homes after launch, so the tube only needs the target roughly ahead). `UWeaponComponent`
  gained `IsTargetWithinArc(ArcDeg)` (generalising the old `IsTargetInArc`), which the torpedo uses with
  its own width for both `Fire()` and `IsReady()`.
- **Radar wedges** — `/api/state` now reports `phaserArc`, `torpedoArc` (degrees) and `inArcTorp`
  (alongside the existing `inArc`). The Helm canvas draws two translucent wedges centred on the heading:
  red-edged for the phaser, blue for the torpedo, each brightening when the locked target falls inside
  that weapon's arc. A small **FIRING ARCS** legend sits under the map. The Weapons console's torpedo
  button now keys off `inArcTorp`.

**Verified (PIE + headless-Firefox render of the Helm page):** with the target locked 35° off the bow,
`/api/state` reported `inArc=false` but `inArcTorp=true` (inside the 110° torpedo arc, outside the 70°
phaser arc); the rendered radar showed the narrow phaser wedge and wider torpedo wedge centred on the
bow arrow, the torpedo wedge highlighted and the target blip sitting in it but at the phaser-wedge edge.
(Commit: Components/WeaponComponent.{h,cpp} + Components/TorpedoLauncherComponent.{h,cpp} +
Net/StationServerSubsystem.cpp + docs.)

---

## 2026-06-27 — 📡 Enemy callsigns on Helm radar + Weapons target

Hostiles now go by readable radio callsigns instead of raw actor names like "EnemyShip_1".

- **Callsigns** — `AEnemyShip` carries a `Callsign` plus `MakeCallsign(type, ordinal)`, which draws from
  per-archetype name pools (Scout: WASP/HORNET/SHRIKE…, Gunship: VIPER/REAPER/JACKAL…, Cruiser:
  LEVIATHAN/TITAN/WARLORD…) suffixed by a per-type ordinal → e.g. `WASP-1`, `VIPER-1`. The mission
  spawner assigns them (tracking a per-type count) before `FinishSpawning`; `BeginPlay` defaults any
  unnamed ship.
- **Surfaced on the consoles** — `/api/state` now tags each radar contact with a `label`, and the
  beam/science target names resolve through the callsign (via a `DisplayName` helper). The Helm canvas
  draws the callsign beside each blip in the blip's colour; the Weapons console's **TARGET** row and
  the Science console's target show the callsign.

**Verified (PIE + headless-Firefox renders):** mission 0 fielded `WASP-1` (Scout) and `VIPER-1`
(Gunship); the Helm radar labelled both blips in matching colours, and the Weapons screen read
`TARGET: WASP-1`. (Commit: Ships/EnemyShip.{h,cpp} + Core/MissionSubsystem.cpp + Net/StationServerSubsystem.cpp + docs.)

---

## 2026-06-27 — 💰 M19.1 Reward economy (credits + XP / rank)

First slice of the progression loop (M19): missions now pay out, banked across the campaign.
- **Wallet on the campaign** — `USpaceGameInstance` gained `Credits` + `XP` (mirrored in `USpaceSaveGame`),
  with `AddReward`, `SpendCredits`, `GetCredits/GetXP`, and `GetRank()` (rank = 1 + XP/400). `ResetCampaign`
  zeroes the wallet; `SaveCampaign`/`LoadCampaign` round-trip it.
- **Per-kill payouts** — `AEnemyShip` carries `RewardCredits`/`RewardXP` set per archetype in
  `ApplyTypePreset` (Scout 40/15, Gunship 80/30, Cruiser 200/80). `ABridgePlayerController::HandleEnemyDeath`
  banks each kill to the GameInstance *before* the win/defeat guard, so even the killing blow pays out.
- **Surfaced** — the victory outcome subtitle shows the run haul + banked total + rank; `/api/state` gains
  `credits`/`xp`/`rank` for the consoles (the M19.3 drydock will spend them).

**Verified (PIE + MCP + web):** killing WASP-1 banked 40cr/15xp, VIPER-1 banked 80cr/30xp; a clean
double-kill reached `phase:victory` with the wallet at 240/90 and the mission advanced; a load round-trip
restored exactly 240cr/90xp/mission 1 from disk after dirtying the live values. (Commit:
Core/SpaceGameInstance.{h,cpp} + Core/SpaceSaveGame.h + Core/BridgePlayerController.{h,cpp} +
Ships/EnemyShip.{h,cpp} + Net/StationServerSubsystem.cpp + docs.)

---

## 2026-06-27 — 🛰️ M19.2 Station actor + docking

The progression loop gets a place: a friendly starbase you fly back to between fights.
- **`AStation`** (`World/Station.{h,cpp}`) — a passive landmark (Imperial hull scaled 3×, friendly cyan
  material) carrying a non-targetable `URadarContactComponent` (new `bTargetable` flag) and a `DockRange`.
  `UMissionSubsystem` spawns one ~5000 uu behind the player's start each encounter.
- **Docking on `ASpaceship`** — `CanDock()` (a station in range + nearly stopped + not already docked),
  `Dock()` / `Undock()`. Docking locks helm input (new `UShipMovementComponent::SetInputLocked`), makes
  the ship combat-safe (new `UHealthComponent::SetInvulnerable` → `ApplyDamage` no-ops), repairs hull/
  shield (`ResetPools`) and restocks torpedoes (new `Resupply`). Undock reverses it.
- **Non-targetable base** — weapon + science `CycleTarget` now skip `bTargetable == false` contacts, so
  you can't lock your own starbase.
- **Web** — `/api/dock?action=dock|undock`; `/api/state` gains `docked` / `canDock` / `stationRange`. The
  Helm page draws the STARBASE blip, a status row, and a DOCK/UNDOCK button (enabled only in range).

**Verified (PIE + MCP + headless Helm render):** the starbase spawned and showed as a friendly STARBASE
blip 5000 uu off; teleporting the ship within range flipped `canDock`; `Dock()` repaired hull 0→80,
locked input, and made damage a no-op; undock restored both; cycling weapon targets only ever locked
WASP-1/VIPER-1 (never STARBASE); the Helm page rendered the blip + "DOCKED — repaired & restocked" + an
UNDOCK button. (Commit: World/Station.{h,cpp} + Ships/Spaceship.{h,cpp} + Components/{Health,ShipMovement,
TorpedoLauncher,RadarContact,Weapon,Science}* + Core/MissionSubsystem.cpp + Net/StationServerSubsystem.* + docs.)

---

## 2026-06-27 — 🔧 M19.3 Engineering drydock store

The payoff of the loop: spend salvage on ship upgrades, run by Engineering while docked.
- **Upgrade catalogue** (`Core/UpgradeCatalogue.h`, data-in-code) — seven tiered paths (Beam Damage,
  Beam Recharge, Targeting Arc, Hull Plating, Shield Capacity, Torpedo Tubes, Reactor Output), each tier
  adding a magnitude to a component field, with escalating cost (`BaseCost*(tier+1)`) and a rank gate
  (`tier+1`). Owned tiers live in `USpaceGameInstance::UpgradeTiers` (mirrored in the save, cleared on reset).
- **Buy + apply** — `USpaceGameInstance::BuyUpgrade` validates max-tier/rank/credits, deducts, bumps the
  tier, and persists. `ASpaceship::ApplyUpgrades` (run at the end of `ApplyShipPreset`) layers owned tiers
  onto the base stats; `RefreshLoadout()` re-applies + tops up pools after a purchase. ApplyShipPreset now
  resets FireArc/MaxShield/ReactorBudget to base first so re-application is idempotent.
- **Web** — `/api/buy?id=` (docked-gated) buys + refreshes the live ship; `/api/state` carries an
  `upgrades` array (tier, next cost, rank req, maxed, affordable). The Engineering page gained a
  **DRYDOCK — UPGRADES** panel: wallet + rank, and a buy row per path (disabled when unaffordable),
  active only while docked.

**Verified (PIE + MCP + headless Engineering render):** funded to 2000cr/rank5 and docked, buying reactor
×2 + beam damage ×1 left credits at 1100 (250+500+150) with `reactorBudget` exactly 4.0 (no accumulation
bug) and `BeamDamage` 28; the reactor tier round-tripped from disk; an undocked `/api/buy` left credits
untouched; the Engineering page rendered the drydock with SALVAGE 1100cr·RANK 5 and per-upgrade buy
buttons, REACTOR LOAD reading 3.0/4.0. (Commit: Core/UpgradeCatalogue.h + Core/SpaceGameInstance.{h,cpp}
+ Core/SpaceSaveGame.h + Ships/Spaceship.{h,cpp} + Net/StationServerSubsystem.* + docs.)

---

## 2026-06-27 — 🚀 M19.4 Buy new ships (hangar)

The drydock now sells whole ships, completing the M19 progression loop.
- **Ship roster** (`Core/ShipCatalogue.h`, data-in-code) — `FShipDef` per hull (mesh, material, scale +
  base stats + price/rank). Two starters (Interceptor, Cruiser, cost 0) plus two buyable hulls: a glass-
  cannon **Corvette** (1200cr/R2: very fast, fragile) and a heavy **Gunboat** (1800cr/R3: 240 hull, big
  guns, ponderous), both reusing the existing Insurgent/Imperial meshes recoloured/rescaled.
- **ApplyShipPreset** now reads the roster (mesh + stats) instead of a hardcoded variant branch, so a
  ship switch swaps the hull on the live pawn; upgrades still layer on top.
- **Ownership** — `USpaceGameInstance` gains `OwnedShips` (starters implicitly owned), `OwnsShip`,
  `BuyShip` (rank/credit-gated), `SelectShip` (persists active hull); mirrored in the save, cleared on reset.
- **Web** — `/api/ship?action=buy|select&id=` (docked-gated; select re-applies the loadout). `/api/state`
  carries a `ships` array (owned/active/cost/rankReq/affordable). The Engineering page gains a
  **HANGAR — SHIPS** panel: ACTIVE / SELECT / BUY-price rows, shown only while docked.

**Verified (PIE + MCP + headless render):** funded to 2000cr/rank5 and docked, buying the Gunboat left
credits at 200 and selecting it transformed the live pawn (hull 240, beam 42, maxSpeed 1100, 8 torps);
the hangar rendered Interceptor/Cruiser=SELECT, Corvette=BUY·1200cr (dimmed, unaffordable), Gunboat=ACTIVE;
Gunboat ownership survived a load round-trip from disk. (Commit: Core/ShipCatalogue.h + Core/StationTypes.h
+ Core/SpaceGameInstance.{h,cpp} + Core/SpaceSaveGame.h + Ships/Spaceship.cpp + Net/StationServerSubsystem.* + docs.)

**M19 complete** — missions pay salvage + XP, you dock at a starbase to repair/restock, and Engineering
spends the haul on tiered upgrades and new ships. Story mode is the planned M20.

---

## 2026-06-28 — 📖 M20.1 Story mode: Shakedown tutorial mission

Story mode opens with a gentle, narrated tutorial as the new campaign mission 0.
- **Shakedown Cruise** (`UMissionSubsystem::BuildCampaign`) — prepended before First Contact, so the
  campaign is now four missions (`USpaceGameInstance` CampaignMissionCount 3→4). Five scripted comms beats
  (a new recurring cast: CMDR VOSS, ENGINEER KANE, TACTICAL) walk the crew through Helm, Engineering,
  docking at the starbase, and Weapons, then a final on-kill sign-off.
- **Passive target drone** — `FMissionDef::EngageDelayOverride` (new) makes every spawned hostile hold
  fire for N seconds; the tutorial sets a huge value so its lone Scout never shoots. Wired via a new
  `AEnemyShip::SetEngageDelay`, applied in the spawner before `FinishSpawning`.
- Killing the drone completes the drill → victory → advance to First Contact (the real campaign), paying
  out the normal salvage so the player can try the drydock immediately.

**Verified (PIE + MCP + headless Science render):** resetting the campaign loaded "Shakedown Cruise" with
one Scout + the starbase; the four timed beats fired in order (VOSS → KANE → VOSS → TACTICAL) while the
ship took zero fire (log: 0 enemy FIRE) and hull stayed 80/80; destroying the drone fired the final VOSS
beat, banked 40cr, reached victory, and advanced MissionIndex to 1; the Science console rendered the
narration log. (Commit: Core/MissionSubsystem.{h,cpp} + Core/SpaceGameInstance.cpp + Ships/EnemyShip.h + docs.)

---

## 2026-06-28 — ⏸️ M20.2 Between-mission staging (resupply before the fight)

Combat missions no longer drop you straight into a firefight — each opens in a **staging phase** so the
crew can dock, repair, outfit, and buy ships before engaging.
- **Staged start** — `UMissionSubsystem` now splits `OnWorldBeginPlay` into always-on setup (clear
  level-placed hostiles, spawn the starbase) and a deferred `LaunchEncounter` that spawns the fleet +
  starts the comms clock. Combat missions begin `bStaged` with no enemies; a CMDR VOSS prompt invites
  the crew to resupply and launch when ready. The tutorial sets `FMissionDef::bAutoLaunch` to skip
  staging (its passive drone is up immediately).
- **Launch control** — `/api/game?action=launch` calls `LaunchEncounter` (idempotent via `bLaunched`).
  `/api/state` exposes `staged`; the Helm page shows a green **LAUNCH MISSION** button only while staged,
  and the landing page reads "STANDING BY — resupply, then LAUNCH".
- Briefing beats now time from launch (not level load), so they land as the fight opens.

**Verified (PIE + MCP + headless Helm render):** loading First Contact showed `staged:true` with only the
STARBASE on radar (the previously-leaking level-placed enemy is now cleared at begin play); the Helm
rendered the clear sector + LAUNCH button; `action=launch` flipped `staged:false`, spawned WASP-1 +
VIPER-1, and fired the COMMAND briefing; a second launch was a no-op. The tutorial still auto-launches.
(Commit: Core/MissionSubsystem.{h,cpp} + Net/StationServerSubsystem.cpp + docs.)

---

## 2026-06-28 — 📖 M20.3 Story arc for the campaign (the Crimson Pact)

The three combat missions now tell a connected story instead of issuing terse combat prompts.
- **Per-mission briefing** — `FMissionDef` gains `BriefSender`/`BriefText`, shown on the comms log during
  the staging phase (with the generic "dock + LAUNCH" reminder appended). Missions without one fall back
  to the old generic prompt.
- **The arc** — First Contact (raiders probing the Veil → "Crimson Pact markings"; a scout escapes a
  transmission), Patrol Ambush ("that wasn't a probe — it was bait"), and Warlord's Reach (renamed from
  Last Stand — the Pact warlord's flagship, a last stand for the frontier). The recurring cast (CMDR VOSS,
  TACTICAL, ENGINEER KANE — seeded by the tutorial) carries through, with launch + on-kill beats per
  mission.

**Verified (PIE + MCP + headless Science render):** First Contact staged with the VOSS briefing, launch
fired the TACTICAL "Crimson Pact" beat, the first kill fired VOSS's "it got a transmission off" line;
Patrol Ambush and Warlord's Reach showed their briefings; the Science console rendered the Warlord's Reach
briefing cleanly. (Commit: Core/MissionSubsystem.{h,cpp} + docs.)

---

## 2026-06-28 — 🏆 M20.4 Campaign epilogue (close the Crimson Pact arc)

Beating the warlord now lands a proper story ending instead of a generic "CAMPAIGN COMPLETE".
- The final victory shows **"THE VEIL IS SECURE"** with a CMDR VOSS-style epilogue closing the arc, then
  the run's salvage/standing. `UOutcomeMenuWidget` now auto-wraps its subtitle (`SetAutoWrapText` +
  `SetWrapTextAt(760)`) so the multi-line epilogue lays out instead of overflowing.
- Mid-campaign victories are unchanged ("SECTOR CLEARED" + NEXT MISSION).

**Verified (PIE + OS capture of the host overlay):** winning Warlord's Reach (the last mission) rendered
the wrapped "THE VEIL IS SECURE" epilogue + "Salvage: +360 credits, +140 XP — RANK 1" + MAIN MENU.
(Commit: Core/BridgePlayerController.cpp + Core/OutcomeMenuWidget.cpp + docs.)

**M20 story mode complete** — shakedown tutorial, between-mission staging/briefings, a connected
three-mission Crimson Pact arc, and a victory epilogue.

---

## 2026-06-28 — 🗺️ M21.1 Sector starmap

A strategic view of the campaign, openable from Helm or Science.
- `FMissionDef` gains normalised `MapX`/`MapY`; each mission is placed across "THE VEIL FRONTIER" sector.
- New `/api/starmap` returns the systems (name, x, y) with each marked cleared / current / locked from the
  live mission index.
- A **SECTOR MAP** button on the Helm and Science consoles opens a shared full-screen modal (added to the
  page shell) that draws the sector: a dashed route through the systems, green=cleared, amber-ringed=
  current, grey=locked, with labels. Scales to mobile.

**Verified (PIE + Playwright desktop + iPhone):** on First Contact, `/api/starmap` reported Shakedown
cleared / First Contact current / the rest locked; the overlay rendered the route + nodes correctly on
both viewports. (Commit: Core/MissionSubsystem.{h,cpp} + Net/StationServerSubsystem.{h,cpp} + docs.)

---

## 2026-06-28 — 🌀 M21.2 Warp drive (Helm)

A charged FTL jump for the helm officer.
- `ASpaceship` gains a warp drive: `WarpCharge` trickle-builds in Tick (scaled by engine power, so routing
  power to engines charges it faster), `IsWarpReady()` (full charge + not docked), and `Warp()` which
  jumps `WarpDistance` (7000 uu) along the bow, spends the charge, jolts the camera, and flashes cyan
  warp FX at departure + arrival.
- Web: `/api/warp` triggers it; `/api/state` carries `warpCharge` + `warpReady`. The Helm page shows a
  **WARP DRIVE** readout (CHARGING %/READY/offline-when-docked) and a **WARP JUMP** button that arms
  green when ready.

**Verified (PIE + MCP):** the charge built to 1.0 and `warpReady` flipped true; `Warp()` moved the ship
exactly 7000 uu along its heading and reset the charge to 0 (the Helm's STARBASE range jumped 5000→12000
afterward); the Helm rendered the READY state + WARP JUMP button. (Commit: Ships/Spaceship.{h,cpp} +
Net/StationServerSubsystem.{h,cpp} + docs.)

---

## 2026-06-28 — 💥 M22 Collision (ramming) mechanics

Ships now collide: flying — or warping — into an enemy hull damages both.
- `ASpaceship::HandleCollisions` (per Tick) checks each living enemy against `CollisionRadius` (900 uu).
  On first contact it deals ram damage scaled by closing speed (`RamDamage` × 0.5→1.5 with impulse) to the
  enemy and `RamSelfFraction` (0.6) of it to the player, knocks both ships apart along the contact normal,
  jolts the camera, and flashes an impact spark. Contacts are debounced via a touching-set so one collision
  only hurts once. Skipped while docked.
- Ramming routes through the normal `UHealthComponent::ApplyDamage`, so shields absorb first and a hard
  enough ram can destroy an enemy (counting as a kill — rewards + comms).

**Verified (PIE + MCP):** teleporting the ship 400 uu from the tutorial drone triggered a ram next tick —
the drone's shield dropped 20→9 (11 dmg at standstill), the player took 7 (absorbed by shields), and the
two ships were flung 400→1400 uu apart; the log recorded a single "Rammed … 11 dmg (self 7)" event (no
per-frame spam). (Commit: Ships/Spaceship.{h,cpp} + docs.)

---

## 2026-06-28 — 💥 M22.1 Collision polish: heavier rams, shield hitbox, sparks, debris

Feedback pass on the new ramming.
- **Heavier, mutual rams** — `RamDamage` 22→48 and `RamSelfFraction` 0.6→1.0, so a ram is a hard hit that
  costs the attacker as much as the target (no more cheap ramming). Still scales 0.5→1.5x with impulse.
- **Shield hitbox** — shields now enlarge the collision bubble: `CollisionRadius` is the hull-to-hull
  base (650), and each ship with shields up adds `ShieldRadiusBonus` (320). A shielded ship is hit
  sooner, on the shield rather than the hull (verified: a ram landed at 1000 uu, beyond the 650 hull
  radius, tagged `[shield]`).
- **Impact sparks, not a blast** — collisions now spawn a short shower of `ABeamFx` spark streaks
  (orange for a hull hit, cyan for a shield hit) instead of the explosion sphere.
- **Debris** — new `ADebris` actor: a destroyed ship throws 5 (cruisers 8) jagged, tinted hull chunks
  that drift with drag, tumble, and shrink away over ~8–14 s. Spawned from `AEnemyShip::HandleDeath`.

**Verified (PIE + MCP + OS capture):** a standstill ram now reads 24 dmg / 24 self (was 11/7); the
shielded-bubble ram triggered at 1000 uu with the `[shield]` tag; destroying a Scout spawned 5 `ADebris`
chunks (jagged non-uniform scale, drifting), visible as Debris0–4 in the world. (Commit:
Ships/Spaceship.{h,cpp} + Ships/EnemyShip.cpp + FX/Debris.{h,cpp} + docs.)

---

## 2026-06-28 — 🪐 M23.1 Sector landmarks (open-world foundation)

First slice of the open sector (M23): the campaign's systems become distinct bodies laid out in space.
- New `AWorldLandmark` (`World/WorldLandmark.{h,cpp}`): a big sphere with a non-targetable radar blip + a
  display name. Planets use LIT materials (M_Earth for the blue home world, M_Insurgent/M_PlayerHull for
  others) so they read as shaded worlds up close; the sun uses the emissive glow — clearly different bodies,
  all from the existing palette (no imports).
- `FMissionDef` gains `LandmarkName/Kind/Color/Scale` (+ `ELandmarkKind{Planet,Sun}`). `UMissionSubsystem`
  maps each system's `MapX/MapY` to a world position over a `SectorSpan` (120 k uu), anchored on the player
  start (home offset so the ship isn't inside the home body), and spawns a landmark at each — Haven (home),
  Tarsis, Korrin Belt, and the **Ember** sun at the Pact staging point.
- Landmark names resolve on the consoles (the `/api/state` contact `DisplayName` now handles
  `AWorldLandmark`).

**Verified (PIE + MCP + OS capture):** four landmarks spawned at the right spread (Haven at home → Ember
~85 k uu out); they show as named, colour-tinted neutral blips on the Helm radar; up close Tarsis renders
as a lit, textured moon (not a white blob). (Commit: World/WorldLandmark.{h,cpp} + Core/MissionSubsystem.{h,cpp}
+ Net/StationServerSubsystem.cpp + docs.)

---

## 2026-07-01 — 🌌 M23.2 Open-world flow: proximity triggers + seamless clears

The campaign stops being a chain of level reloads and becomes one continuous open sector.
`UMissionSubsystem` is now the **sector director**: the level loads once; on a 0.25 s timer it watches
the player and, when the ship flies within `TriggerRadius` (18 k uu) of the active objective's landmark,
spawns that mission's fleet **around the body** (facing the incoming ship) and opens its comms — no more
staging phase or LAUNCH button.
- **Seamless clears** — when the live fleet is wiped, the director advances + saves the campaign, fires a
  "Sector cleared — next objective: <system>. Lay in a course." beat, and arms the next objective (its
  briefing appends "Set course for <landmark> and engage"). No pause, no overlay, no reload. Only the
  **final** clear hands off to `ABridgePlayerController::OnCampaignComplete()` for the "THE VEIL IS SECURE"
  epilogue. Defeat/retry unchanged.
- The controller's `HandleEnemyDeath` no longer ends the encounter — it only banks salvage + camera trauma;
  the director owns clear detection (it tracks the spawned `LiveFleet`).
- **Warp** bumped 7 k → **18 k uu** so a jump meaningfully shortens the long sector hops.
- Consoles: dropped `staged`/LAUNCH; `/api/state` now carries `objective`, `engaged`, `objectiveDist`; the
  Helm shows an OBJECTIVE readout (name + range, or "ENGAGED") and the footer reads "◇ EN ROUTE — <system>"
  until a fight opens. `/api/game?action=launch` repurposed to a debug force-trigger.

**Verified (PIE + MCP + web /api/state):** spawned at home → the tutorial drone triggered by proximity
(`engaged:true` at 14 k); killing it advanced Shakedown→First Contact **with `phase:playing`** (no reload,
comms beat fired), objective flipped to Tarsis at 29 k; teleporting the ship to Tarsis proximity-triggered
its Scout+Gunship fleet (WASP-1 + VIPER-1) — all four landmarks present throughout the single world; warp
distance read 18 000. (Commit: Core/MissionSubsystem.{h,cpp} + Core/BridgePlayerController.{h,cpp} +
Ships/Spaceship.h + Net/StationServerSubsystem.cpp + docs.)

---

## 2026-07-01 — 🧭 M23.3 Live navigation map + warp-to-objective

The sector map becomes a real nav display and warp links to it.
- `/api/starmap` now carries the live ship position folded back into the normalised map space
  (`UMissionSubsystem::GetMapPosition`, the inverse of `GetSystemLocation`), each system's landmark name +
  kind (`sun`) + colour, and the distance to the active objective. The shared modal polls while open
  (400 ms) and plots the player as a chevron, a dashed course line to the objective, each body tinted/sized
  by kind (planets vs the bigger amber sun) ringed by status (green cleared / yellow current / grey locked),
  and an "OBJECTIVE: <system> — <dist>k uu / ENGAGED" readout.
- **Warp-to-objective:** new `ASpaceship::WarpToObjective(Target)` turns the bow to the objective and jumps
  toward it, clamped so it never overshoots (4 k standoff) and spending the charge. `/api/warp?mode=objective`
  drives it; a **LAY IN COURSE** button sits on the nav map (hidden once engaged) and a compact **COURSE**
  button on the Helm beside the tactical WARP.

**Verified (PIE + MCP + web + headless capture):** at Tarsis (29 266 uu out) one course-warp turned the bow
and jumped exactly 18 000 uu toward it (→ 11 266), the player marker moving diagonally toward the system on
the live map, and the jump landed inside the trigger zone so First Contact's fleet powered up. A headless
Chromium capture of the nav map showed the four bodies correctly tinted/ringed (Haven cleared, Tarsis current
with the ship chevron on it, Korrin locked, the Ember sun larger/amber), the course line, and the objective
readout. (Commit: Core/MissionSubsystem.{h,cpp} + Ships/Spaceship.{h,cpp} + Net/StationServerSubsystem.cpp + docs.)

---

## 2026-07-01 — 🪐 M23.4 Sector-map & celestial-body polish (user fixes)

Fixes off user feedback ("sector map should be a real map not a progress bar; the station and a planet
have weird textures; two identical earths; didn't see a sun").
- **Nav map is a real chart:** dropped the dashed polyline that chained the systems in mission order (it
  read like a progress bar); systems now sit at their true `MapX/MapY` on a coordinate grid + border, with
  the live ship→objective course line kept for navigation.
- **New planet material `M_Planet`** (authored via MaterialEditingLibrary): a tintable lit sphere with
  world-noise surface mottling (cloud/continent bands, darkened so it never blows out) + a subtle Fresnel
  atmosphere limb. `AWorldLandmark::Setup` now uses it (as a runtime MID coloured to each body's hue) for
  every non-home planet, so no two look alike and none wear a ship-hull texture. The home world keeps the
  detailed `M_Earth`; the sun keeps the emissive glow.
- **Two earths → one:** the level had a leftover backdrop Earth actor (`Earth` mesh, scale 260) near origin
  from an early milestone; now that Haven is a real Earth landmark it was the duplicate — removed from
  `VSlice_Arena` and the map re-saved.
- **Station rework:** was the enemy `Imperial` *ship* mesh under a flat emissive `M_GlowCyan` (a glowing
  blob). Now a wide, flat metallic hub (Engine `Cylinder` + lit `M_Imperial`) with a glowing cyan beacon
  core on top — reads as a built structure.

**Verified (PIE + MCP + headless captures):** nav map shows a gridded spatial chart (no chain line); the
home area shows a single Earth beside the new hub-and-beacon station; Tarsis renders as a shaded teal world
with cloud mottling (not a white blob, not an Earth); the Ember sun reads as a bright star (far out at the
final objective). (Commit: World/WorldLandmark.cpp + World/Station.{h,cpp} + Net/StationServerSubsystem.cpp
+ Content/Art/Materials/M_Planet.uasset + Content/Maps/VSlice_Arena.umap + docs.)

---

## 2026-07-01 — 🛰️ M23.5 Solid planets + tighter dock range (user fixes)

Two feel fixes off user feedback.
- **Planets/suns are solid.** `AWorldLandmark` caches its world radius (mesh bounds × scale, exposed via
  `GetBodyRadius`); the ship's per-tick `HandleCollisions` now also clamps the ship to `radius +
  BodyClearance(500)` of any celestial body — flying full-throttle into a planet halts it at the surface
  (engines cut on contact) instead of passing through. Reuses the existing not-docked collision guard.
- **Dock zone tightened:** `AStation::DockRange` 3000 → 1600, so you must actually pull up to the hub to
  dock rather than from far out.

**Verified (PIE + MCP):** teleporting the ship 1000 uu inside Tarsis ejected it to the surface (3200 =
2700 radius + 500); flying full-throttle straight at Tarsis stopped dead at 3200 (no pass-through); dock
range now reads `canDock` true at 1400 uu and false at 1800 uu. (Commit: Ships/Spaceship.{h,cpp} +
World/WorldLandmark.{h,cpp} + World/Station.h + docs.)

---

## 2026-07-08 — 💥 M24 Combat readability & death polish

Phase 6 opener (PROJECT_PLAN M24): the smoke-test punch list around dying and arriving.
- **Player death is an event now.** `ASpaceship::HandleShipDestroyed` (bound to its own health
  `OnDeath`): main blast + 5 satellite bursts (`AExplosionFx`) + 6 tumbling `ADebris` chunks, hull mesh
  hidden + collision off, helm frozen (`SetInputLocked`), engine hum stopped, full camera trauma. The pawn
  survives so the follow camera frames the wreck.
- **Defeat beat.** `HandlePlayerDeath` flips the phase to Defeat immediately (web API reports it, AI reads
  it) but the DEFEAT overlay now waits `DefeatBeatSeconds` (2.2 s) on a timer — the crew watches the ship
  go up instead of an instant menu.
- **Hostiles stand down.** `AEnemyShip::Tick` early-outs to Idle when the player's health reads dead — no
  more beaming the wreck forever (smoke-test #2).
- **Stale lock cleared.** `UWeaponComponent::TickComponent` drops `CurrentTarget` when the actor is gone
  (smoke-test #1: consoles kept showing the dead ship).
- **Arrival grace 6 → 12 s** (`EngageDelay` default): spawned fleets close in but hold fire ~12 s so a solo
  crew can man stations after a warp-in (smoke-test #4 death trap).
- **Warp standoff clears the body.** `WarpToObjective` standoff is now `max(4000, bodyRadius + 3000)` for
  a landmark at the objective — laying in a course for Ember (r≈12500) arrives off the surface instead of
  inside its collision clamp.

**Verified (PIE + MCP + /api/state):** lock WASP-1 → kill → `target:"none"` on the API; warp-to-Ember from
20000 out lands exactly 15500 from centre; Tarsis fleet spawned 08:10:59.5, first shot 08:11:13.1 (~13.6 s);
player kill → same frame: phase Defeat + both raiders `state -> Idle` + mesh hidden + zero FIRE lines after;
not paused immediately, paused (overlay) ~2.2 s later; death screenshot = screen-filling blast; defeat →
`/api/game?action=restart` → playing, hull 80/80, mission + credits kept. Log: 0 Fatal/Ensure/Accessed None.
(Commit: Ships/Spaceship.{h,cpp} + Ships/EnemyShip.{h,cpp} + Core/BridgePlayerController.{h,cpp} +
Components/WeaponComponent.cpp + docs.)

---

## 2026-07-08 — 📡 R5(a) API honesty + root redirect

Web-console API truthfulness (RELEASE_PLAN R5, smoke-test #3/#5). Every mutating `/api/*` endpoint now
answers `{"ok":true}` or `{"ok":false,"reason":"…"}` (application/json) instead of an unconditional
plain-text `ok`.
- **Command gate** `GetCommandShip()`: ship must exist, be alive, and the phase be Playing — otherwise the
  reason says why (e.g. `ship destroyed - encounter over`). `/api/game` is deliberately ungated: restart
  from the defeat screen is its main job (it now rejects unknown actions instead of okaying them).
- **Per-endpoint reasons:** helm (docked / missing params), dock (already docked / not docked / out of
  range-moving), warp (offline docked / still charging), buy + ship (not docked / credits-rank-tier / not
  owned), weapons fire mirrors `FireBeam`'s gate order (charging / no target / out of range / out of arc),
  torpedo, science scan (no contact), power (missing system), engineering weld (rate-limit not credited).
- **Root redirect:** the router can't route "/" (`FHttpPath` rejects it), so a request preprocessor 301s
  `/` → `/stations` (unregistered on teardown). No more bare-IP 404 (smoke-test #5).
- Console JS fires-and-forgets its command fetches, so the body change is compatible; pages can start
  surfacing `reason` later (R2 polish).

**Verified (PIE + curl):** `curl -i /` → `301 Location: /stations`; fire with no target → `no target`; buy
undocked → `not docked…`; undock while free-flying → `not docked`; `action=bogus` → `unknown action`; helm
no params → `missing throttle/turn`; warp twice → ok then `warp drive still charging`; scan with no contact
→ `no scan contact…`; after killing the ship: helm/fire → `ship destroyed - encounter over` but
`game?action=restart` → `{"ok":true}` and the encounter resets (phase playing, hull 80). `/stations` +
`/helm` still 200; log clean. (Commit: Net/StationServerSubsystem.{h,cpp} + docs.)

---

## 2026-07-08 — 📦 R1 First packaged build (Linux)

The game runs outside the editor for the first time (RELEASE_PLAN R1).
- **Dev plugins fenced:** `UnrealClaude` + `VibeUE` get `"TargetAllowList": ["Editor"]` in
  `SpaceGame.uproject` — they no longer ship (packaged build stages no project plugins, no :8088
  listener, zero VibeUE/Claude log lines → the "arbitrary Python execution" security hole is editor-only).
- **Project identity:** `DefaultGame.ini` was literally empty; now GeneralProjectSettings carry
  ProjectName "SpaceGame — Bridge Simulator" (working title — final name is a user call), version 0.9.0,
  company/copyright + CC0 asset credit line.
- **Cook settings:** the runtime loads assets by string path (LoadClass `WBP_BridgeHUD`,
  ship/upgrade-catalogue `StaticLoadObject`s) that the cooker can't see, so
  `+DirectoriesToAlwaysCook=(Path="/Game")` cooks all content (small, ~63 MB raw) and both maps are in
  `MapsToCook`. `Packaged/` is gitignored.
- **BuildCookRun** (Linux Development, build+cook+stage+pak+archive): BUILD SUCCESSFUL first try —
  473 packages cooked, zero missing-asset warnings, exit 0.

**Verified (packaged binary, no editor):** default boot lands on `/Game/Maps/MainMenu`; launching into
`VSlice_Arena` starts Shakedown with the :8080 station server live; played mission 1 entirely over the
LAN API (warp?mode=objective → 3500 uu off Haven, cycle → WASP-1, helm turns into arc, fire → kill) —
credits +40 and seamless advance to First Contact/Tarsis; `/stations` + `/helm` 200, `/` 301 →
`/stations`; M24 lock-clear works packaged; log: 0 Fatal/Ensure/Accessed None. Shipping config + icon/
splash still pending (R1 tail). (Commit: SpaceGame.uproject + Config/DefaultGame.ini + .gitignore + docs.)

---

## 2026-07-08 — 📦 R1(b) Shipping build + menu-world fixes + remote game start

Closing out R1: the Shipping config packages and the full loop is provable without a keyboard.
- **Shipping BuildCookRun** succeeds (exit 0). Gotcha discovered: Shipping ignores a command-line
  map override (UE behaviour), so the packaged gate test has to go menu → game like a player would.
- **Menu-world sector leak fixed:** `UMissionSubsystem::OnWorldBeginPlay` now early-outs in worlds
  without `ASpaceGameMode` — previously the MainMenu world (also EWorldType::Game when packaged) built
  the whole sector behind the menu (starbase + 4 landmarks + director timer).
- **Remote game start:** `/api/game?action=new|restart` from the menu world now opens `VSlice_Arena`
  (same flow as the NEW GAME button) instead of silently no-opping — a crew phone can start the game.

**Verified (Shipping package, no editor):** boot → menu world has 0 contacts (leak gone) →
`game?action=new` → `{"ok":true}` → arena: hull 80, tutorial engaged, 6 contacts → mission 1 played
over the LAN API (warp to Haven, cycle WASP-1, turn into arc, fire to kill) → credits 40, seamless
advance to First Contact/Tarsis, lock cleared. Dev-editor PIE re-verified the same two fixes first.
`PackagedShipping/` covered by the gitignore (`Packaged*/`). (Commit: Core/MissionSubsystem.cpp +
Net/StationServerSubsystem.cpp + .gitignore + docs.)

---

## 2026-07-08 — 📱 R2(a) Crew-console URL on the menus

First R2 slice: nobody should need the log to find the consoles.
- `UStationServerSubsystem::GetCrewUrl()` (public static): "http://<lan-ip>:8080/stations", empty when
  no LAN adapter.
- **Main menu** shows "CREW CONSOLES → <url>" between the subtitle and NEW GAME — the station server
  already listens in the menu world, and with R1(b)'s remote start the crew can join (and even start
  the game) before the captain touches a key. Falls back to "no LAN adapter found".
- **Pause overlay** shows the same line under QUIT, for crew joining mid-session.

**Verified (PIE + OS captures):** menu shows `CREW CONSOLES → http://192.168.178.71:8080/stations`
under the title; ESC pause overlay shows the same under QUIT. (Commit: Net/StationServerSubsystem.{h,cpp}
+ Core/MainMenuWidget.cpp + Core/PauseMenuWidget.cpp + docs.)

---

## 2026-07-08 — ⌨️ R2(b) Solo hotkey playability + controls reference

The full gameplay loop is now playable by one player at the keyboard — torpedo, dock, warp, and
science were web-console-only verbs before this.
- **New hotkeys** (`ABridgePlayerController::SetupInputComponent`): **F** dock/undock toggle and
  **R** tactical bow warp and **G** lay in course (warp to the active objective, same path as
  `/api/warp?mode=objective`) on Helm; **T** fire torpedo on Weapons; **C** cycle scan contact and
  **X** scan from any station; **H** toggles a controls card. Handlers are `BlueprintCallable`
  (TogglePause precedent) so PIE tests can drive them via `call_method`.
- **`UControlsOverlayWidget`** (new): the H card — dim non-pausing overlay at viewport z 110
  (above HUD, below pause), listing every key by station.
- **Main menu CONTROLS button** (between CONTINUE and QUIT): swaps to the same key reference with
  BACK. Both views share `MenuUI::AddControlRows` so they can never drift apart.

**Verified (PIE, handlers driven via `call_method` + `/api/state`):** C/X → `sciTarget` WASP-1,
`sciScanning` true; R → 18 000-unit bow jump; station gating holds (HelmWarp on Weapons moves 0);
F beside the starbase → `IsDocked()` true, second F → false; G → bow turned 74° and jumped toward
the objective; T with a locked in-arc contact → ammo 3→2. OS captures confirm the H overlay
in-game and the menu CONTROLS panel + BACK. Python note: the `EStation` py class is shadowed by
`AStation` — recover it via `type(pc.call_method("GetStation"))`. (Commit:
Core/ControlsOverlayWidget.{h,cpp} + Core/BridgePlayerController.{h,cpp} + Core/MainMenuWidget.{h,cpp}
+ Core/MenuUI.h + docs.)

---

## 2026-07-08 — ⚙️ R2(c) Settings menu + quit confirmations

Rest of R2's first-run UX (minus the tutorial rewrite — see note).
- **`USettingsMenuWidget`** (new): SETTINGS overlay reachable from the main menu and the pause
  menu (viewport z 130, above pause). Four cycle-rows showing live values: WINDOW MODE
  (fullscreen/borderless/windowed), RESOLUTION (display-supported modes, fallback list),
  QUALITY (LOW→EPIC via engine scalability), MASTER VOLUME (10% steps, wraps). Every click
  applies immediately and persists — window/res/quality through `UGameUserSettings`
  (GameUserSettings.ini), volume through `FApp::SetVolumeMultiplier` + a `[SpaceGame.Audio]`
  config key re-applied at boot by the new `USpaceGameInstance::Init`. SFX/music split deferred
  until music exists (Q6) — one master slider is honest for the current soundscape.
- **Quit confirmations:** main-menu QUIT swaps to a QUIT TO DESKTOP?/BACK panel; the pause
  menu's QUIT **and** MAIN MENU both swap to a confirm panel ("Progress since your last save
  will be lost.") before proceeding.

**Verified (PIE):** settings panel captured over the main menu with live values; quality cycled
CUSTOM→EPIC; volume clicks wrote `MasterVolume=0.9` then wrapped back to `=1` in
GameUserSettings.ini (quality persists to the editor ini under PIE, game ini when packaged);
BACK removes the overlay (viewport-check). Pause flows proven end-to-end: OnMainMenu→confirm→
BACK restores; QUIT→CONFIRM actually quit (PIE ended); MAIN MENU→CONFIRM swapped the world
VSlice_Arena→MainMenu; SETTINGS opens/closes over the pause panel.

**Note:** the R2 "tutorialisation pass" (rewriting Shakedown comms around the new solo hotkeys)
was **dropped by user decision** — solo keyboard play is a testing affordance, not the shipped
mode; the tutorial keeps teaching the crew-console flow. (Commit: Core/SettingsMenuWidget.{h,cpp}
+ Core/SpaceGameInstance.{h,cpp} + Core/MainMenuWidget.{h,cpp} + Core/PauseMenuWidget.{h,cpp} + docs.)

---

## 2026-07-09 — 🔧 M25 Subsystem damage — Engineering matters in combat

Hull hits can now knock out ship systems; the crew feels it and Engineering fixes it.
- **`UDamageControlComponent`** (new, player ship only — enemies keep flat stats): three
  damageable systems `EDamageSystem { Engine, Weapons, Sensors }`. On every **hull** hit
  (shields up = no roll), 35% chance (`DamageChance`) a random still-working system breaks and
  runs at 50% (`DamagedMultiplier`) with an alarm sting + log line. Consumers use the same
  lazily-cached `FindComponentByClass` pattern as power scaling, defaulting to 1.0 when the
  component is absent.
- **Effects:** Engine → `GetEffectiveMaxSpeed` halves; Weapons → beam recharge halves;
  Sensors → Helm radar range halves (blips clamp closer) and a new Science `ScanRange`
  (40 000 uu, `GetEffectiveScanRange`) halves — `BeginScan` and `/api/science?action=scan`
  refuse targets beyond it ("close the distance").
- **Repair:** the existing Engineering weld sweep now repairs systems **before** hull —
  3 welds (`WeldsPerSystemRepair`) fix one system, first-damaged-first (`GetRepairTarget`);
  hull welds resume once all systems are green. Docking (`Dock()`) repairs everything.
- **Visibility:** on-ship Engineering console rows and the web engineering page go **amber
  “⚠ DMG”** on damaged systems; the DAMAGE CONTROL label shows “⚠ REPAIRING: <systems> —
  WELD ON GREEN”; a “! SENSORS DMG” flag rides the reactor line. `/api/state` gains
  `dmg:[e,w,s]` + `scanRange`, and `radarRange` reflects the sensors multiplier live.

**Verified (PIE + /api/state + headless-Firefox page shot):** scripted damage halves every
stat exactly — maxSpeed 2100→1050, radarRange 20000→10000, scanRange 40000→20000, beam
recharge 0.55→0.275/s (measured 0.275; Interceptor's catalogue rate is 0.55, ×0.5 exact).
Combat roll fires only on hull hits (shield hits: no roll; first hull hit broke SENSORS).
Weld sequencing proven: hull frozen at 70.2 through welds 1–3 (ENGINE green), 4–6 (SENSORS
green), then +8/weld hull repair; scan beyond 20 000 uu returned the range verdict over the
API. Engineering web page screenshot shows ENGINE “100% ⚠ DMG” amber + the REPAIRING sweep
label. (Commit: Components/DamageControlComponent.{h,cpp} + Components/{ScienceComponent,
ShipMovementComponent,WeaponComponent}.{h,cpp} + Core/{EngineeringConsoleWidget,RadarWidget}.cpp
+ Net/StationServerSubsystem.cpp + Ships/Spaceship.{h,cpp} + docs.)

---

## 2026-07-09 — 🛸 M26 Enemy archetype behaviors — Helm & Science matter

The three archetypes now fight differently, and each one pressures a different station.
- **Scout — strafe runs** (`bStrafeRuns`): no static standoff. It dives past the player at
  full speed (aiming a 1 400 uu lateral lead so a run is a fly-by, not a ram), breaks into a
  new `Overshoot` AI state inside `StrafePassDistance` (2 800 uu), holds its heading flat out
  to `StrafeBreakoffDistance` (9 000 uu), then loops back — alternating sides each pass. It
  holds fire through the loop-out.
- **Gunship — torpedo volleys** (`bTorpedoVolleys`): beams replaced by 3-round homing torpedo
  volleys (`VolleySize`/`VolleyGap` 0.6 s, `FireInterval` 9 s) at 1 900 uu/s — slower than the
  Interceptor's 2 100 top speed, so the helm can outrun them. `ATorpedoProjectile` gained a
  lifetime param (enemy rounds chase 12 s) and an honesty fix: `Detonate` only lands the
  payload if the target is inside `BlastRadius` (700 uu) — a torpedo that times out because
  the target ran **fizzles harmlessly** (applies to the player's shots too).
- **Cruiser — armored** (`bArmored`): player beams deal 50% (`ArmoredBeamMultiplier`) until
  Science completes a scan — `UScienceComponent` scan-complete calls `RevealWeakpoint()` and
  posts a SCIENCE comms confirm ("armor gap is on the tactical grid") via the director's new
  `PostComms`. Torpedoes always land full damage (the counter while unscanned).
- Mission fleets already mix archetypes (M18); no def changes needed.

**Verified (PIE, mission 3 "Warlord's Reach" fleet forced via `/api/game?action=launch`):**
scouts logged full strafe cycles (Engage→Overshoot→Approach→Engage; telemetry: HORNET-2 in to
1 403 uu, past to 8 610, loop, re-engage); VIPER-1 logged "TORPEDO VOLLEY inbound (3 rounds)"
+ staggered launches, and fleeing after a launch made all three rounds log "fizzled ~40 000 uu
short — evaded" (staying put earlier proved they hit: hull dropped to defeat); cruiser armor
flip exact — same beam took LEVIATHAN-1's shield down 10.0 pre-scan (mult 0.5) and 20.0
post-scan (mult 1.0), with the weakpoint log + SCIENCE comms beat firing on scan complete.
[S] `strafe_101238.png`: a scout crossing the bow at 2 000 uu mid-Overshoot. Bonus: an M25
engine-damage roll fired in live combat (maxSpeed 1050 in the defeat-state JSON). Python
notes: `time.sleep` in MCP scripts blocks the game thread (scans/recharge never tick) — split
steps across invocations; landmark actors are reachable via `get_all_actors_of_class(world,
unreal.WorldLandmark)` and the GI via `unreal.GameplayStatics.get_game_instance`. (Commit:
Ships/EnemyShip.{h,cpp} + FX/TorpedoProjectile.{h,cpp} + Components/{WeaponComponent,
ScienceComponent}.cpp + Core/MissionSubsystem.{h,cpp} + docs.)

---

## 2026-07-09 — 📡 M27 Living sector — travel events

The long hops between systems are alive now: while the crew isn't engaged, the director rolls
every 25 s (`EventRollInterval`, 40% `EventChance`) for **at most one live event**.
- **Distress call**: a convoy under raider attack near the closest *other* system — two scouts
  spawn there and a MAYDAY hits the comms. Clear them inside 150 s for a 150-credit bounty
  (plus per-kill salvage); too slow and the convoy goes dark.
- **Pirate interdiction**: a scout + gunship ambush powers up 11 000 uu dead ahead on the
  ship's course. Kill both for +60 credits; they jump out after 180 s.
- **Salvage cache**: a new `ASalvageCache` cargo pod (glowing tumbled cube, amber neutral
  radar blip, not targetable) drifts 9 000 uu off. Fly within 1 500 uu to tractor it in —
  pays 90 credits, or a torpedo restock when the magazine has room. Beacon dies after 120 s.
- Every beat announces on comms (`PostComms`); the **nav map** draws a pulsing pink diamond
  with a label + live countdown at the event (`/api/starmap` gains an `event` object), and the
  pod/ambush ships mark the helm radar via their radar contacts. Event ships live in their own
  `EventFleet` — wiping them resolves the event but never advances the campaign, while each
  kill still banks its salvage through the controller. `/api/game?action=event&type=...`
  force-starts one (debug, mirrors `action=launch`); state readouts show the pod as "CARGO POD".

**Verified (PIE + /api + headless-Firefox map shot):** all three types forced, resolved, and
paid out — salvage collected by proximity (credits 0→90, ENGINEERING comms) *and* expired
naturally at 120 s ("beacon just died", EVENT EXPIRED log); distress resolved (+150 bounty,
CONVOY MASTER comms, credits math exact at 360); interdiction spawned scout+gunship ahead and
paid +60 on the wipe ("Pirates neutralised"). A **natural roll** also fired mid-test (random
distress at Haven 26 s after the previous event closed) and the forced event correctly no-oped
against it — the one-live-event rule holds. [S] `navmap_event.png`: the sector map with the
pink "◆ SALVAGE 120s" diamond beside Haven and the pod's amber blip on the helm radar above.
(Commit: World/SalvageCache.{h,cpp} + Core/MissionSubsystem.{h,cpp} + Net/StationServerSubsystem.cpp
+ docs.)

---

## 2026-07-09 — 📋 M28 Station contracts

Docking at the starbase now offers work beyond the drydock: a **contract board** with one
signable job at a time, persisted in the campaign save.
- **Three contract types**, rolled fresh on each docking (while none is active): **bounty**
  (a named pirate — KRAIT/DUSKRUNNER/RED HARROW/VULTURE/IRONJAW — loiters at a random system;
  destroy it, 220 cr), **patrol** (sweep two named systems in order, 140 cr), **delivery**
  (run supplies to a system and dock back home, 160 cr). Completion pays credits + XP/4.
- **Persistence:** contract fields live on `USpaceGameInstance` mirrored into `USpaceSaveGame`
  (type, two target indices, stage, ship name, reward); accept/stage/complete all auto-save.
  On world begin-play the director respawns an active bounty's target, so reloads keep the hunt.
- **Loitering AI:** `AEnemyShip` gains an `AggroRange` leash (0 = always hunts, unchanged
  default) — the bounty target idles at its landmark until the player closes to 15 000 uu.
- **Web board** on the engineering page below the hangar: shows the active contract's progress
  anywhere, the day's offer + ✎ ACCEPT while docked (new `/api/contract` endpoint, accept via
  `action=accept`); every beat (logged / waypoint / complete) goes out on STARBASE OPS comms.
  The director tracks waypoints at 4 Hz within 9 000 uu (`ContractVisitRange`).

**Verified (PIE + /api + save/load):** all three types accept→complete→payout — bounty:
DUSKRUNNER spawned loitering (AI **Idle** at 60 k uu while the tutorial drone stayed Engaged),
survived a full world reload (respawned at Korrin Belt) *and* an explicit
`ClearContract`→`LoadCampaign` disk roundtrip, kill paid 220 + 80 salvage; patrol: leg-1 comms
at Ember, disk roundtrip mid-contract, leg 2 at Tarsis paid 140; delivery: stage 1 at Ember,
**world reload mid-contract kept stage 1**, docking home completed it for 160 (credits
300→440→600 exact). Offer rerolls proven across dockings (bounty/patrol/delivery mix); offers
null while undocked or a contract is active. [S] `board2.png`: the engineering console with
"ON OFFER — BOUNTY … Pays 220 cr." + ACCEPT CONTRACT button. Test note: back-to-back
undock+dock inside one MCP script never sees a director tick — no dock edge, same offer; give
the 4 Hz director a beat between them. (Commit: Core/{StationTypes,SpaceSaveGame,
SpaceGameInstance{,.cpp},MissionSubsystem{,.cpp}}.h/.cpp + Ships/EnemyShip.{h,cpp} +
Net/StationServerSubsystem.{h,cpp} + docs.)

---

## 2026-07-09 — 🚨 M29 Red alert & power doctrine

The bridge gains an alert state and one-tap Engineering doctrines — shields are now something
the crew *runs*, not a passive stat.
- **Red alert** (`ASpaceship::SetRedAlert/ToggleRedAlert`): toggled by **B** on Helm (gated,
  in the controls card) or `/api/alert?state=red|green|toggle` from any console (whole-bridge
  doctrine). Going red plays the alarm sting and logs the state change.
- **Shield doctrine** (`UHealthComponent::TickShield`, driven from the player ship's Tick):
  shields **only charge at red alert** (`ShieldChargeRate` 4/s × Shields power) and **bleed at
  green** (`ShieldBleedRate` 1.5/s). Docked ships hold steady (combat-safe). Enemies keep
  their static pools. The encounter trigger now nudges the crew on comms: "Recommend RED
  ALERT — shields won't charge without it."
- **Power presets** (`UPowerComponent::ApplyPreset`): COMBAT 0.4/1.3/1.3, TRAVEL 2.0/0.5/0.5,
  BALANCED 1/1/1 — one-tap buttons on the web engineering page (`/api/power?preset=...`),
  each triple summing to the nominal reactor budget. **Bug found & fixed:** the budget-guarded
  setter clamped a boost applied before the cuts (TRAVEL from COMBAT gave Engine 0.4, not
  2.0) — presets now clear all three systems before setting the triple.
- **Console tint:** every web console goes red at red alert (`body.red` CSS via the shared
  poll — dark red page, red header, red-bordered buttons); the Helm button flips to
  "⚠ STAND DOWN — GREEN"; the engineering page gains a SHIELD row with a ▲/▼ doctrine arrow;
  the on-ship HUD hull line becomes "⚠ RED ALERT  HULL …" in red. `/api/state` gains
  `shield`, `maxShield`, and `alert`.

**Verified (PIE + /api):** [L] shield gating exact — 4.01/s measured charge at red (rate ×1.0
Shields power), 1.49/s measured bleed at green, pool capped at 50 and drained to 0; [L]
presets return exactly [0.4, 1.3, 1.3], [2.0, 0.5, 0.5], [1.0, 1.0, 1.0] over `/api/state`
(including the previously-clamped COMBAT→TRAVEL transition); B toggles red on Helm and is
inert on Weapons; the TACTICAL "Recommend RED ALERT" beat fired on encounter trigger.
[S] `red_alert.png`: the whole helm console tinted red with the STAND DOWN button.
(Commit: Ships/Spaceship.{h,cpp} + Components/{HealthComponent,PowerComponent}.{h,cpp} +
Core/{BridgePlayerController.{h,cpp},BridgeHUDWidget.cpp,MenuUI.h,MissionSubsystem.cpp} +
Net/StationServerSubsystem.{h,cpp} + docs.)

---

## 2026-07-09 — 👑 M30 Final battle v2 + skirmish + difficulty

Q5's closer: the campaign finale becomes a phase fight, and the game grows a post-campaign
mode and a challenge dial.
- **Flagship phase fight** (final objective only): the Warlord's cruiser spawns
  **invulnerable** behind two **AEGIS shield drones** (the mission's scouts, renamed). The
  director (`WireFlagshipFight`) keeps `SetInvulnerable(true)` until both drones die, then
  drops the matrix with a SCIENCE comms beat ("She's vulnerable — hit her with everything!").
  Scanning the shielded flagship calls the tactic out instead of the M26 armor line.
- **Skirmish mode** (`SKIRMISH` on the main menu): endless waves at Ember — the GI flag
  routes the director into wave mode at world start (no objectives/events/contracts), parks
  the ship off the sun, and reuses the campaign spawner per wave. Composition scales
  (scouts → gunships → cruisers, cap 6; 6 s engage grace), each clear pays a rising bonus
  (25 cr × wave) + comms, next wave in 12 s (`WaveInterval`). The web footer becomes a
  **wave HUD**: "● WAVE N — REPEL THE ATTACK" / "◇ WAVE N+1 INBOUND" (`wave` in /api/state).
  Nothing is saved; defeat + RESTART rebuilds wave 1. NEW GAME/CONTINUE clear the flag.
- **Difficulty selector** (`EDifficulty`, persisted in the save): a second New Game step
  after the ship pick — ENSIGN (enemy damage ×0.7, hull ×0.75), CAPTAIN (baseline), ADMIRAL
  (×1.4 / ×1.3) — applied in `ApplyTypePreset` to every spawned hostile.

**Verified (PIE + /api):** [L] flagship phase exact — 1 000 raw damage (beam + shield-bypass
torpedo) left LEVIATHAN-1 at 220/220 hull, 110 shield; scan fired the drone callout; killing
AEGIS-1/2 fired the collapse beat and the next 30 damage landed (shield 110→80). [L] skirmish
scripted run cleared waves 1–3 (compositions 1/2/3 ships, credits 80→145→315→550 with kill +
bonus math exact) and wave 4 (5 ships) spawned live; a wave-4 fleet then actually killed the
idle test ship — defeat screen took over the footer as designed. Admiral scaling exact on a
live fleet (scout 65/26 hull/shield + 7.0 beam; cruiser 286/143 + 19.6). [S] `wave_hud2.png`:
the weapons console footer reading "● WAVE 1 — REPEL THE ATTACK". (Commit:
Core/{StationTypes,SpaceSaveGame,SpaceGameInstance{,.cpp},MissionSubsystem{,.cpp},
MainMenuWidget{,.cpp}}.h/.cpp + Ships/EnemyShip.{h,cpp} + Components/ScienceComponent.cpp +
Net/StationServerSubsystem.cpp + docs.)

---

## 2026-07-09 — 🔒 R5(a) Web console access PIN

The LAN control surface is no longer wide open: every HTTP route now demands the session PIN.
- **4-digit session PIN** (`UStationServerSubsystem::GetSessionPin`), generated once per game
  process. The crew URL everywhere it appears (main menu, pause overlay, startup log) becomes
  `http://<ip>:8080/stations?pin=XXXX` — typing it as shown is all the crew needs.
- **Gate on every route** (R5's ship-blocker): the `Bind`/`BindStation` helpers wrap each
  handler with a PIN check. `/api/*` without the right pin gets an honest
  `{ok:false, reason:"invalid pin …"}`; pages get a "CREW ACCESS LOCKED" hint page. This
  also kills cross-site GETs from malicious pages on the LAN (CSRF against local services).
- **Pages propagate the PIN** client-side: JS reads it from `location.search`, appends it to
  every fetch (`post()`, state poll, starmap, contract board) and rewrites the station /
  header links, so navigation keeps working without server-side templating.
- Note: the `/` → `/stations` redirect (smoke-test #5) is not implementable on UE's HTTP
  router — `FHttpPath::IsValidPath` rejects the bare root path (pre-existing limitation,
  documented where the routes bind). Settings toggle (server on/off, loopback-only) still open.

**Verified (PIE + curl):** no pin and wrong pin → JSON rejection on `/api/state`, LOCKED page
on `/stations`; correct pin (from the startup log URL) → full state JSON, the stations hub,
and a mutating action (`/api/alert?state=red&pin=…` → `{ok:true}`); the served helm page
contains the PIN-appending JS at all four fetch sites. (Commit: Net/StationServerSubsystem.{h,cpp}
+ docs.)

---

## 2026-07-10 — 🔍 Full gameplay audit: six logic holes closed

A systematic read of every gameplay-relevant source file (ships, components, director,
controller, game instance, web server) hunting for exploits and logic holes. Six confirmed,
all fixed:

1. **Dead ship could dock — and resurrect.** `CanDock()` never checked the ship was alive,
   and `Dock()`'s `ResetPools()` refills hull *and clears the death latch*: pressing F beside
   the starbase during the defeat beat un-killed the wreck. `CanDock()` now requires a live
   hull.
2. **Dead ship could warp.** `IsWarpReady()` only checked docked+charge; the R/G hotkeys
   teleported the wreck around during the defeat sequence, and warp charge kept building
   after death. Alive-gated (readiness + charge accrual); moved `IsWarpReady` to the .cpp.
3. **Docked-invulnerability weapons exploit.** A docked ship is invulnerable and
   auto-repaired, but beams/torpedoes had no docked gate — lure a chaser (bounty leash
   15 000 uu, distress events spawn near home) to the starbase, dock, snipe with impunity.
   `FireBeam()`, torpedo `Fire()` and `IsReady()` now refuse while docked; `/api/weapons`
   returns `weapons offline while docked` and the weapons console buttons read
   "DOCKED — OFFLINE".
4. **Skirmish leaked into the campaign save.** ESC → SAVE and drydock buys called
   `SaveCampaign()` during skirmish, persisting wave-farmed credits/XP into the campaign.
   `SaveCampaign()` now no-ops (returns false, logged) while `bSkirmish` — skirmish is
   genuinely session-only.
5. **Skirmish wave spawned onto the defeat screen.** A wave timer pending when the player
   died still fired, spawning ships + "WAVE N inbound" comms over the DEFEAT overlay.
   `SpawnWave()` bails when the player is dead.
6. **The wreck ran errands.** Contract waypoints/deliveries and salvage-pod collection kept
   progressing (with payout comms) while the ship was destroyed. `CheckContract()` and the
   salvage branch of `CheckEvent()` are alive-gated; the contract itself survives in the
   save for the retry.

Also removed the dead `FMissionDef::bAutoLaunch` field (set by the tutorial, read nowhere
since staging was removed in M23 — the home zone's TriggerRadius covers the player start).

**Verified [L] (PIE via MCP):** parked at the station warp-ready (`charge=1.00
warpReady=True canDock=True`), killed the ship (99999 bypass) → `canDock=False dockOk=False
warpReady=False warpOk=False movedBy=0uu`, hull stays 0.0, not docked. Docked with a locked
live target at full charge → `beam=False torpedo=False torpReady=False` with both "Fire
blocked — weapons offline while docked" log lines; after undock + facing the target the same
beam fired (`beam=True`, charge 1.00→0.00). `SaveCampaign` → `normal=True skirmish=False`.
**[S] (curl, live PIN):** `/api/dock?action=dock` → `{ok:true}`, then `/api/weapons?action=
fire` and `?action=torpedo` both → `{"ok":false,"reason":"weapons offline while docked"}`,
state showed `"docked":true`, undock → `{ok:true}`. (Commit: Ships/Spaceship.{h,cpp} +
Components/{WeaponComponent,TorpedoLauncherComponent}.cpp + Core/{MissionSubsystem{.h,.cpp},
SpaceGameInstance.cpp} + Net/StationServerSubsystem.cpp + docs.)

## 2026-07-11 — 🛠️ justfile: one-command standalone packaging (Linux/Win/Mac)

Added a `justfile` wrapping `RunUAT.sh BuildCookRun` into three recipes — `package-linux`,
`package-windows`, `package-mac` — plus `run-linux` and `clean`. Config is overridable
(`config=Development just package-linux`); Shipping is the default, cooking MainMenu +
VSlice_Arena. Header documents the cross-compile reality (Linux host → Linux only; Windows →
Win+Linux; macOS → Mac only) so the Win/Mac recipes are labelled run-on-that-host. Editor-only
plugins (VibeUE/UnrealClaude/ModelingToolsEditorMode, all `TargetAllowList=Editor`) drop out
of packages automatically; `Build.cs` already lists `HTTPServer/Sockets/Networking`, so the LAN
server cooks into Shipping. `Packaged*/` is gitignored — nothing build-output committed.

**Verified [L] (real package + run, this Linux box):** `just package-linux` (Shipping) →
`BUILD SUCCESSFUL`, `AutomationTool exiting with ExitCode=0`, cook clean (0 errors, 1 warning =
the editor-only UnrealClaude cook-commandlet tag, absent from the game). Staged
`Packaged/Linux/SpaceGame.sh` + `SpaceGame-Linux-Shipping` (160 MB). Launched the packaged
binary → LAN server bound `:8080` in ~2 s. **[S] (curl against the shipped build):** `GET /` →
`301 → /stations`, `GET /stations` → `HTTP 200` (262 B), `GET /api/state` → `HTTP 200
{"ok":false,"reason":"invalid pin ..."}` (R5(a) PIN gate live in Shipping). No fatal/assert in
the boot log; process shut down cleanly, port down. (Commit: justfile + docs.)

## 2026-07-11 — ⚡ Performance: PSO precaching + drop the per-frame collision actor scans

Chasing reported stutter when turning the ship fast. Two independent causes:

1. **Render-side (the stutter itself).** No PSO/pipeline caching was configured, so an
   off-screen mesh/material rotating into the frustum had its graphics pipeline compiled
   *inline on the render thread* the first time it drew — a classic turn-hitch, worst on the
   Vulkan build. Added a `[SystemSettings]` block to `DefaultEngine.ini`: `r.PSOPrecaching=1`,
   `r.PSOPrecache.Components=1`, `r.PSOPrecache.Resources=1`, and
   `r.PSOPrecache.ProxyCreationWhenPSOReady=1` (defer a mesh's first draw until its PSO is
   ready — a frame-late pop-in instead of a frame stall).
2. **CPU headroom.** `ASpaceship::HandleCollisions()` ran *every tick* and did two full-world
   `GetAllActorsOfClass` sweeps (all enemies + all landmarks). Landmarks are spawned once at
   begin play and never change; enemies come and go slowly. Now the candidate lists are cached
   as weak pointers and refreshed on a 0.25 s interval (`CollisionScanInterval`), while live
   positions are still read off the cached pointers every frame — so ram detection is unchanged
   but two per-frame world scans leave the hot path.

**Verified [L]:** editor target compiles clean (13.4 s, Spaceship.cpp rebuilt); `just
package-linux` (Shipping) → `BUILD SUCCESSFUL`, `ExitCode=0`; the repackaged binary boots and
the LAN server binds `:8080` (~3 s), no fatal in the log, clean shutdown. **Not verifiable from
here:** the *felt* smoothness — frame hitches can't be observed over the LAN API, so the
turning improvement needs an in-game re-test. If precaching alone isn't enough, the definitive
fix is a bundled PSO cache recorded from a playthrough (`r.ShaderPipelineCache`), a follow-up.
(Commit: Config/DefaultEngine.ini + Ships/Spaceship.{h,cpp} + docs.)

## 2026-07-11 — 🎛️ Settings: VSYNC + MAX FRAMERATE options, and smooth-by-default

Profiling (see prior entry's method) proved the "lag" was never workload: offscreen 1080p and
windowed 720p both ran ~600 fps with zero real hitches (89 draw calls, 45k prims). The stutter
was isolated to the **fullscreen present path** — on a 60 Hz display the engine floods the
compositor with ~10× the frames it can show, so the vsync'd present beats irregularly (KDE
Wayland / XWayland / RADV). Capping to the refresh + vsync fixes it (user-confirmed live via
`t.MaxFPS 60`).

- **Two new SETTINGS rows** in `USettingsMenuWidget`: **VSYNC** (on/off) and **MAX FRAMERATE**
  (60/120/144/240/UNLIMITED/30), wired exactly like the existing rows — `SetVSyncEnabled` /
  `SetFrameRateLimit` → `ApplySettings(false)` → `SaveSettings()`. (Confirmed the pre-existing
  WINDOW MODE / RESOLUTION / QUALITY / VOLUME rows are all real `UGameUserSettings` calls too;
  QUALITY just doesn't move fps because nothing is GPU-bound, and VOLUME only bites with audio.)
- **Smooth by default:** `USettingsMenuWidget::SeedVideoDefaults()` (called from
  `USpaceGameInstance::Init`) seeds vsync on + a 60 FPS cap on first launch, guarded by a one-shot
  `[SpaceGame.Video] DefaultsSeeded` flag so it never overrides a later player choice. (A
  `Config/DefaultGameUserSettings.ini` seed was tried first but doesn't apply in cooked builds —
  the runtime file came back without the keys — so the reliable code path replaced it.)

**Verified [L]:** editor compiles clean (13.4 s). Fresh Development package, Saved config wiped →
first launch wrote `bUseVSync=True` + `FrameRateLimit=60.000000`, and the CSV capture confirms the
cap holds: **16.80 ms / 59.5 fps** steady (was ~600 fps uncapped), game/render/GPU at 3.1/5.4/2.3
ms with headroom to spare. Fullscreen judder itself can't be measured headlessly — that needs an
in-game re-test on the 60 Hz display. (Commit: Core/SettingsMenuWidget.{h,cpp} +
Core/SpaceGameInstance.cpp + docs.)

## 2026-07-15 — ✅ FIX: damage beep only when the hull is critical

The bridge alarm beeped on **every** subsystem knockout — `UDamageControlComponent::DamageSystem`
fired `S_Alarm` unconditionally, and since `HandleOwnerDamaged` rolls `DamageChance` (0.35) on every
unshielded hull hit, routine chip damage at 90% hull beeped just as loudly as a real emergency. It
read as "beeping on damage" and was pure noise.

- **Gated the beep on hull state:** new `IsHullCritical()` helper reads the sibling
  `UHealthComponent` and compares hull fraction against a new `AlarmHullFraction` tunable
  (EditAnywhere, default **0.3** — deliberately matching `ASpaceship::LowHullFraction`, the
  threshold the looping low-hull klaxon already uses). The alarm now plays only at/below it.
- **Healthy hull:** a knocked-out system still logs and flags amber on the Engineering console, the
  web page, and `/api/state` — it just doesn't beep. The information is still there, visually.
- **Critical hull:** the beep returns, punctuating the continuous klaxon that's already running down
  there, so audio escalates only when the situation genuinely warrants it.

Untouched: the per-hit `S_Hit` impact thud, the one-shot red-alert sting, and the low-hull klaxon
loop itself. Enemies never had a `DamageControlComponent` (`AEnemyShip` is a plain `APawn`), so this
alarm was always player-only — the non-spatial `PlaySound2D` was never an enemy leaking into the mix.

**Verified [L]:** editor compiles clean (13.8 s, `Result: Succeeded`).

## 2026-07-15 — ✅ FIX: ship SM5 alongside SM6 so the build runs on non-SM6 GPUs

The v0.9.0 Linux release died on a tester's machine with **"Vulkan device could not be created at
the project's supported feature levels"**. Cause: `Config/DefaultEngine.ini` dropped the engine's
default Linux RHI and cooked **SM6 only**:

```
[/Script/LinuxTargetPlatform.LinuxTargetSettings]
-TargetedRHIs=SF_VULKAN_SM5     ; removed the BaseEngine.ini default
+TargetedRHIs=SF_VULKAN_SM6
```

`LinuxDynamicRHI.cpp::PlatformCreateDynamicRHI` reads `TargetedRHIs`, sorts, and walks from the
highest feature level down calling `IsSupported()`, logging `Skipping <format>...` on each miss. With
SM6 as the only entry, a GPU/driver without SM6 has nothing to fall back to → `bVulkanSMFailed` →
that error. It never showed up here because this box is an RX 9070 XT on Mesa 26.1 / Vulkan 1.4.

- **Fix:** target **both** `SF_VULKAN_SM5` + `SF_VULKAN_SM6`. SM6 is still preferred where supported;
  SM5 is the fallback. Verified both shader sets are now cooked into the pak (was SM6-only).
- **Why it matters:** the SM5 path negotiates **Vulkan API 1.1**, where SM6 demands **1.3** — that
  gap is precisely what older drivers fail on.

**Verified [L]:** `-sm5` (a real RHI force via `LinuxDynamicRHI.cpp`, not just the NullRHI path)
boots as `SF_VULKAN_SM5`, initializes the engine, loads `VSlice_Arena`, and runs 987 frames with
**0** missing-shader/fatal errors, at `Using API Version 1.1`. Unforced still selects SM6 (no
regression). Package re-scanned: no `VibeUEApiKey`, no credential-format strings.

---

## Public-repo hygiene + license (2026-07-16)

Repo went public. Cleanup pass so nothing machine-specific or secret ships:
- **Secret:** blanked the committed AndroidFileServer `SecurityToken` in `Config/DefaultEngine.ini`
  (auto-generated dev token, `bIncludeInShipping=False` — never shipped; editor regenerates on demand).
- **Paths / username:** replaced hardcoded `/home/julian/...` and the `julian-gaming-cachy` hostname
  across `README.md`, `justfile` (now `UE_ROOT`-overridable, defaults to `$HOME/UnrealEngine/UE_5.7`),
  `tools/_import_*.py` (anchored to the repo via `__file__`), and `docs/*`.
- **License:** added `LICENSE` — **PolyForm Noncommercial License 1.0.0** (noncommercial use allowed,
  commercial use forbidden) + a README License section.

**History scan:** searched all 90 commits for credential formats (`sk-ant-`/`ghp_`/`github_pat_`/
`AKIA…`/PRIVATE KEY) and for any committed `VibeUEApiKey=`/`ApiKey=` value — **none found**. The only
secret ever in history is the AndroidFileServer token above, present since the baseline commit
`deb059d`; it's inert (dev-only, unshipped) so history was left intact rather than force-rewritten.

---

## Session recording ("analysis mode") (2026-07-16)

New opt-in telemetry recorder so played sessions can be analysed offline to tune the game against
real play data (feeds the GitHub issue work).

- **`USessionRecorderSubsystem`** (`Core/SessionRecorderSubsystem.{h,cpp}`) — a `UTickableWorldSubsystem`
  (Game/PIE worlds only). Off by default; armed by CVar `sg.RecordSession 1` (settable live from the
  console) or the `-recordsession` launch flag. Samples ship + mission + campaign state every
  `sg.RecordSampleInterval` s (default 0.5 = 2 Hz) to `Saved/SessionLogs/session_<ts>.jsonl`.
- **Format:** one JSON object per line, tagged by kind — `meta` header, `s` samples (pos/heading/speed,
  hull/shield, target/ammo, mission/objective/engaged/objDist, enemies alive, credits/xp/rank), `e`
  events **derived by diffing successive samples** (damage, death, kill, engage, mission-advance,
  victory/defeat, dock/undock, warp — so gameplay code needs no hooks), and an `end` footer. Per-line
  flush → a crash still leaves a valid, parseable log.
- **`tools/analyse_session.py`** — prints a per-session digest (duration, distance flown, hull damage
  taken, event tallies, per-mission timings, outcome); `--json` for a machine digest; newest log by default.

**Verified [L]:** built clean (editor target, `Result: Succeeded`). Ran the arena headless
(`-game -nullrhi -recordsession`, no window) ~27 s → wrote a 113-line JSONL: correct `meta`
(v0.9.1 / VSlice_Arena / Captain), 111 well-formed samples with live hull/shield/mission/objDist, and
a correctly-derived `engage` event when a fleet spawned. `timeout`-killed mid-run (no `end` line) yet
the file parsed fine and `analyse_session.py` produced a clean report — crash-resilience confirmed.

---

## Crew web server: auto-pick a free port (2026-07-16)

The LAN station server hard-bound `0.0.0.0:8080`; if anything else already held 8080 (another dev
server on the host, a lingering previous session) it silently failed to bind and the consoles were
unreachable. Now `UStationServerSubsystem` probes 8080..8087 at begin play (plain exclusive bind, no
address reuse, so an existing listener reads as busy) and uses the first free port. The chosen port is
mirrored in a static so `GetCrewUrl()` / the logged crew URL advertise the right one.

**Verified [L]:** headless run with 8080 occupied → log `port 8080 busy — trying next` → bound 8081
(and 8082 when 8081 also lingered); the API answered `{"ok":true}` on the chosen port.

## Reverse thrust — issue #4 (2026-07-16)

"How the fuck do i move away from bullets and fire back?" — the ship could only go forward, so you had
to turn away (and lose your guns) to retreat. Throttle may now go negative down to
`ReverseThrottleMin` (default −0.35, a maneuvering burn capped well short of full ahead). The Tick's
translation is already `forward * CurrentSpeed`, so a negative speed simply backs the ship straight up
while it keeps facing — and firing at — the enemy.
- `UShipMovementComponent::SetThrottle` clamps to `[ReverseThrottleMin, 1]` (was `[0,1]`).
- Keyboard: `S` steps past a full stop into reverse; `W` brings it forward through zero.
- Web Helm: the throttle slider now runs `-35..100` and is labelled "(◄ reverse)".

**Verified [L]:** headless arena, commanded `throttle=-0.3` over the API → recorder track shows the ship
holding **−630 uu/s** and travelling from px 0 → −10923 (backward along its +X heading) while heading
stayed 0. Forward (throttle 0.5) unaffected.

---

## Torpedo turn-rate limit — issue #2 (2026-07-16)

Torpedoes were *perfect* homers — every tick they re-aimed straight at the target, so they could snap
180° and always connect (and chase you through a warp jump). Now `ATorpedoProjectile` carries a
`Heading` steered toward the target at `TurnRateDeg` (75°/s): it can gently track a maneuvering ship
but can't reverse fast enough to re-acquire a target that warps or hard-jukes. A missed torpedo flies
straight on and fizzles at `MaxLifetime` (`Detonate` only lands the payload within `BlastRadius`).
Applies to both player and enemy torpedoes → skill shots, not guaranteed hits. **Verified:** builds clean.

## Evasive strafe / side-thrusters — issue #7 (2026-07-16)

Added lateral thrust so the helm can dodge without turning the bow off-target. `UShipMovementComponent`
gains `SetStrafe(-1..1)` → eased `CurrentStrafeSpeed` (cap `MaxStrafeSpeed` 950 uu/s, scaled by engine
power/damage) translated along the ship's right vector. Controls: keyboard **Q/E** (held), and web Helm
**STRAFE ◄◄ PORT / STBD ►►** hold-buttons (`/api/helm?strafe=`).
**Verified [L]:** commanded `strafe=1` headless → recorded track shows **py 0 → 11299 at 950 uu/s** while
heading held **0.0°** and px stayed ~236 — a pure side-slip.

## Enemies avoid you + clear friend/foe blips — issue #9 (2026-07-16)

- **Avoidance:** `AEnemyShip` now steers straight out when it encroaches within `MinSeparation` (1300 uu,
  well above the ram-collision radius), harder the closer it gets — moving ships veer around the hull
  instead of boring through it. Ram contact becomes the exception, not the rule.
- **Friend/foe clarity:** the Scout's radar blip was **cyan** (read as friendly). Every hostile now blips
  in the warm red/orange band (Scout → amber-orange), unmistakable against the green starbase and amber
  cargo pods. **Verified:** builds clean.

---

## Story pacing + ACCEPT ORDERS + nav-map — issue #8 (2026-07-16)

The campaign fired too fast and you could blunder into a fight just by flying. Reworked the open-sector
director so encounters are deliberate and the map/story are legible.

- **ACCEPT ORDERS gate.** Reaching an objective no longer auto-spawns its fleet. The director now
  *offers* the objective on arrival (`OfferObjective` → a "you've reached X, orders are yours, ACCEPT
  when ready" hail) and holds; `AcceptObjective()` opens the fight. Controls: web **ACCEPT ORDERS**
  button on Helm + Science (shown only while `offered && !engaged`), `/api/mission?action=accept`, and
  the solo **Y** key. `IsObjectiveOffered()` + `offered` exposed on `/api/state` and `/api/starmap`.
- **Wider spacing.** `SectorSpan` 120k → 160k, so systems sit farther apart — more room to fly/warp and
  no drifting into the next zone.
- **Reactive comms.** DAMAGE CONTROL calls out a critical hull (≤30%) mid-fight (one-shot) — the story
  reacts to how the battle is going, not just a fixed timeline. Plus the arrival hail + engage beat.
- **Nav-map (Science).** `/api/starmap` now reports each system's real planar range + the offered state;
  the map draws a **distance label under every system** and an **ORDERS PENDING** objective readout.
  Helm/Science also show the objective range + ORDERS PENDING inline.

**Verified [L]:** headless arena — `Objective 0 OFFERED — awaiting ACCEPT ORDERS` fired at spawn with
**no** encounter; `/api/state` read `offered:true, engaged:false`; `POST /api/mission?action=accept`
→ `{"ok":true}` → `engaged:true` (fleet spawned). Landmarks spread at span 160000.

---

## Celestial gravity + toggles + analysis buttons — issues #1/#3 (2026-07-16)

Gentle gravity that pulls the player *and* enemies toward planets/suns (per wz-stabl on #3: affects
the player, only a slight pull, no damage, always escapable with moderate thrust).

- **`AWorldLandmark`** gains a mass-driven well: `GravityMass` (the "predefined mass parameter" of #1,
  set from kind+scale — a sun ~8× a planet), sizing `InfluenceRadius` and `PeakPull` (capped 420 uu/s,
  far under ~1800 uu/s thrust, easing quadratically to 0 at the influence edge).
- **`World/GravityField`** sums every well into a planar drift velocity; applied each tick to the
  player (`UShipMovementComponent`, suppressed while docked) and enemies (`AEnemyShip`, weak enough
  they never get trapped).
- **Toggleable:** CVar `sg.Gravity` (default on). Web Helm **GRAVITY** button flips it live
  (`/api/toggle?what=gravity`) and shows the current pull; `/api/state` exposes `gravity`/`gravityPull`.
- **Analysis logging button:** web Helm **REC LOG** button toggles session recording
  (`/api/toggle?what=recording` → `sg.RecordSession`); `/api/state` exposes `recording`.

**Verified [L]:** `gravityPull` reads 78–94 uu/s near the home world; fly out + release → the ship
drifts back (px 7773 → 7204, pull rising 78→87 as it nears) and thrust escapes freely; GRAVITY toggle
→ pull 0; REC LOG toggle → recording on.

---

## Drydock modules: Auto-Turret + Maneuvering Thrusters — issues #5, #7 (2026-07-16)

Two one-time drydock purchases (MaxTier 1) the starter hull doesn't carry, added to `UpgradeCatalogue`
so they appear in the Engineering drydock automatically:

- **Auto-Turret (#5):** `UWeaponComponent` gains a turret that auto-fires at the Weapons-locked target
  on its own cooldown **regardless of the ship's heading** (no arc constraint), within `TurretRange` —
  so the helm can dodge/reposition while it keeps shooting. `TurretDamage` 0 = not installed; the
  "Auto-Turret" module sets it. Offline while docked; respects weapon power + armor mitigation.
- **Maneuvering Thrusters (#7 follow-up):** per wz-stabl, strafe is no longer free — `MaxStrafeSpeed`
  now defaults to **0** (Q/E and the web STRAFE buttons do nothing) until the "Maneuvering Thrusters"
  module is bought, which sets it to 950 uu/s.

Both are wired through the existing `ApplyShipPreset` reset + `ApplyUpgrades` path (idempotent across
drydock refreshes / ship switches).

**Verified [L]:** `/api/state` lists both modules in the drydock (strafe 160cr, turret 240cr, maxTier 1);
with gravity isolated off, commanding strafe on an un-upgraded ship moves it **0.0 uu** — correctly
disabled until purchased.

---

## PR #11 codex-review fixes (2026-07-16)

Addressed all three findings from codex's review of the gravity/modules branch:
- **[P1] Gravity surface-pin.** Changed the well falloff to a band profile (`4·d·(1-d)`) — pull is now
  **0 at the body surface** (and at the influence edge), peaking mid-well. A ship resting on a body is
  no longer dragged back in each frame while collision zeroes its throttle. Verified: pull peaks ~190
  mid-well, tapers to ~175 as the ship nears the surface, and still pulls a released ship back.
- **[P1] Turret firing after death.** `UWeaponComponent` turret tick now gates on the owner's
  `HealthComponent::IsAlive()`, so a destroyed ship can't auto-fire during the defeat beat (which could
  award a kill / flip Defeat to Victory).
- **[P2] Enemies buried in planets.** Added `GravityField::ClampOutsideBodies`; `AEnemyShip` clamps
  itself out of the collision-less landmark meshes each tick (enemies had no body clearance of their own).

**#6 orbit button deferred:** built an orbit-assist autopilot but it spiralled outward while reporting
"stable" (radius not held). Reverted it from this PR rather than ship a broken control; it's a clean
follow-up (the `GravityField::DominantBody` groundwork stays for it).
