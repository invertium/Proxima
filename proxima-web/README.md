# proxima-web — browser-native Proxima (Three.js)

A working vertical slice of [issue #16](https://github.com/invertium/Proxima/issues/16):
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
npm run assets     # art_src/generated_ships/*.glb -> public/assets/ships/
npm run dev        # http://localhost:5173  (crew: /station.html)
```

Crew members open `http://<your-LAN-IP>:5173/station.html` on their phones. The relay
rides on the dev server, so there is no second process to start.

## Status

| | |
|---|---|
| Ship movement, power, shields, beams, torpedoes, ramming | ported, tested |
| Open sector, proximity mission triggers, seamless clears | ported, tested |
| Docking, repair/resupply, drydock hull purchase | ported, tested |
| Warp | ported, tested |
| Four crew stations over the LAN | working |
| Three.js sector: hulls, planets, sun, starfield, beams, blasts | working |
| Enemy AI | simplified — standoff ring + fire interval; no strafing passes or torpedo volleys yet |
| Save/load, contracts, upgrades, science scans, damage control | **not ported** |
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
src/render/     Three.js. Reads snapshots, owns no game state
src/net/        protocol + transport (the seam WebRTC lands on)
src/host/       pilot app + the sim Web Worker
src/stations/   crew console (one page, four stations)
server/relay.ts dumb pipe: host <-> stations. No game state, no validation
test/           vitest (headless sim) + smoke.mjs (real browser)
```

The dependency rule that makes the rest work: **`src/sim` imports nothing from
`three`, the DOM, or the network.** That is what lets the same code run in a worker, in
a test, and in CI without a GPU.

## Verification

```bash
npm test          # 13 headless sim tests, ~1s — determinism, damage, director, docking, warp
npm run typecheck
node test/smoke.mjs   # real browser: boots, flies, links a crew station, drives it, screenshots
```

`smoke.mjs` asserts the whole crew loop: it clicks STOP and FULL AHEAD on a *station*
page and waits for the *pilot* HUD to reflect it. Screenshots land in `shots/`. It uses
the system Chromium (`CHROME=` to override) so nothing has to be downloaded.

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
- Bundle: **157 KB gzipped** for the pilot (mostly Three.js), **2.3 KB** for a crew
  station. Build is ~1 s.
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

Honest list, beyond the status table: no save/load, no host migration when the host tab
closes, no reconnect state resync beyond "next snapshot wins", no protocol versioning
enforcement, no TURN/NAT traversal (LAN only), no audio, no accessibility pass on the
station UIs, and the enemy AI is a simplification rather than a port.
