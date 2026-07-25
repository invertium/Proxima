// Stage 3 — the combat model the port had quietly simplified away:
// alert doctrine, honest reactor power, docked safety, and ram debouncing.

import { describe, expect, it } from 'vitest';
import { applyCommand, createWorld, snapshot, step } from '../src/sim/world';
import { applyDamage } from '../src/sim/combat';
import { effectiveStats } from '../src/sim/stats';
import {
  COLLISION_RADIUS,
  ENEMIES,
  RAM_DAMAGE,
  RAM_SPEED_MAX,
  SHIELD_BLEED_RATE,
  SHIELD_CHARGE_RATE,
  TICK_DT,
} from '../src/sim/data';
import type { EnemyShip, World } from '../src/sim/types';

const run = (w: World, seconds: number): void => {
  for (let i = 0; i < Math.round(seconds / TICK_DT); i++) step(w, TICK_DT);
};

/** A hostile parked next to a stationary player, with the director kept out of it. */
const withEnemy = (range: number): { w: World; e: EnemyShip } => {
  const w = createWorld({ seed: 9 });
  w.player.pos = { x: 0, y: 0, z: 0 };
  w.missionIndex = 99;

  const def = ENEMIES.gunship;
  const e: EnemyShip = {
    kind: 'enemy',
    id: w.nextId++,
    enemyType: 'gunship',
    pos: { x: range, y: 0, z: 0 },
    heading: Math.PI,
    hull: def.maxHull,
    maxHull: def.maxHull,
    shield: 0,
    maxShield: def.maxShield,
    alive: true,
    fireCooldown: 9999, // never shoots; this fixture is about collisions
    graceTimer: 9999,
    rewarded: false,
    aiState: 'idle',
    strafeSide: 1,
    volleyRemaining: 0,
    volleyTimer: 0,
  };
  w.enemies.push(e);
  return { w, e };
};

describe('alert doctrine', () => {
  it('starts at green', () => {
    expect(createWorld().alert).toBe('green');
  });

  it('bleeds the shield pool at green alert', () => {
    const w = createWorld();
    w.missionIndex = 99;
    const before = w.player.shield;

    run(w, 4);

    expect(w.player.shield).toBeCloseTo(before - SHIELD_BLEED_RATE * 4, 1);
  });

  it('charges the pool at red alert, scaled by shields power', () => {
    const w = createWorld();
    w.missionIndex = 99;
    w.player.shield = 0;
    applyCommand(w, { c: 'alert', state: 'red' });
    applyCommand(w, { c: 'power', system: 'shields', v: 1 });

    run(w, 3);

    expect(w.player.shield).toBeCloseTo(SHIELD_CHARGE_RATE * 3, 1);
  });

  it('charges nothing at red alert with the shields row starved', () => {
    const w = createWorld();
    w.missionIndex = 99;
    w.player.shield = 0;
    applyCommand(w, { c: 'alert', state: 'red' });
    applyCommand(w, { c: 'power', system: 'shields', v: 0 });

    run(w, 3);

    // Raw power semantics: a starved system is dead, not merely slower.
    expect(w.player.shield).toBe(0);
  });

  it('toggles from any console and tells the crew', () => {
    const w = createWorld();
    applyCommand(w, { c: 'alert', state: 'toggle' });
    expect(w.alert).toBe('red');
    expect(w.events.some((e) => e.t === 'alert' && e.red)).toBe(true);

    applyCommand(w, { c: 'alert', state: 'toggle' });
    expect(w.alert).toBe('green');
    expect(snapshot(w).alert).toBe('green');
  });

  it('holds the pool steady while docked, rather than bleeding on the clamps', () => {
    const w = createWorld();
    const base = w.landmarks.find((l) => l.dockable)!;
    w.player.pos = { ...base.pos };
    w.player.speed = 0;
    applyCommand(w, { c: 'dock' });

    const shield = w.player.shield;
    run(w, 5);
    expect(w.player.shield).toBe(shield);
  });
});

