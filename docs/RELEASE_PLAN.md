# RELEASE PLAN — from vertical slice to a shippable game

Written 2026-07-06 after a full project review (source, content, configs, progress log) plus a
runtime smoke test. Companion to PROJECT_PLAN.md (which tracked the slice milestones M1–M23);
this file tracks the road to a public release. Same cadence: one commit per checkbox where
practical, verified in PIE or a packaged build before ticking.

---

## Where the game stands today

**What exists and works (verified through M23.5):**
- Complete core loop: 4-mission open-sector campaign (Crimson Pact arc) with proximity-triggered
  encounters, seamless clears, epilogue, defeat/retry.
- Five crew stations: Helm, Weapons (beams + torpedoes), Engineering (power + repair minigame),
  Science (scan + story comms) — playable solo on one keyboard AND as LAN web consoles
  (browser stations on port 8080, canvas radar + nav map).
- Progression: credits/XP/rank, drydock upgrades, buyable ships, SaveGame persistence.
- Presentation: real ship meshes (Quaternius CC0), Earth/planet/sun materials, nebula+starfield
  backdrop, beam/explosion/debris FX, camera shake, 6 SFX + engine hum + hull alarm.
- Menus: main menu map, pause overlay, outcome screens.

**The hard truth:** the game has only ever run inside the editor (PIE). It has never been
cooked, packaged, or launched as a standalone build. That gap — not gameplay — is the biggest
release risk, so it comes first.

---

## R1 — Standalone packaged build (highest risk, do first)

Nothing else matters if the game can't run outside the editor.

- [ ] **Fence off the dev plugins.** `SpaceGame.uproject` enables UnrealClaude and VibeUE
  unconditionally — both are editor/MCP tooling and must not ship. Add
  `"TargetAllowList": ["Editor"]` to both entries (ModelingToolsEditorMode already has it).
  Verify the game module has no compile-time dependency on either.
- [ ] **First cook + package (Linux, Development config first):**
  `RunUAT.sh BuildCookRun -project=… -platform=Linux -clientconfig=Development -cook -stage -pak -build`.
  Expect and fix the usual first-cook fallout:
  - Everything loaded **by string path** must survive cooking — `LoadClass` of WBP_* widgets,
    `StaticLoadObject` of M_Earth/M_Planet/M_GlowOrange/M_Imperial etc., audio assets. Add
    `Content/` dirs to `DirectoriesToAlwaysCook` in DefaultGame.ini (or reference the assets
    hard) so the cooker doesn't strip them.
  - Both maps (MainMenu, VSlice_Arena) in the maps-to-cook list.
- [ ] **Verify the web-console server runs in a packaged build.** `UStationServerSubsystem`
  uses the engine `HTTPServer` module — confirm the module is a runtime dependency in
  Build.cs and that the listener starts outside the editor. This is a headline feature;
  it must work packaged.
- [ ] **Shipping config build** once Development packs cleanly (Shipping strips logging and
  ensures — check nothing load-bearing lives behind `UE_LOG`-adjacent debug paths, e.g. the
  debug `/api/game?action=launch` endpoint should be compiled out or key-gated).
- [ ] **Project identity:** DefaultGame.ini is literally empty. Set ProjectName, ProjectVersion
  (start 0.9.0), CopyrightNotice, ProjectDisplayedTitle; add an app icon + splash; decide the
  real game title ("SpaceGame" is a placeholder — pick the shipping name early, it touches
  icon, window title, save dir, and store pages).
- [ ] **Gate:** on a machine (or clean user account) without the editor, the packaged build
  boots to the main menu, plays mission 1, saves, and serves working crew consoles to a phone
  on the same LAN.

## R2 — First-run experience & options

A stranger with no README must be able to play.

- [ ] **Settings menu** (pause + main menu): window mode/resolution, master + SFX volume
  (needs a SoundMix/SoundClass pass — volumes are currently hardcoded), graphics quality
  preset (engine scalability levels), persisted to config.
- [ ] **Show the crew URL in-game.** The LAN console feature is invisible unless you read the
  repo README. Put `http://<lan-ip>:8080` (and station list) on the pause menu and/or a
  bridge HUD corner so a living-room crew can join without documentation.
- [ ] **Key reference in-game:** a HELP overlay (or pause-menu page) listing station keys
  1/2/3/4, W/S/A/D, arrows, Space, etc. Full remapping is post-1.0; documenting is not.
