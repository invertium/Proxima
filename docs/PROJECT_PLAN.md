# PROJECT PLAN — phased roadmap

Status legend: [ ] todo · [~] in progress · [x] done · [!] blocked

Each milestone needs a concrete "done when…" check confirmable via screenshot or log/PIE assertion.

---

## Phase 0 — Environment & setup  [~]
- [x] Audit OS/GPU/display/toolchain (see PROGRESS 2026-06-17)
- [x] Install UE 5.7 on Linux — binary 5.7.4 at /home/julian/UnrealEngine/UE_5.7, verified runnable (2026-06-18)
- [x] Create SpaceGame C++ project + clone UnrealClaude & VibeUE into Plugins/ (2026-06-18)
- [x] Build plugins — SpaceGameEditor build Succeeded; all 3 module .so built (patched VibeUE -Wcomment bug)
- [x] Headless load verification — both plugins mount/init, no crash; UnrealClaude registered capture_viewport + actor tools
- [x] Launch editor GUI (user, Wayland+Vulkan) — runs; all CEF/GUI libs present
- [x] Drive UnrealClaude MCP via curl on localhost:3000 (no client registration needed)
- [x] **MCP smoke test PASSED** — connect, list 28 tools, open empty level, capture_viewport, read back FRESH frame. Fixed stale-frame bug (forced viewport redraw) en route. (2026-06-18)
- [!] USER (deferred — not needed yet): paste VibeUE API key, wire VibeUE MCP (8088) when first needed for Blueprint/Python authoring

## Phase 0 — COMPLETE ✅ (env + engine + plugins + verified screenshot loop)
- [ ] **MCP smoke test** — done when: connect to MCP, list tools, start PIE on empty level, capture 1 viewport screenshot, read it back, stop PIE — all succeed. Highest-risk step; gate before any game work.

## Phase 1 — Game design & plan  [~]
- [x] Scoping answered: bridge sim (Artemis/EmptyEpsilon), vertical slice, Helm+Weapons+Engineering, single-machine first, stylized 3D 3rd-person + 2D consoles (see DECISIONS D5-D11)
- [x] Milestones + DECISIONS written
- [ ] **AWAITING USER APPROVAL of plan below** → then build

---

**Vertical-slice target loop:** Enemy ship detected on radar → Engineering routes power → Helm closes & orients → Weapons locks & fires beams → enemy destroyed; mismanaged power/shields → player takes damage / loses. One polished encounter.

Each milestone: smallest meaningful change, then PIE + screenshot + log, compare to "done-when", append to PROGRESS. Motion/feel items flagged for user hand-testing. `[S]`=screenshot check, `[L]`=log/PIE assertion.

## Phase 2 — World & ship foundation
- [x] M1 Arena map + space backdrop (dir light, starfield/skybox, post-process). DONE 2026-06-18 — PIE [S] shows dark starfield space scene (not default grid). Verified in actual PIE.
- [x] M2 `ASpaceship` (C++) + placeholder mesh + 3rd-person follow cam. DONE 2026-06-18 — [S] ship 3rd-person vs starfield (PIE, via ship follow cam); [L] `ASpaceship spawned: Spaceship_0 at X=0 Y=0 Z=0`. (Cone orientation + hull material deferred to M3/M13.)
- [x] M3 `UShipMovementComponent` (impulse throttle + turn). DONE 2026-06-19 — [L] `SetThrottle(1)/SetTurn(1)` in PIE: speed 0→1800 uu/s, yaw advancing ~60°/s, ship arced from origin to ~3000 uu; [S] before/after show starfield pan + ship reorient (follow cam tracks). Cone reoriented nose-+X. (Input wiring → M6 Helm.)