describe('reactor power', () => {
  it('is linear and honest — zero power is a dead system', () => {
    const w = createWorld();
    const stats = effectiveStats(w.player);

    applyCommand(w, { c: 'power', system: 'engines', v: 0 });
    applyCommand(w, { c: 'throttle', v: 1 });
    run(w, 6);

    expect(w.player.speed).toBe(0);

    // The reactor caps the total, so the other rows have to give the power up first.
    applyCommand(w, { c: 'power', system: 'weapons', v: 0 });
    applyCommand(w, { c: 'power', system: 'shields', v: 0 });
    applyCommand(w, { c: 'power', system: 'engines', v: 2 });
    run(w, 6);
    // Double power, double the rated top speed.
    expect(w.player.speed).toBeGreaterThan(stats.maxSpeed * 1.5);
  });

  it('does not let weapons power scale beam DAMAGE as well as recharge', () => {
    const damageAt = (power: number): number => {
      const { w, e } = withEnemy(3000);
      w.player.heading = 0;
      w.player.beamCharge = 1;
      w.player.targetId = e.id;
      applyCommand(w, { c: 'power', system: 'weapons', v: power });

      const before = e.hull + e.shield;
      applyCommand(w, { c: 'fireBeam' });
      return before - (e.hull + e.shield);
    };

    // Power drives how OFTEN the beam fires, never how hard it hits.
    expect(damageAt(2)).toBeCloseTo(damageAt(0.5), 5);
  });
});

describe('docked ships are safe', () => {
  const docked = (): World => {
    const w = createWorld();
    const base = w.landmarks.find((l) => l.dockable)!;
    w.player.pos = { ...base.pos };
    w.player.speed = 0;
    applyCommand(w, { c: 'dock' });
    return w;
  };

  it('cannot be damaged', () => {
    const w = docked();
    const hull = w.player.hull;
    applyDamage(w.player, 9999, true, 0, w.events);
    expect(w.player.hull).toBe(hull);
  });

  it('cannot fire', () => {
    const w = docked();
    w.enemies.push({ ...withEnemy(3000).e, pos: { x: w.player.pos.x + 3000, y: 0, z: w.player.pos.z } });
    const e = w.enemies[0]!;
    w.player.targetId = e.id;
    w.player.heading = 0;
    w.player.beamCharge = 1;

    const before = e.hull + e.shield;
    applyCommand(w, { c: 'fireBeam' });
    expect(e.hull + e.shield).toBe(before);
  });

  it('drops its protection on undock', () => {
    const w = docked();
    applyCommand(w, { c: 'dock' });
    applyDamage(w.player, 10, true, 0, w.events);
    expect(w.player.hull).toBeLessThan(w.player.maxHull);
  });
});

describe('ramming', () => {
  it('lands once per collision, not every tick while overlapping', () => {
    const { w, e } = withEnemy(COLLISION_RADIUS);
    w.player.shield = 0;
    e.pos = { x: COLLISION_RADIUS, y: 0, z: 0 };

    const hullBefore = w.player.hull;
    // Pin the two together for a full second and count what it costs.
    for (let i = 0; i < 60; i++) {
      e.pos = { x: COLLISION_RADIUS, y: 0, z: 0 };
      step(w, TICK_DT);
    }

    const dealt = hullBefore - w.player.hull;
    // One ram at rest is 0.5x. Per-tick application would have been ~2900.
    expect(dealt).toBeGreaterThan(0);
    expect(dealt).toBeLessThanOrEqual(RAM_DAMAGE * RAM_SPEED_MAX + 1);
  });

  it('hurts more at speed than at rest', () => {
    const hit = (speed: number): number => {
      const { w, e } = withEnemy(COLLISION_RADIUS);
      w.player.shield = 0;
      w.player.speed = speed;
      const before = w.player.hull;
      step(w, TICK_DT);
      void e;
      return before - w.player.hull;
    };

    expect(hit(2100)).toBeGreaterThan(hit(0) * 1.5);
  });

  it('re-arms once the ships separate', () => {
    const { w, e } = withEnemy(COLLISION_RADIUS);
    w.player.shield = 0;
    step(w, TICK_DT);
    const afterFirst = w.player.hull;

    // Fly apart, then collide again — the second impact must also land.
    e.pos = { x: 90000, y: 0, z: 0 };
    step(w, TICK_DT);
    expect(w.touching).toHaveLength(0);

    e.pos = { x: COLLISION_RADIUS, y: 0, z: 0 };
    step(w, TICK_DT);
    expect(w.player.hull).toBeLessThan(afterFirst);
  });
});
