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
