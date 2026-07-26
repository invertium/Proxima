// @vitest-environment jsdom
//
// Render-side interpolation. The sim ticks at a fixed 60Hz in a worker while the
// renderer draws on rAF; the two are not phase-locked, so drawing the newest snapshot
// directly means some frames reuse a sample and others skip one. At 2100 units/second
// that reads as a hard stutter, worst while accelerating or turning because the
// per-tick delta is itself changing.

import { describe, expect, it } from 'vitest';
import { blend, mix, mixAngle } from '../src/render/scene';
import { createWorld, snapshot, step } from '../src/sim/world';
import { TICK_DT } from '../src/sim/data';
import type { Snapshot } from '../src/sim/types';

/** Two consecutive real snapshots of a ship under way and turning. */
const consecutive = (): [Snapshot, Snapshot] => {
  const w = createWorld({ seed: 8 });
  w.missionIndex = 99;
  w.pending.push({ cmd: { c: 'throttle', v: 1 } }, { cmd: { c: 'turn', v: 1 } });
  for (let i = 0; i < 300; i++) step(w, TICK_DT);

  const a = snapshot(w);
  step(w, TICK_DT);
  return [a, snapshot(w)];
};

describe('angle blending', () => {
  it('takes the short way round the wrap point', () => {
    // 350deg -> 10deg must go forwards through zero, not backwards the long way.
    const a = (350 * Math.PI) / 180;
    const b = (10 * Math.PI) / 180;
    const mid = mixAngle(a, b, 0.5);
    const deg = ((mid * 180) / Math.PI + 360) % 360;
    expect(deg).toBeCloseTo(0, 4);
  });

  it('is exact at both ends', () => {
    expect(mixAngle(1.2, 2.4, 0)).toBeCloseTo(1.2, 6);
    expect(mixAngle(1.2, 2.4, 1)).toBeCloseTo(2.4, 6);
  });

  it('mix is linear', () => {
    expect(mix(10, 20, 0.25)).toBe(12.5);
  });
});

describe('snapshot blending', () => {
  it('places the ship between the two samples', () => {
    const [a, b] = consecutive();
    expect(a.player.pos.x).not.toBe(b.player.pos.x);

    const mid = blend(a, b, 0.5);
    const expected = (a.player.pos.x + b.player.pos.x) / 2;
    expect(mid.player.pos.x).toBeCloseTo(expected, 6);
  });

  it('produces evenly spaced steps across a tick — which is the whole point', () => {
    const [a, b] = consecutive();

    // Sample the blended position at even fractions and measure the step sizes.
    const steps: number[] = [];
    let previous = blend(a, b, 0).player.pos;
    for (let i = 1; i <= 8; i++) {
      const p = blend(a, b, i / 8).player.pos;
      steps.push(Math.hypot(p.x - previous.x, p.z - previous.z));
      previous = p;
    }

    const min = Math.min(...steps);
    const max = Math.max(...steps);
    // Drawing the newest snapshot directly gives steps of [full, 0, full, 0...];
    // interpolation gives a near-constant step. That difference IS the judder.
    expect(max - min).toBeLessThan(max * 0.01);
    expect(min).toBeGreaterThan(0);
  });

  it('leaves readouts on the newer sample rather than lagging them', () => {
    const [a, b] = consecutive();
    const mid = blend(a, b, 0.5);

    // Positions blend; everything a crew *reads* comes from the newer snapshot.
    expect(mid.time).toBe(b.time);
    expect(mid.player.hull).toBe(b.player.hull);
    expect(mid.player.throttle).toBe(b.player.throttle);
  });

  it('shows a contact that only exists in the newer sample at its real position', () => {
    const [a, b] = consecutive();
    const fresh: Snapshot = {
      ...b,
      contacts: [
        {
          id: 999,
          name: 'WASP-9',
          className: 'Pact Scout',
          pos: { x: 1234, y: 0, z: 5678 },
          heading: 0,
          hull: 10,
          maxHull: 10,
          shield: 0,
          hostile: true,
          inBeamArc: false,
          inTorpedoArc: false,
          range: 100,
          scanned: false,
          shielded: false,
        },
      ],
    };

    const mid = blend(a, fresh, 0.5);
    // No earlier sample to blend from, so it must not be dragged toward the origin.
    expect(mid.contacts[0]!.pos.x).toBe(1234);
  });
});

describe('throttle is an order, not a measurement', () => {
  it('reports the commanded throttle even while the ship is still accelerating', () => {
    const w = createWorld({ seed: 2 });
    w.missionIndex = 99;
    w.pending.push({ cmd: { c: 'throttle', v: 1 } });
    step(w, TICK_DT);

    const snap = snapshot(w);
    expect(snap.player.throttle).toBe(1);
    // The ship has barely moved: a lever reflecting speed would sit near zero and
    // then crawl upward under the operator's finger.
    expect(snap.player.speed / snap.player.maxSpeed).toBeLessThan(0.05);
  });
});
