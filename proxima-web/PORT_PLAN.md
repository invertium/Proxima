# Full port plan — Unreal C++ → TypeScript + Three.js

Scope: bring `proxima-web` from the current vertical slice to **feature parity** with the
12.5k-line Unreal build, then verify it end-to-end.

**Out of scope for now:** asset generation. The four TRELLIS hulls in
`public/assets/ships/` stay as placeholders. New art will come from **img2threejs**
(procedural Three.js models built in code) in a later milestone — no other content
engine. Nothing in this plan should assume a GLB pipeline; `src/render` keeps loading
models behind `makeShip()`, which a procedural factory can satisfy just as well.

## Principles carried from the slice

1. **`src/sim` imports nothing from `three`, the DOM, or the network.** Every gameplay
   rule is a pure function over state. This is what makes headless verification cheap.
2. **Content is data.** `src/sim/data.ts` holds every tunable. Balance work must never
   require touching a code path.
3. **Commands are the only mutation.** Stations and the pilot both submit `Command`s
   that the host validates. No privileged path.
4. **Numbers ported verbatim** from the C++ headers so the browser build plays at the
   tuning the Unreal build shipped with.

## What is already done

Ship movement, reactor power, shield mitigation, beams, torpedoes, ramming, the open-
sector director (proximity trigger + seamless clear), docking/repair/resupply, drydock
hull purchase, warp, four crew stations over a relay, and the Three.js sector.

---

## M1 — Ship systems

Closes the gap between "a ship that flies and shoots" and the full bridge.

- **Damage control** (`DamageControlComponent`): three damageable systems — Engine
  (max speed halved), Weapons (beam recharge halved), Sensors (radar + scan range
  halved). Every hull hit with shields down rolls `DamageChance` 0.35 to knock one out.
  `DamagedMultiplier` 0.5. Engineering's weld sweep: `WeldsPerSystemRepair` 3 welds fix
  the first damaged system in enum order; only then do welds restore hull. Hull-critical
  alarm below 0.3.
- **Science** (`ScienceComponent`): target cycling, `ScanDuration` 2.5s, `ScanRange`
  40000 (halved by Sensors damage), scanned targets reveal hull/shield numbers.
- **Upgrades** (`UpgradeCatalogue`): all 9 paths — beam damage +8/tier, beam recharge
  +0.15, fire arc +15°, hull +40, shields +30, torpedo +2, reactor +0.5, plus two
  one-time modules: manoeuvring thrusters (strafe 950) and auto-turret (12 dmg). Tier
  `t+1` costs `BaseCost*(t+1)` and needs rank `t+1`.
- **Rank/XP**: `rank = 1 + XP/400`, gating upgrade tiers and hull purchases.
- **Auto-turret**: `TurretDamage`, `TurretRange` 12000, `TurretInterval` 1.6 — fires
  independently of the beam once bought.
- **Strafe**: currently a flat 35% of max speed; becomes the bought `MaxStrafeSpeed`
  with `StrafeAcceleration` 2600.

Verify: unit tests per system — damage rolls are seeded and deterministic, weld repair
takes exactly 3 welds, an upgrade purchase moves the stat it claims to, rank gates bite.

## M2 — Hostiles

- **AI states** (`EEnemyAIState`): Idle / Approach / Engage / Overshoot, replacing the
  slice's single standoff-ring behaviour.
- **Strafing runs**: scouts close to `StrafePassDistance` 2800, fly through, and loop
  out at `StrafeBreakoffDistance` 9000 before re-attacking.
- **Torpedo volleys**: gunships fire `VolleySize` 3 torpedoes at `VolleyGap` 0.6s,
  speed 1900 — slow enough for the helm to outrun, which is the intended counterplay.
- **Archetype presets**: per-type scale, hull, shields, damage, rewards from
  `EnemyShip.cpp` `SetupFor`.
- **Armoured cruisers**: `ArmoredBeamMultiplier` 0.5 while shields hold (already in).
- **Enemy torpedoes** as real projectiles the player can outmanoeuvre.

Verify: a headless fight harness — spawn each archetype against a scripted player,
assert the AI reaches each state, that a volley is 3 torpedoes 0.6s apart, and that a
strafer actually breaks off rather than parking.

## M3 — Sector life

