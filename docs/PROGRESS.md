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
