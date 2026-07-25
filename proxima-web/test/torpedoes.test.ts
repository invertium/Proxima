// Stage 4b — torpedoes are projectiles that track, and can miss.
//
// This is what makes a gunship volley playable: it's slow enough to outrun and only
// turns so fast, so the helm can break the lock. A torpedo that flew straight (as the
// port's did) was a coin flip; one that tracked perfectly would be an unavoidable hit.

import { describe, expect, it } from 'vitest';
import { applyCommand, createWorld, step } from '../src/sim/world';
import {
  ENEMIES,
  TICK_DT,
  TORPEDO_BLAST_RADIUS,
  TORPEDO_DAMAGE,
  TORPEDO_TURN_RATE_DEG,
} from '../src/sim/data';
import { dist } from '../src/sim/math';
import type { EnemyShip, World } from '../src/sim/types';

const run = (w: World, seconds: number): void => {
  for (let i = 0; i < Math.round(seconds / TICK_DT); i++) step(w, TICK_DT);
};

/** Player at the origin with one hostile ahead, director disabled. */
const setup = (range: number): { w: World; e: EnemyShip } => {
  const w = createWorld({ seed: 12 });
  w.player.pos = { x: 0, y: 0, z: 0 };
  w.player.heading = 0;
  w.missionIndex = 99;

  const def = ENEMIES.cruiser;
  const e: EnemyShip = {
    kind: 'enemy',
    id: w.nextId++,
    enemyType: 'cruiser',
    pos: { x: range, y: 0, z: 0 },
    heading: Math.PI,
    hull: 9999,
    maxHull: 9999,
    shield: 0,
    maxShield: def.maxShield,
    alive: true,
    fireCooldown: 9999,
    graceTimer: 9999,
    rewarded: false,
    callsign: 'TARGET-1',
    aiState: 'idle',
    strafeSide: 1,
    volleyRemaining: 0,
    volleyTimer: 0,
  };
  w.enemies.push(e);
  w.player.targetId = e.id;
  return { w, e };
};

describe('torpedoes', () => {
  it('tracks a target that moves off the launch bearing', () => {
    const { w, e } = setup(9000);
    applyCommand(w, { c: 'fireTorpedo' });
    expect(w.torpedoes).toHaveLength(1);

    // Slide the target sideways after launch. A dumb-fired torpedo would sail past.
    // 9000 units at 5000/s is ~1.8s of flight, so give it three seconds.
    for (let i = 0; i < 60 * 3; i++) {
      e.pos.z += 40;
      step(w, TICK_DT);
    }

    expect(e.hull).toBeLessThan(9999);
  });

  it('cannot turn faster than its rated rate', () => {
    const { w } = setup(9000);
    applyCommand(w, { c: 'fireTorpedo' });
    step(w, TICK_DT);

    const before = w.torpedoes[0]!.heading;
    step(w, TICK_DT);
    const after = w.torpedoes[0]!.heading;

    const perTick = (TORPEDO_TURN_RATE_DEG * Math.PI) / 180 / 60;
    expect(Math.abs(after - before)).toBeLessThanOrEqual(perTick + 1e-6);
  });

  it('misses a target that out-turns it, rather than always connecting', () => {
    const { w, e } = setup(4000);
    applyCommand(w, { c: 'fireTorpedo' });

    // Teleport the target hard off-axis every tick: nothing with a 75 deg/s limit
    // can follow that, so the warhead must eventually fizzle.
    for (let i = 0; i < 60 * 14; i++) {
      e.pos.x = Math.cos(i * 0.4) * 40000;
      e.pos.z = Math.sin(i * 0.4) * 40000;
      step(w, TICK_DT);
    }

    expect(w.torpedoes).toHaveLength(0);
    expect(e.hull).toBe(9999); // never hit
  });

  it('reports a detonation either way, so the crew learns it missed', () => {
    const { w, e } = setup(3000);
    applyCommand(w, { c: 'fireTorpedo' });

    let detonations = 0;
    let hits = 0;
    for (let i = 0; i < 60 * 3; i++) {
      step(w, TICK_DT);
      for (const ev of w.events) {
        if (ev.t !== 'detonate') continue;
        detonations++;
        if (ev.hit) hits++;
      }
    }

    expect(detonations).toBe(1);
    expect(hits).toBe(1);
    expect(e.hull).toBe(9999 - TORPEDO_DAMAGE);
  });

  it('splashes everything inside the blast radius, not just the locked ship', () => {
    const { w, e } = setup(6000);
    // Passive hulls: two live cruisers shove each other apart via the separation AI,
    // which moves the formation out from under the blast and confounds the test.
    e.enemyType = 'derelict';
    const wingman: EnemyShip = {
      ...e,
      enemyType: 'derelict',
      id: w.nextId++,
      pos: { x: e.pos.x, y: 0, z: e.pos.z + TORPEDO_BLAST_RADIUS * 0.5 },
    };
    w.enemies.push(wingman);

    applyCommand(w, { c: 'fireTorpedo' });
    run(w, 4);

    expect(e.hull).toBeLessThan(9999);
    expect(wingman.hull).toBeLessThan(9999);
  });

  it('an enemy volley is outrunnable — that is the counterplay', () => {
    const w = createWorld({ seed: 3 });
    w.player.pos = { x: 0, y: 0, z: 0 };
    w.player.heading = 0;
    w.missionIndex = 99;

    const def = ENEMIES.gunship;
    w.enemies.push({
      kind: 'enemy',
      id: w.nextId++,
      enemyType: 'gunship',
      pos: { x: -3000, y: 0, z: 0 },
      heading: 0,
      hull: def.maxHull,
      maxHull: def.maxHull,
      shield: 0,
      maxShield: def.maxShield,
      alive: true,
      fireCooldown: 0.05,
      graceTimer: 0,
      rewarded: false,
      callsign: 'VIPER-1',
      aiState: 'engage',
      strafeSide: 1,
      volleyRemaining: 0,
      volleyTimer: 0,
    });

    // Run flat out, directly away. The interceptor does 2100; the torpedo does 1900.
    applyCommand(w, { c: 'throttle', v: 1 });
    const hull = w.player.hull;
    run(w, 16);

    expect(w.player.hull).toBe(hull);
  });

  it('a stationary ship eats the same volley', () => {
    const w = createWorld({ seed: 3 });
    w.player.pos = { x: 0, y: 0, z: 0 };
    w.missionIndex = 99;

    const def = ENEMIES.gunship;
    w.enemies.push({
      kind: 'enemy',
      id: w.nextId++,
      enemyType: 'gunship',
      pos: { x: -3000, y: 0, z: 0 },
      heading: 0,
      hull: def.maxHull,
      maxHull: def.maxHull,
      shield: 0,
      maxShield: def.maxShield,
      alive: true,
      fireCooldown: 0.05,
      graceTimer: 0,
      rewarded: false,
      callsign: 'VIPER-1',
      aiState: 'engage',
      strafeSide: 1,
      volleyRemaining: 0,
      volleyTimer: 0,
    });

    const hull = w.player.hull;
    run(w, 16);

    expect(w.player.hull).toBeLessThan(hull);
    void dist;
  });
});
