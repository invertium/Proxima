# OPUS BRIEF — execution instructions for the next agent

You are continuing a working game, not starting one. Read this file fully, then skim
`README.md` (build/run), `docs/RELEASE_PLAN.md` (release chores + smoke-test punch list) and
`docs/PROJECT_PLAN.md` Phase 6 (gameplay milestones M24–M30). `docs/PROGRESS.md` is the
append-only log — the last ~10 entries tell you exactly how this project verifies work.

**The game:** a spaceship bridge simulator (Artemis/EmptyEpsilon style), UE 5.7.4 C++ on
Linux. One continuous open sector, 4-mission campaign, crew stations playable solo on
keyboard AND as LAN web consoles (port 8080 while playing). It builds clean and the full
loop works; your job is the execution queue below.

## Ground rules (non-negotiable)

1. **One commit per work item**, message style `M24: <what changed>` / `R1: <what changed>`,
   and append a terse entry to `docs/PROGRESS.md` in the existing style (what/verified/gotchas).
2. **Verify before you tick.** Every item has a done-when; prove it in PIE (log lines,
   `/api/state` JSON, screenshots) before committing. Never claim unverified work.
3. **NEVER commit anything under `Saved/`** — `Saved/Config/LinuxEditor/EditorPerProjectUserSettings.ini`
   holds the VibeUE API key. It is gitignored; do not force-add, do not echo the key.
4. **Don't regress the campaign paths**: defeat→retry, save/load, drydock buy/sell, web
   consoles. When touching the director/controller, re-run the mission-1 flow after.
5. **Small diffs.** Match existing code style (comment density, naming). Data-driven content
   lives in `UMissionSubsystem::BuildCampaign()` — extend patterns, don't invent frameworks.
6. Editor GUI actions you cannot do (drag-drop imports, plugin settings dialogs): stop and
   ask the user rather than working around with asset imports via MCP (see gotcha list).

## The dev loop (exact commands — these are hard-won, don't improvise)

```sh
# 0. Editor must be CLOSED for builds. Check (pgrep -f self-matches; use this):
ps -eo comm | grep -i UnrealEditor
# kill if needed: kill <pids from pgrep -x UnrealEditor>, then kill -9 stragglers.

# 1. Build:
~/UnrealEngine/UE_5.7/Engine/Build/BatchFiles/Linux/Build.sh \
  SpaceGameEditor Linux Development \
  -Project=~/spaceGame/SpaceGame.uproject -WaitMutex

# 2. Launch editor (background), then poll VibeUE MCP until ready (~60-120s):
nohup ~/UnrealEngine/UE_5.7/Engine/Binaries/Linux/UnrealEditor \
  ~/spaceGame/SpaceGame.uproject > /tmp/spacegame_editor.log 2>&1 &
# poll http://127.0.0.1:8088/mcp until it answers.

# 3. Drive the editor with Python: write a .py file, then
python3 tools/vibe_py.py yourscript.py
# Load the level (load_level SILENTLY NO-OPS — use this):
#   unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/VSlice_Arena")
# Start/stop PIE:
#   unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play() / _end_play()
# Game world/pawn in PIE:
#   w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
#   pawn = unreal.GameplayStatics.get_player_controller(w,0).get_controlled_pawn()

# 4. Probe the running game over HTTP (best signal, no Python needed):
curl -s http://127.0.0.1:8080/api/state        # mission/objective/hull/target/credits/engaged
curl -s http://127.0.0.1:8080/api/starmap      # nav map data
curl -s "http://127.0.0.1:8080/api/warp?mode=objective"   # and the other /api/* actions

# 5. Screenshots:
# 3D PIE view (works headless):
#   unreal.AutomationLibrary.take_high_res_screenshot(1280, 720, "/abs/path.png")  # poll for file
# UMG/HUD (in-engine capture CANNOT see Slate): foreground the editor then OS-capture:
#   tools/os_foreground.sh && tools/os_capture.sh out.png    # spectacle; use -a if -f grabs white
# Web console pages: curl the /api JSON, or headless-browser screenshot (see PROGRESS M23 entries).
```

**Shell is POSIX (not fish):** `for i in $(seq 1 60); do ...; done`.

## Gotchas that have burned this project before (all verified, all in PROGRESS/memory)

- **Do NOT import assets (FBX/audio/textures) via MCP** — Interchange crashes the editor.
  If an asset import is truly needed, ask the user to do it in the editor GUI.
- **No actor edits/saves immediately after `editor_request_end_play`** — teardown segfaults.
  Let it settle (few seconds) or restart the editor before level edits.
- **`EditorAssetLibrary.save_asset` wants the PACKAGE path** (`/Game/X/M_Foo`), not the object
  path (`/Game/X/M_Foo.M_Foo`); verify the `.uasset` exists on disk before closing the editor.
- Materials are authored via `unreal.MaterialEditingLibrary` (see `M_Planet` in PROGRESS
  M23.4 for the working recipe). The default input pin name on connects is `""`.
- Duplicate editors = dead MCP. Before relaunching, confirm no UnrealEditor process and
  ports 8088/3000 are free.
- Chatty logs are normal; grep editor logs for `Fatal|Assertion|Ensure|Accessed None` as the
  health check.

## Execution queue (in this order)

**Q1 — M24 Combat readability & death polish** (PROJECT_PLAN Phase 6). Small, high-value,
all in C++ you already have: `BridgePlayerController` (defeat flow), `EnemyShip` (stand-down
+ grace), `WeaponComponent` (clear dead target), `Spaceship::WarpToObjective` (standoff).
One commit.

**Q2 — API honesty + web fixes** (RELEASE_PLAN R5, smoke-test #3/#5): all `/api/*` actions
return `{ok:false, reason}` on rejection and are gated on game phase; redirect `/` →
`/stations`. Verify by curling the rejection cases the smoke test listed (fire while dead,
buy undocked, broke). One commit.

**Q3 — R1 Packaging** (RELEASE_PLAN R1 — read it; it has the full checklist). Highlights:
`TargetAllowList: ["Editor"]` on UnrealClaude + VibeUE in `SpaceGame.uproject`; BuildCookRun
Linux Development → fix cook fallout (string-path-loaded WBP/M_ assets, maps list); confirm
the :8080 server runs packaged; then Shipping; fill `DefaultGame.ini` project identity.
Gate: packaged build plays mission 1 and serves consoles. Commit per sub-step.

**Q4 — R2 First-run UX** (RELEASE_PLAN R2): settings menu (window/volume/quality), crew URL
shown in-game, help/key overlay, quit confirms.

**Q5 — Gameplay milestones M25 → M30 in order** (PROJECT_PLAN Phase 6). One commit +
PROGRESS entry each, PIE-verified against each done-when. These are the "better gameplay
loop": subsystem damage (M25), enemy behaviors (M26), travel events (M27), contracts (M28),
red alert (M29), final battle + skirmish + difficulty (M30).

**Q6 — Remaining release chores** (RELEASE_PLAN R3 leftovers: music/UI audio — needs user
for imports; R5 PIN; R6 credits/licences/distribution). Coordinate with the user on anything
involving asset imports, store accounts, or the Windows build.

## When to stop and ask the user

- Anything destructive (deleting content, rewriting saves' format without a version bump).
- Design forks the plans don't answer (e.g. paid vs free framing, game title).
- GUI-only editor actions and asset imports (Interchange crash class).
- Packaging blocked on platform/tooling you don't have (Windows machine, store accounts).

Everything else: decide, do, verify, commit, log.
