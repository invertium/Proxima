// M2 — hostile AI: state machine, strafe runs, torpedo volleys, armour.

import { describe, expect, it } from 'vitest';
import { applyCommand, createWorld, step } from '../src/sim/world';
import {
  ENEMIES,
  ENEMY_TORPEDO_SPEED,
  SPAWN_GRACE,
  STRAFE_BREAKOFF_DISTANCE,
  STRAFE_PASS_DISTANCE,
  TICK_DT,
  VOLLEY_GAP,
  VOLLEY_SIZE,
} from '../src/sim/data';
import { dist } from '../src/sim/math';
import type { EnemyShip, EnemyType, World } from '../src/sim/types';

/**
 * A hostile of one archetype, parked at `range` from a stationary player, with no
 * campaign director running. Keeps each behaviour test to a single ship.
 */
const duel = (type: EnemyType, range: number, opts: { grace?: number } = {}): { w: World; e: EnemyShip } => {
  const w = createWorld({ seed: 3 });
  // Park the player far from any landmark so the director never spawns a fleet.
  w.player.pos = { x: 0, y: 0, z: 0 };
  w.missionIndex = 99;

  const def = ENEMIES[type];
  const e: EnemyShip = {
    kind: 'enemy',
    id: w.nextId++,
    enemyType: type,
    pos: { x: range, y: 0, z: 0 },
    heading: Math.PI, // facing back toward the player at the origin
    hull: def.maxHull,
    maxHull: def.maxHull,
    shield: def.maxShield,
    maxShield: def.maxShield,
    alive: true,
    fireCooldown: def.fireInterval,
    graceTimer: opts.grace ?? 0,
    rewarded: false,
    aiState: 'approach',
    strafeSide: 1,
    volleyRemaining: 0,
    volleyTimer: 0,
  };
  w.enemies.push(e);
  return { w, e };
};

const run = (w: World, seconds: number): void => {
  for (let i = 0; i < Math.round(seconds / TICK_DT); i++) step(w, TICK_DT);
};

/** Counts hostile beam shots over a window. Shield regen refills fast enough that
 *  pool levels are a poor proxy for 'did it shoot'. */
const countEnemyBeams = (w: World, seconds: number): number => {
  let shots = 0;
  for (let i = 0; i < Math.round(seconds / TICK_DT); i++) {
    step(w, TICK_DT);
    shots += w.events.filter((e) => e.t === 'beam' && !e.friendly).length;
  }
  return shots;
};

describe('spawn grace', () => {
  it('holds fire after spawning so an encounter never opens with a volley', () => {
    const { w } = duel('cruiser', 5000, { grace: SPAWN_GRACE });

    expect(countEnemyBeams(w, SPAWN_GRACE * 0.8)).toBe(0);
    // Past the grace it engages normally.
    expect(countEnemyBeams(w, SPAWN_GRACE * 0.4 + 8)).toBeGreaterThan(0);
  });
});

describe('standoff archetypes', () => {
  it('a cruiser closes to its standoff ring and then holds', () => {
    const def = ENEMIES.cruiser;
    const { w, e } = duel('cruiser', 20000);

    expect(e.aiState).toBe('approach');
    run(w, 30);

    expect(e.aiState).toBe('engage');
    // It should be sitting at roughly its standoff distance, not on top of the player.
    expect(dist(e.pos, w.player.pos)).toBeLessThan(def.standoffDistance * 1.3);
    expect(dist(e.pos, w.player.pos)).toBeGreaterThan(def.standoffDistance * 0.5);
  });

  it('never bores through the player', () => {
    const { w, e } = duel('cruiser', 3000);
    let closest = Infinity;
    for (let i = 0; i < 60 * 30; i++) {
      step(w, TICK_DT);
      closest = Math.min(closest, dist(e.pos, w.player.pos));
    }
    // The separation bubble pushes it back out; it must never reach the hull.
    expect(closest).toBeGreaterThan(400);
  });
});