- [ ] **Tutorialisation pass on Shakedown (mission 1):** the comms text exists; make sure a
  first-time player is told the actual keys/actions at each beat and can't soft-lock.
- [ ] **Quit buttons everywhere they're expected** (main menu, pause, outcome screens) and a
  confirm on quit-to-menu mid-mission.

## R3 — Polish & known jank (the "feels finished" bar)

- [ ] **Player death presentation** (smoke-test #2): on hull 0, explode/hide the player ship
  (reuse the enemy death-FX path), stop enemy AI firing at the wreck, then show the defeat
  screen. Currently enemies beam an intact mesh forever.
- [ ] **Clear weapons target on kill** (smoke-test #1): drop `CurrentTarget` when the target
  dies so the consoles don't show a lock on a dead ship.
- [ ] **Arrival grace period** (smoke-test #4): 10–15s of reduced/no enemy aggression after an
  encounter triggers, so a browser-console crew isn't killed while getting to stations.
- [ ] **Shakedown scan beat** (smoke-test #6): verify the tutorial's "scan the derelict"
  instruction actually works at spawn positions/arcs.
- [ ] **Warp standoff vs body size:** warp-to-objective uses a fixed 4000uu standoff; the
  Ember sun's radius (~12500) means a course-warp to the final objective lands inside it and
  the collision clamp ejects you in one tick. Make the standoff `max(4000, bodyRadius + margin)`.
- [ ] **HUD styling pass** (deferred since M13): consistent font, station-tab polish, remove
  the collapsed legacy M4 label for real.
- [ ] **Music + UI audio:** no music exists at all (menu track + ambient sector track + combat
  sting would transform feel; CC0 sources fine), plus button click/comms-ping UI sounds.
  NOTE: asset import via MCP crashes Interchange — use the legacy-factory workflow from memory,
  or import by hand in the editor.
- [ ] **Balance/difficulty playthrough:** at least two full human campaign runs (solo keyboard;
  crewed via browsers). Tune per-mission fleet size/damage; verify economy can't strand a
  player unable to afford repairs/torpedoes needed to progress.
- [ ] **Perf hygiene:** `ASpaceship::HandleCollisions` calls `GetAllActorsOfClass` twice per
  tick (enemies + landmarks) — cache the landmark list (static for the level) and have the
  mission subsystem hand over the live fleet. Fine at current actor counts, but cheap to fix
  and it's the hot path. Run `stat unit`/`stat game` in the packaged build once.
- [ ] **Log hygiene:** the code logs every throttle/turn/fire; downgrade chatty logs to
  Verbose so Shipping/Development builds don't spam.

## R4 — Content depth (scope decision — pick one before doing R6)

> 2026-07-06 update: gameplay-loop depth now has its own milestones — PROJECT_PLAN.md
> Phase 6 (M24–M30: subsystem damage, enemy behaviors, travel events, contracts, red alert,
> final battle v2 + skirmish + difficulty). Execution order lives in docs/OPUS_BRIEF.md.
> The options below remain the minimum-scope fallback if Phase 6 is cut.

The campaign is ~45–90 minutes. That's shippable for a free/itch release if framed honestly,
thin for anything paid. Missions are data-in-code (`BuildCampaign()`), so content is cheap:

- [x] Option A (minimum): keep 4 missions; add a post-campaign **free-play/skirmish mode**
  (endless waves at chosen landmark, reusing the director + fleet archetypes) for replay value.
- [ ] Option B (better): extend to 6–8 missions with 1–2 new wrinkles (escort/defend-the-station
  beat, an ambush that starts at red alert), plus skirmish.
- [x] Either way: a difficulty selector (enemy damage/HP multipliers) on new game.

## R5 — Web console hardening (ship-blocking because it's exposed on LAN)

- [x] **Access PIN:** the server binds 0.0.0.0 with zero auth, and mutating actions are
  unauthenticated GETs (`/api/warp`, `/api/buy`, `/api/game?action=restart`). Anyone on the
  LAN (or a malicious webpage doing cross-site GETs from any browser on the LAN — classic
  CSRF against local services) can control or reset the game. Minimum bar: a short session
  PIN shown in-game, required as a query/header token on every `/api/*` call; reject others.
- [ ] **Honest API responses** (smoke-test #3): every `/api/*` action should return
  `{ok:false, reason:"..."}` when rejected (out-of-arc, dead, undocked, broke) and be gated on
  game phase — today everything answers `ok` and consoles can't give feedback.
- [ ] **Redirect `/` → `/stations`** (smoke-test #5) so typing just `ip:8080` works.
- [ ] **Settings toggle:** enable/disable the crew server, and loopback-only mode.
- [ ] Native UE networking (dedicated crew clients) stays **post-1.0** — the browser-console
  model IS the multiplayer story for release; say so on the store page.

## R6 — Legal, distribution, release mechanics

- [ ] **Credits & licences:** in-game credits screen + THIRD_PARTY.md — Quaternius ship packs
  (CC0, credit anyway), any CC0 audio used, Unreal Engine trademark/EULA notice requirements.
- [ ] **Windows build:** UE cannot cross-compile Linux→Windows; a Win64 package needs a
  Windows machine (or CI, e.g. GitHub Actions self-hosted with UE installed). Linux-only
  first release on itch.io is a legitimate scoping call — decide explicitly.
- [ ] **Distribution:** itch.io first (zip per platform, butler push for updates); Steam
  later if traction (adds $100 fee, depot setup, Steamworks SDK decision).
- [ ] **Release mechanics:** version string surfaced on the main menu; git tag per release;
  a short player-facing README (keys, crew-console howto) inside the zip; 5–6 store
  screenshots + a 30s capture (the OS-capture tooling can produce these).
- [ ] **Save compatibility policy:** bump `USpaceSaveGame` version field now and default-load
  gracefully on mismatch, so post-release patches don't corrupt saves.

---

## Runtime smoke-test findings (2026-07-06, PIE @ 04e9e9c)

Full loop verified end-to-end in one session: tutorial kill → warp → 2-raider combat → defeat →
restart → dock → restock → buy upgrade → undock. Build clean (0 errors/warnings in game code),
log clean (0 fatals/ensures/Accessed-None over a 2800-line session). Concrete defects found:

1. **Stale weapons target after a kill** — `/api/state` keeps `target:"WASP-1"` (with range)
   after the target dies; only warp/restart clears it. Weapons console shows a lock on a wreck.
2. **Defeat state never ends** — after player death the raiders pour beams into the intact
   player mesh forever; no explosion, no wreck, no camera change on the player ship at hull 0.
   AI should stand down on phase→Defeat, and the player needs death FX (the enemy pipeline
   already has them).
3. **API silent failures** — every `/api/*` action returns `ok` even when rejected (fire
   out-of-arc, fire while dead, buy undocked, insufficient credits). One defeat-state fire-spam
   test produced 676 "Fire blocked" log lines. Endpoints need phase gating + a real
   `{ok:false, reason}` so browser consoles can show feedback.
4. **Warp-in death trap** — two Tarsis raiders killed a full-hull stationary ship in ~1 min.
   With crew fumbling browser consoles after arrival, an idle helm is near-certain death. Add
   a spawn/arrival grace period or slower AI aggression ramp.
5. **Root URL 404** — `http://ip:8080/` returns Epic's raw `route_handler_not_found` JSON; the
   index lives at `/stations`. Redirect `/` → `/stations`.
6. **Science scan oddity** — during Shakedown (`engaged:true`, scannable derelict present)
   `science?action=cycle` left `sciTarget:"none"`; likely arc/range gating. The mission text
   tells the player to scan, so verify the tutorial beat actually works.
7. **Python name clash warning** — `EStation` vs `AStation` both expose as "Station"
   (`LogPython` warning at boot); add `ScriptName` metadata to one.
8. **Confirmed R1 risk** — VibeUE logs "SECURITY WARNING: NO API key set → arbitrary local
   Python execution" at editor boot; these plugins absolutely must be editor-only in packaging.

Items 1–5 fold into R3 (gameplay) and R5 (API hardening) below; they're the difference between
"demo" and "released".

---

## Suggested order & effort

| Phase | Days (est) | Why this order |
|---|---|---|
| R1 packaging | 2–4 | Highest unknown; everything else is wasted if this fails |
| R2 first-run UX | 2–3 | Cheap, mandatory for strangers |
| R3 polish | 3–5 | Includes the two human playthroughs |
| R5 web hardening | 1 | Small, but ship-blocking (LAN-exposed control surface) |
| R4 content | 2–6 | Scope decision; Option A is 2 days |
| R6 legal/distro | 1–2 | Mostly checklists |

Realistic total: **2–3 focused weeks** to a v0.9 itch.io Linux release; Windows adds a
machine/CI dependency, not much code.