## Phase 3 — Stations UI scaffold (single-machine)
- [x] M4 `StationManager` + station switcher (keys 1/2/3) + HUD shell. DONE 2026-06-19 — C++ `ASpaceGameMode`+`ABridgePlayerController` (keys 1/2/3 → `SetStation`) + `UBridgeHUDWidget` shell, WBP_BridgeHUD reparented to it. [S] composited HUD over space in all 3 states (active tab cyan + big console label HELM/WEAPONS/ENGINEERING); [L] `[Bridge] Active station -> Helm/Weapons/Engineering`. (Key inject not scriptable on Linux → switch driven via `SetStation`, same path keys call; 1/2/3 hand-confirmable in focused PIE.)
- [x] M5 Tactical radar widget (2D): player centered, range rings, heading, world-actor blips. DONE 2026-06-20 — `URadarWidget` (NativePaint draws rings/axes/player heading-arrow/contact blips via `UWidgetBlueprintLibrary::DrawLines`, north-up world+X=up) + `URadarContactComponent` marker; WBP_Radar (root SizeBox 440×440) embedded bottom-left in WBP_BridgeHUD; test rig `BP_RadarContact` placed at (3000,0,0). [S] /tmp/m5_hud2.png: red blip due north at ~15% radius = 3000/20000 (bearing+range correct), player marker+heading arrow up.
- [x] M6 Helm console: throttle/turn wired to movement, speed/heading readouts, embeds radar. DONE 2026-06-20 — `ABridgePlayerController` W/S throttle ±0.2 + A/D yaw (gated to Helm); `UHelmConsoleWidget` polls movement comp → THR/SPD/HDG + throttle bar; WBP_HelmConsole (readouts top-right + radar bottom-left) toggled visible only at Helm. [S] /tmp/m6_console.png THR 60%/SPD 1080/HDG 344; [L] speed ramped 0→1080, yaw swept ~30°/s, ship looped; switching to Weapons collapses console.
- [x] M7 Engineering console: `UShipPowerComponent` (Beams/Impulse/Maneuver/Shields), power sliders + bars; power scales a system stat. DONE 2026-06-20 — `UPowerComponent` (EShipSystem Engine/Weapons/Shields, 0..200% each); engine power scales `UShipMovementComponent::GetEffectiveMaxSpeed`; `UEngineeringConsoleWidget` shows per-system bars + reactor load, arrow keys (gated to Engineering) select/adjust. [L] EnginePower 1.0→2.0 lifted effMax + speed 1800→3600; [S] /tmp/m7_eng_final.png (bars + selected highlight) & /tmp/m7_helm_boosted.png (Helm SPD 3600). Big M4 center label collapsed (overlapped consoles). Editor launch + foreground now fully self-service (tools/os_foreground.sh, KWin D-Bus).
- [x] M8 Weapons console: target cycle/select from radar, target-info panel (range/hull/shield), beam fire btn + charge bar. Done: [S] selected enemy + target info; [L] fire logs beam event. DONE 2026-06-20 — `UWeaponComponent` (forward beam, `Charge` 0..1 recharged by `BaseRechargeRate × weapon-power`, `BeamRange` 15000, `CycleTarget`/`FireBeam` over radar contacts, DrawDebugLine beam); `UWeaponsConsoleWidget` shows TARGET/RANGE/IN-OUT-OF-RANGE/BEAM-READY + charge bar; Weapons input gated (Right cycle, Space fire). [L] beam fired at in-range contact, range-gated on the far one, recharge tracks weapon power (0→held, 2.0→refill); [S] /tmp/m8_console.png (TARGET _0, RANGE 3000, IN RANGE, BEAM READY) & /tmp/m8_oor.png (RANGE 18000, OUT OF RANGE). Second test contact RadarTestContact2 at (0,18000,0).

