# proxima-web — browser-native Proxima (Three.js)

A feature-complete port of [issue #16](https://github.com/invertium/Proxima/issues/16):
Proxima as a browser-first game. One player opens the page and hosts the authoritative
simulation; the crew joins from phones on the LAN and runs Helm / Weapons / Engineering
/ Science.

**Renderer: Three.js, not Babylon.js.** Issue #16 recommends Babylon on
engine-completeness grounds, which is a fair argument. This slice takes the other
branch because the project's two stated priorities — asset generation and token cost —
both point at Three.js. The reasoning, including where Babylon genuinely wins, is in
[§Why Three.js](#why-threejs).

```bash
npm install
npm run dev        # http://localhost:5173  (crew: /station.html)
```

Crew members open `http://<your-LAN-IP>:5173/station.html` on their phones. The relay
rides on the dev server, so there is no second process to start.

## Status

**Playable as a bridge simulator, and now at parity with the Unreal build.** An
earlier revision of this file claimed feature-completeness while roughly a third of
the simulation and half the console controls were missing, and the crew consoles
could not be operated by a human at all. [PORT_PLAN.md](PORT_PLAN.md) documents the
staged repair; §Not done lists what genuinely remains.

| | |
|---|---|
| Ship movement, power, shields, beams, torpedoes, ramming | ported, tested |
| Damage control — 3 systems, weld repair, hull alarm | ported, tested |
| Science scans (and the cruiser armour they break) | ported, tested |
| Drydock — 9 upgrade paths, hull purchases, rank/XP gates | ported, tested |
| Auto-turret, manoeuvring thrusters | ported, tested |
| Hostile AI — 4 states, strafe runs, torpedo volleys, archetypes | ported, tested |
| Open sector, objective hail/accept, seamless clears | ported, tested |
| Sector events — distress, interdiction, salvage | ported, tested |
| Contracts — bounty, patrol, delivery | ported, tested |
| Gravity wells | ported, tested |
| Save/load with versioned migration | ported, tested |
| Difficulty, skirmish waves, defeat/retry | ported, tested |
| Menus, pause, outcome screens | working |
| Four crew stations over the LAN | working |
| Three.js sector, camera trauma, torpedoes, FX | working |
| Red alert doctrine — shields charge at red, bleed at green | ported, tested |
| Homing torpedoes with a turn-rate limit and blast radius | ported, tested |
| Flagship climax — invulnerable until its AEGIS escorts die | ported, tested |
| Command rejection reasons, surfaced as console toasts | ported, tested |
| Crew-side RESTART / NEW CAMPAIGN and a game-phase footer | working |
| Settings (volume, quality, wake lock), controls card | working |
| Session PIN, joinable by link or QR | working |
| Session telemetry (`?record=1`) | working |
| Engineering weld minigame — 1.2s sweep, green band, rate limit | ported, tested |
| Enemy callsigns (WASP-1, VIPER-2) | ported, tested |
| Solid planets and sun | ported, tested |
| Starbase, planets and star as procedural models (img2threejs) | working — 8 draw calls |
| Science owns comms: fleet orders *and* the contract board | working, tested |
| Docked invulnerability; ram debounce and speed scaling | ported, tested |
| Reactor power is linear — 0 power is a dead system | ported, tested |
| Audio | bus is real, **cues are synthesised stand-ins** (the Unreal `.uasset` samples aren't readable from the browser) |
| Art | **placeholder** TRELLIS hulls; procedural models via img2threejs come later |
| WebRTC peer-to-peer | **not started** — the relay is a plain WebSocket pipe today |

All gameplay numbers are ported verbatim from the C++ (`ShipCatalogue.h`,
`EnemyShip.h`, `WeaponComponent.h`, `HealthComponent.h`, `MissionSubsystem.cpp`), so
the browser build plays at the tuning the Unreal build shipped with.

## Layout

```
src/sim/        authoritative simulation — no Three.js, no DOM, no timers
  data.ts       ALL content and tuning: hulls, hostiles, campaign, constants
  world.ts      createWorld / step / snapshot
  combat.ts     damage model, firing arcs
  ai.ts         hostile behaviour
  save.ts       pure serialisation; the storage adapter lives in src/host
  sector.ts     events, contracts, gravity
  stats.ts      base hull + upgrades - damage -> the numbers everything reads
src/render/     Three.js. Reads snapshots, owns no game state
src/net/        protocol + transport (the seam WebRTC lands on)
src/host/       pilot app, front-end menus, IndexedDB save, the sim Web Worker
src/stations/   crew console (one page, four stations)
server/relay.ts dumb pipe: host <-> stations. No game state, no validation
test/           vitest (unit + full-campaign replay), e2e.mjs, budget.mjs
```

The dependency rule that makes the rest work: **`src/sim` imports nothing from
`three`, the DOM, or the network.** That is what lets the same code run in a worker, in
a test, and in CI without a GPU.

## Verification

Three layers, cheapest first.

```bash
npm run verify    # typecheck + unit + build + browser E2E
npm test          # 198 headless sim tests, ~2s
npm run e2e       # 12 browser user journeys (needs `npm run dev` running)
npm run budget    # bundle size + frame time under load (needs a built `npm run relay`)
```

**Unit + replay (198 tests, ~2 s).** Per-system rules, plus a full-campaign replay: a
scripted crew flies start to victory headlessly. That catches what unit tests can't — a
mission that can't be reached, an encounter that never clears, a comms beat that never
fires, or a balance change that makes the campaign unwinnable.

The replay's win assertion is a *threshold* (75% of seeds), not "always". The scripted
crew trades rather than kiting, so an ambush mission can legitimately kill it — that's a
statement about the bot, not the game. The value is the cliff: real breakage drops it to
near zero.

**Browser E2E (12 journeys).** Real pages driven through Playwright, each asserting
host-side state rather than pixels: new game → hail → accept → engage; a station flying
the ship the pilot renders; an Engineering preset changing top speed; a Science scan
resolving a contact that Weapons then reads; four stations linked at once; pause;
skirmish; and progress surviving a reload. Uses system Chromium (`CHROME=` to override),
so nothing is downloaded.

**Budgets.** Host 180 KB gzipped, station 22 KB, median frame time under a full skirmish
fight ~57 ms on software rendering (SwiftShader — a real GPU is an order of magnitude
under this). All three fail the build if they regress.

**Tunable drift.** `CPP_REFERENCE` in `src/sim/data.ts` records the Unreal value of every
ported constant, and a test asserts each one matches unless it appears in `DIVERGENCES`
with a justification. There is currently one declared divergence (`DOCK_RANGE`).

**Console layout divergence.** The tunable table covers numbers, not which console owns
which verb. One deliberate departure from the C++: **Science owns both acceptances.**
ACCEPT ORDERS was on the C++ Science page and had drifted onto Helm in this port — that
is a parity fix. The contract board was on the C++ Engineering page and moved to Science
— that is a change, made because both verbs are the same act (answering a hail), and a
crew that has to remember which console answers which hail has been given a filing
system rather than a bridge. `test/panels.test.ts` asserts the ownership so it cannot
drift back.

Deep paths needing a long flight — docking, repair, the drydock purchase loop — are
covered in the headless replay rather than E2E, because clicking a ship across 200 000
units to reach them is a worse test than simulating it.

## Asset pipeline

This is the part that motivated the renderer choice.

```bash
# 1. generate (content-engine MCP / TRELLIS) -> art_src/generated_ships/foo.glb
# 2.
npm run assets
# 3. add a row to src/sim/data.ts:  { type: 'foo', model: 'foo', scale: 700, ... }
```

That is the entire import step. `npm run assets` runs `gltf-transform optimize`
(weld, join, simplify, meshopt, WebP@1K) — the four current hulls go **27 MB → 2.6 MB**
in a few seconds.

Two things are handled automatically so new assets don't need hand-fixing:

- **Scale.** Every GLB is recentred and normalised to unit length on load, so the
  catalogue's `scale` is the only number controlling how big a hull reads.
- **Orientation.** Generators disagree about which axis a hull points down, and each
  TRELLIS batch lands on a different one. `axisAlign()` in `src/render/ships.ts` sorts
  the bounding box and rotates length→X, width→Z, height→Y. Bow-vs-stern is the one
  thing a bounding box can't resolve; if a hull flies backwards, that is the one manual
  fix.

### Procedural models (the img2threejs half)

Landmarks don't go through that pipeline at all — they are built in code from a generated
reference image, which is what `src/render/models/{starbase,planet}.ts` are. No mesh file,
no texture file; the starbase is 8 draw calls because every static part is baked into one
merged geometry per material at build time.

The check that matters is visual, and it has caught every defect these models have had
(black planets, an opaque corona ring, a ring built in the wrong plane) while the test
suite stayed green throughout. So there is a bench for it:

```bash
npm run dev
node tools/model-shot.mjs starbase front /tmp/sb.png   # or three-quarter | top
```

`/model.html?model=…&view=…` renders one model with the game's exact lighting, to be held
up against `art_src/refs/`. It is dev-server only — not a build entry, so it costs the
shipped bundle nothing.

Compare with the Unreal path this replaces: per-format Interchange feature flags, a
crash class when importing from the Python worker thread, 0-size bounds on reimport,
material graphs rebuilt over MCP, and 79 MB of `.uasset` for the same four ships.

## Performance

- Fixed 60 Hz sim in a Web Worker; rendering can never stall it, and it can never stall
  the frame. Catch-up is capped at 8 ticks so a backgrounded tab doesn't spiral.
- No shadow maps — nothing in space casts a useful one.
- One `Points` cloud for 4000 stars; ships clone shared prototypes so materials batch.
- Pooled beams and blasts; nothing allocates per shot.
- Logarithmic depth buffer, because the sector spans ~220 000 units and a ship is ~1000.
- Bundle: **168 KB gzipped** for the pilot (mostly Three.js), **16 KB** for a crew
  station. Build is ~1 s. Both are budget-checked (`npm run budget`).
- Still on WebGL. Materials are deliberately kept node-compatible (no raw GLSL), so
  moving to `WebGPURenderer` is a renderer swap rather than a shader rewrite.

## Networking

Today: WebSocket relay (`server/relay.ts`). The host browser is authoritative; stations
submit commands the sim validates exactly as it validates the pilot's (range, arc,
charge, reactor headroom). The relay holds no game state and inspects nothing.

Issue #16 argues for WebRTC data channels, and that's right for the eventual topology —
it removes the relay from the gameplay path. `HostTransport` / `StationTransport` in
`src/net/transport.ts` are the seam; the relay then demotes to signaling. WebSocket
first because it makes LAN crew play work *now*, which is the point of the game.

## Why Three.js

Issue #16's table rates Three.js as "primarily a rendering library" — true, and its
engine-completeness argument for Babylon is legitimate. Two things push the other way
for *this* project:

1. **Code-as-content.** For a bridge sim in empty space, most visual surface is
   procedural — starfield, planets, beams, blasts, radar overlays — not sculpted
   assets. That is generated code, and the tooling for generating Three.js code
   (`img2threejs` and its class) has no Babylon equivalent and doesn't port: different
   geometry constructors, different material system, different handedness.
2. **Token cost.** ~3.5M vs ~400K weekly downloads is roughly 9:1 of training-data
   representation. Agents write Three.js with less lookup, fewer wrong turns, and less
   doc-reading per change — which is the stated optimization target.

Where Babylon would genuinely have won: the Node Material Editor (a real content-
generation advantage with no Three.js equivalent — worth revisiting if shield/nebula
shaders get ambitious), better exotic-glTF-extension fidelity (unused here — these
hulls are baseColor + metal/rough), and not having to hand-assemble physics/audio/GUI.
The last one is a real future cost this slice hasn't paid yet.

**The renderer is confined to `src/render`** — but note that codegen output lands
*inside* that boundary. The boundary protects `src/sim`; it does not preserve a
cost-free renderer swap.

## Not done

Honest list, beyond the status table:

- **Ship hulls and audio are still placeholders.** Hulls are the TRELLIS GLBs; audio cues
  are synthesised. The starbase, planets and star are done — built in code by
  **img2threejs** reconstruction from generated references in `art_src/refs/` — but every
  ship, player and enemy alike, is still a GLB. `makeShip()` is the seam that pass lands
  on, and `art_src/refs/enemy-cruiser.png` is already waiting for it.
- **No WebRTC.** The relay is a plain WebSocket pipe, so play is LAN-only: no TURN, no
  NAT traversal, no peer-to-peer. The session PIN is a courtesy lock on a trusted LAN,
  not authentication — it is sent in the URL and the relay is unencrypted over `ws:`.
- **No host migration.** If the host tab closes, the game ends.
- **Reconnect is "next snapshot wins."** No state-resync protocol, no command
  acknowledgement or replay.
- **Protocol versioning is declared but not enforced** — `PROTOCOL_VERSION` exists and
  nothing rejects a mismatch.
- **No accessibility pass** on the station UIs.
- **Session recorder / telemetry** (`sg.RecordSession`) is not ported.