describe('strafe runs', () => {
  it('a scout dives past, overshoots, breaks off, and comes back', () => {
    const { w, e } = duel('scout', 12000);
    const seen = new Set<string>();

    for (let i = 0; i < 60 * 60; i++) {
      step(w, TICK_DT);
      seen.add(e.aiState);
    }

    expect(seen.has('approach')).toBe(true);
    expect(seen.has('overshoot')).toBe(true);
    // A strafer must actually leave after a pass rather than parking on the player.
    expect(seen.has('engage')).toBe(true);
  });

  it('commits to the pass inside the pass distance and breaks off beyond the breakoff', () => {
    const { w, e } = duel('scout', STRAFE_PASS_DISTANCE - 200);
    step(w, TICK_DT);
    expect(e.aiState).toBe('overshoot');

    // Once far enough out again it turns around, flipping the side it crosses on.
    const side = e.strafeSide;
    e.pos = { x: STRAFE_BREAKOFF_DISTANCE + 1000, y: 0, z: 0 };
    step(w, TICK_DT);
    expect(e.aiState).toBe('approach');
    expect(e.strafeSide).toBe(-side);
  });

  it('holds fire through the loop-out', () => {
    const { w, e } = duel('scout', STRAFE_PASS_DISTANCE - 200);
    step(w, TICK_DT);
    expect(e.aiState).toBe('overshoot');

    const shield = w.player.shield;
    // Pin it in overshoot and let plenty of fire intervals elapse.
    for (let i = 0; i < 60 * 5; i++) {
      e.pos = { x: STRAFE_PASS_DISTANCE - 200, y: 0, z: 0 };
      e.aiState = 'overshoot';
      step(w, TICK_DT);
    }
    expect(w.player.shield).toBe(shield);
  });
});

describe('torpedo volleys', () => {
  /** Distinct hostile torpedo ids seen over a window — they expire, so a single
   *  end-of-run snapshot undercounts a salvo. */
  const launchedIds = (w: World, seconds: number): Set<number> => {
    const ids = new Set<number>();
    for (let i = 0; i < Math.round(seconds / TICK_DT); i++) {
      step(w, TICK_DT);
      for (const t of w.torpedoes) if (!t.friendly) ids.add(t.id);
    }
    return ids;
  };

  it('a gunship fires exactly a volley of three', () => {
    const def = ENEMIES.gunship;
    const { w, e } = duel('gunship', 5000);

    // The gunship's fire interval is 9s, so the first salvo is a long way in.
    const ids = launchedIds(w, def.fireInterval + VOLLEY_GAP * VOLLEY_SIZE + 0.5);

    expect(e.volleyRemaining).toBe(0);
    expect(ids.size).toBe(VOLLEY_SIZE);
  });

  it('launches slow torpedoes the helm can outrun', () => {
    const { w } = duel('gunship', 5000);
    run(w, ENEMIES.gunship.fireInterval + 0.2);

    const torp = w.torpedoes.find((t) => !t.friendly);
    expect(torp?.speed).toBe(ENEMY_TORPEDO_SPEED);
    // Slower than the interceptor's 2100 top speed — that's the counterplay.
    expect(torp!.speed).toBeLessThan(2100);
  });

  it('finishes a started volley even if the player breaks range', () => {
    const { w, e } = duel('gunship', 5000);
    e.volleyRemaining = VOLLEY_SIZE;
    e.volleyTimer = 0;

    // Teleport the player far outside engage range mid-salvo.
    w.player.pos = { x: 0, y: 0, z: 900000 };
    run(w, VOLLEY_GAP * VOLLEY_SIZE + 0.2);

    expect(e.volleyRemaining).toBe(0);
  });
});

describe('cruiser armour', () => {
  it('halves beam damage until Science scans the weakpoint', () => {
    const fire = (scanned: boolean): number => {
      const { w, e } = duel('cruiser', 3000);
      // Point the bow at it and give the beam a full charge.
      w.player.heading = 0;
      w.player.beamCharge = 1;
      w.player.targetId = e.id;
      if (scanned) w.player.scanned.push(e.id);

      const before = e.shield + e.hull;
      applyCommand(w, { c: 'fireBeam' });
      return before - (e.shield + e.hull);
    };

    const armored = fire(false);
    const exposed = fire(true);

    expect(armored).toBeGreaterThan(0);
    expect(armored).toBeCloseTo(exposed * ENEMIES.cruiser.armoredBeamMultiplier, 4);
  });
});

describe('derelicts', () => {
  it('never move and never shoot', () => {
    const { w, e } = duel('derelict', 4000);
    const pos = { ...e.pos };
    const shield = w.player.shield;

    run(w, 30);

    expect(e.pos).toEqual(pos);
    expect(w.player.shield).toBe(shield);
    expect(e.aiState).toBe('idle');
  });
});