## Phase 4 — Combat + enemy + loop
- [x] M9 `AEnemyShip` + simple AI (spawn, approach, fire intervals) + health components. Done: [S] before/after approach; [L] AI state + enemy firing. DONE 2026-06-20 — `UHealthComponent` (hull+shield, `ApplyDamage` shields→hull, `OnDeath` delegate) + `AEnemyShip` (APawn, Tick AI: face/approach player to `StandoffDistance`, fire every `FireInterval` once within `EngageRange`; `EEnemyAIState` Idle/Approach/Engage; carries radar-contact + health). Placed at (20000,0,0); removed static M8 RadarTestContact rig. [L] spawn→Approach→Engage→FIRE@2.5s, range closed 9000→5994; [S] /tmp/m9_before.png (dist 19841, blip at radar edge) vs /tmp/m9_after.png (dist 5999, blip pulled in, enemy in view). Gotchas: don't do actor ops right after PIE end_play (segfaults teardown); use `spectacle -a` not `-f` post-relaunch (fullscreen grabs white).
- [x] M10 Weapons→damage: beams reduce shields→hull, recharge gated by Beams power; beam FX. Done: [L] enemy hull drops on fire; [S] beam FX; enemy 0 hull → despawn + explosion [S]. DONE 2026-06-20 — `FireBeam` calls target `UHealthComponent::ApplyDamage(BeamDamage=25)` (shields→hull); beam FX = bright line + cyan impact flare sphere; `AEnemyShip::HandleDeath` (bound to OnDeath) draws an orange/red explosion burst then `Destroy()`s. [L] shield 50→25→0 then hull 100→75→50→25→0 (6 hits) → DESTROYED(0 enemies); [S] /tmp/m10_step_4.png (beam+flare hitting enemy, console TARGET/RANGE) & /tmp/m10_step_6.png (explosion burst, radar blip gone). Drove via Python (`set_station` WEAPONS, `set_system_power` WEAPONS 2.0, cycle+fire).
- [x] M11 Player damage + shields + lose: enemy beams damage player; shield power scales mitigation; 0 hull → defeat screen. Done: [L] player hull drops; [L] shield-power changes mitigation (assert); [S] defeat screen. DONE 2026-06-20 — player `UHealthComponent` (MaxShield 0 = pure power-mitigation); `UHealthComponent::GetShieldMitigation` from sibling Shields power scales `ApplyDamage`; `AEnemyShip` beams deal `EnemyBeamDamage`; controller `OnPossess`→bind player OnDeath→`ShowEndScreen` (pause + UIOnly); reusable `UEndScreenWidget`/`WBP_EndScreen`; HUD `HullText` readout. [L] enemy fire dropped hull 100→63.6; mitigation assert 40 dmg → LOSS 40 @power0 vs LOSS 12 @power2.0; [S] /tmp/m11_defeat.png (DEFEAT overlay, HULL 0%) & /tmp/m11_hud.png (HULL 38% amber).
- [x] M12 Win condition + encounter flow: destroy enemy → victory screen. Done: full loop in one PIE session [S sequence]: engage→manage power→destroy→victory. DONE 2026-06-21 — controller `BindEnemyDeaths` subscribes to every `AEnemyShip` OnDeath; last kill → `HandleEnemyDeath` recounts survivors → `ShowEndScreen("VICTORY", green)` (reuses M11 `UEndScreenWidget`/pause/UIOnly). [S] sequence /tmp/m12_engaged.png (target in range, BEAM READY, HULL 48%) → /tmp/m12_fire_3.png (beam hitting enemy, return fire HULL 41%) → /tmp/m12_victory.png (VICTORY, NO TARGET, paused, HULL 29%); [L] 6 shots shield50→0 hull100→0, fire returned ALL DEAD, game paused. Closes the M9–M12 combat loop.

## Phase 5 — Vertical-slice polish
- [~] M13 Visual polish: emissive hull, Niagara thrusters, beam/explosion FX, backdrop, HUD styling. Done: [S] side-by-side stylized vs primitive. PARTIAL 2026-06-21 — emissive faction materials (M_PlayerHull/M_EnemyHull + glow mats); composited ship models (player fighter w/ swept wings + cyan engine glow; enemy cruiser w/ prongs + red sensor eye + orange engine); beam + explosion FX as real emissive meshes (ABeamFx/AExplosionFx) replacing DrawDebug. [S] /tmp/m13_ships.png, m13_enemy2.png, m13_beam.png (cyan beam connecting ships). TODO: explosion screenshot (0.65s flash vs capture latency + victory-pause freeze), Niagara thrusters (impractical via MCP), backdrop/nebula, HUD font styling. UPDATE 2026-06-21 — real ship meshes swapped in: imported **Quaternius Ultimate Spaceships (CC0)** Insurgent (player) + Imperial (enemy) into /Game/Art/Meshes with palette textures + M_Insurgent/M_Imperial materials; composited-primitive hulls removed. [S] /tmp/twoship3.png. (Import must use the legacy FBX factory — Interchange crashes via MCP; see PROGRESS + memory.) UPDATE 2026-06-21 — nebula backdrop added: M_Starfield→additive + new M_Nebula on a Nebula_Dome behind the stars. [S] /tmp/nebula1.png. Remaining M13: explosion shot, HUD font styling.
- [~] M14 Audio/feedback: SFX (beams/impulse/alarms), camera shake. Done: [L] triggers fire + assets assigned; feel→user hand-test. PARTIAL 2026-06-21 — camera shake done (trauma model on ASpaceship: fire recoil + damage + nearby-explosion; PIE-verified numerically). TODO: SFX/audio (needs sound assets + import), hit/impact FX.
- M15 Hardening: tune, fix, repeatable loop, "how to play" note. Done: user plays through once successfully.

## LATER (post-slice backlog, not yet detailed)
- Networking: server + per-station clients, replicate ship/subsystem state.
- Stations: Science (scan/database), Comms/Relay.
- Systems: missiles, multi-facing shields, warp/jump, multiple enemies, scenarios/missions, GM tools.
- Wire VibeUE API key + MCP (needed once we author Blueprints/UMG — i.e. M4).