- **Sector events** (`ESectorEvent`): Distress (timed — clear raiders near another
  system), Interdiction (pirate ambush on the ship's path), Salvage (free-floating
  pickup collected by proximity). Rolled on a timer, with time limits and resolution.
- **Contracts** (`EContractType`): Bounty (kill a named ship at a landmark), Patrol
  (visit two landmarks), Delivery (fly cargo out, return, dock). Offered at the
  starbase, one active at a time, persisted.
- **Objective offer/accept**: `bObjectiveOffered` + `AcceptObjective` — arriving at a
  system hails first and waits for the crew to accept, rather than ambushing them.
- **Gravity fields**: soft drift toward bodies, capped under thrust so it's escapable.
- **Salvage caches**: proximity pickup, credits.

Verify: scripted campaign runs that trigger each event kind and each contract type to
completion, asserting rewards land and state persists across a save/load cycle.

## M4 — Campaign flow and persistence

- **Save/load**: `USpaceSaveGame` → IndexedDB. Mission index, credits, XP, upgrade
  tiers, owned ships, active contract and its stage. Versioned with a migration hook.
- **New game**: difficulty select (Ensign/Captain/Admiral — damage and hull scaling)
  and starting hull.
- **Menus**: main menu, pause, settings (volume, quality), outcome screens
  (victory/defeat), retry.
- **Skirmish mode**: endless waves, `WaveInterval` 12s, wave counter.
- **Flagship fight**: escort wiring for the final encounter.

Verify: save → reload → state identical; a defeat → retry cycle; skirmish reaching
wave 5.

## M5 — Presentation

- **Camera trauma/shake**: `TraumaDecayPerSec` 1.6, `HitTraumaPerDamage` 0.04, per-axis
  maxima — the existing feel, driven off sim events.
- **FX**: visible torpedo projectiles, debris on kill, better explosions, beam draw
  time, engine trails.
- **HUD parity**: damaged-system indicators, hull-critical alarm state, docking prompt,
  warp charge, objective offer prompt.
- **Audio**: WebAudio bus with the six cues (alarm, beam, enemy fire, engine hum,
  explosion, hit). Source assets are Unreal `.uasset`; until they're exported this
  milestone ships a procedural WebAudio fallback so the bus and mixing are real even
  though the samples aren't final.

Verify: browser smoke shots per state; an events-to-FX unit test that every `SimEvent`
kind has a handler.

## M6 — Station parity

- **Science station**: scan control, scan results, sector starmap.
- **Engineering**: damage-control weld sweep, upgrade purchase UI, power presets.
- **Weapons**: separate beam and torpedo arc wedges, scanned-target detail.
- **Helm**: sector map modal with live player marker, warp-to-objective ("lay in
  course"), contract objective display.
- Mobile pass: touch targets, no layout shift on repaint, works one-handed.

Verify: browser E2E driving each station through its full control set.

## M7 — Verification

Three layers, cheapest first:

1. **Unit** (`vitest`, ~seconds): every system's rules. Target: every sim module has
   tests; determinism test stays green.
2. **Campaign replay** (headless, ~seconds): a scripted command stream plays the whole
   campaign start to victory with no browser. This is the regression net — it catches
   balance and flow breakage that unit tests miss.
3. **Browser E2E** (`playwright`, ~minute): real user journeys —
   - new game → fly → objective offered → accept → fight → clear → advance
   - take damage → system knocked out → Engineering welds it back
   - dock → buy an upgrade → confirm the stat moved → buy a hull
   - accept a contract → complete it → collect
   - four stations open at once, each driving the host
   - save → reload the page → campaign resumes
   - defeat → retry
   Each journey asserts host-side state, not just pixels, and captures a screenshot.

Plus a **performance budget** check: frame time under load (full fleet + FX) and
bundle size ceilings, failing CI if either regresses.

## M8 — Art via img2threejs (deferred, not in this pass)

Replace placeholder hulls with procedural Three.js models generated by img2threejs from
reference images, built in code behind the existing `makeShip()` seam. Listed here so
the architecture keeps room for it; **not executed in this pass**.

---

## Execution order

M1 → M2 → M3 → M4 → M5 → M6, with M7's unit and replay layers grown continuously
alongside each milestone rather than saved for the end. Full browser E2E lands once M6
gives it something complete to drive.

Each milestone: implement → unit tests green → typecheck → commit.
