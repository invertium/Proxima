// Headless simulation tests. These are the primary verification surface for gameplay
// work: no browser, no GPU, no editor — `npm test` runs the whole campaign in ~1s.

import { describe, expect, it } from 'vitest';
import { applyCommand, createWorld, snapshot, step } from '../src/sim/world';
import { applyDamage, shieldMitigation } from '../src/sim/combat';
import { CAMPAIGN, TICK_DT, MAX_MITIGATION, REACTOR_BUDGET } from '../src/sim/data';
import { dist } from '../src/sim/math';
import type { Command, World } from '../src/sim/types';

/** Runs `seconds` of simulation, optionally issuing commands each tick. */
const run = (world: World, seconds: number, each?: (w: World, i: number) => Command[] | void): void => {
  const ticks = Math.round(seconds / TICK_DT);
  for (let i = 0; i < ticks; i++) {
    for (const cmd of each?.(world, i) ?? []) applyCommand(world, cmd);
    step(world, TICK_DT);
  }
};

describe('determinism', () => {
  it('two worlds with the same seed and input produce identical state', () => {
    const a = createWorld({ seed: 42 });
    const b = createWorld({ seed: 42 });
    const drive = () => [{ c: 'throttle', v: 1 } as Command, { c: 'turn', v: 0.3 } as Command];

    run(a, 30, drive);
    run(b, 30, drive);

    expect(snapshot(a)).toEqual(snapshot(b));
  });

  it('a different seed diverges once a fleet spawns', () => {
    const a = createWorld({ seed: 1 });
    const b = createWorld({ seed: 2 });
    const drive = () => [{ c: 'throttle', v: 1 } as Command];

    run(a, 60, drive);
    run(b, 60, drive);

    // Same helm input means the same player track; the fleet layout is seeded.
    expect(a.enemies.length).toBe(b.enemies.length);
    if (a.enemies.length > 0) {
      expect(a.enemies[0]!.pos).not.toEqual(b.enemies[0]!.pos);
    }
  });
});

describe('helm', () => {
  it('throttle eases toward max speed rather than snapping', () => {
    const w = createWorld();
    applyCommand(w, { c: 'throttle', v: 1 });

    step(w, TICK_DT);
    const afterOneTick = w.player.speed;
    expect(afterOneTick).toBeGreaterThan(0);
    expect(afterOneTick).toBeLessThan(200); // acceleration-limited, not instant

    run(w, 5);
    expect(w.player.speed).toBeGreaterThan(2000); // interceptor maxSpeed 2100 at nominal power
  });

  it('engine power scales achievable top speed', () => {
    const full = createWorld();
    const starved = createWorld();
    applyCommand(starved, { c: 'power', system: 'engines', v: 0 });

    run(full, 8, () => [{ c: 'throttle', v: 1 }]);
    run(starved, 8, () => [{ c: 'throttle', v: 1 }]);

    expect(starved.player.speed).toBeLessThan(full.player.speed * 0.7);
  });
});

describe('reactor', () => {
  it('total allocation can never exceed the reactor budget', () => {
    const w = createWorld();
    applyCommand(w, { c: 'power', system: 'engines', v: 2 });
    applyCommand(w, { c: 'power', system: 'weapons', v: 2 });
    applyCommand(w, { c: 'power', system: 'shields', v: 2 });

    const total = w.player.power.engines + w.player.power.weapons + w.player.power.shields;
    expect(total).toBeLessThanOrEqual(REACTOR_BUDGET + 1e-6);
  });
});

describe('damage model', () => {
  it('shield power mitigates, and mitigation is capped', () => {
    expect(shieldMitigation(0)).toBe(0);
    expect(shieldMitigation(1)).toBeCloseTo(0.35);
    expect(shieldMitigation(99)).toBe(MAX_MITIGATION);
  });

  it('the shield pool absorbs before hull takes anything', () => {
    const w = createWorld();
    const p = w.player;
    p.power.shields = 0; // no mitigation, so the arithmetic is exact
    const hullBefore = p.hull;

    // 10 damage against a 50-point shield pool: hull untouched.
    applyCommandDamage(w, 10);
    expect(p.shield).toBe(40);
    expect(p.hull).toBe(hullBefore);

    // Strip the pool, then overflow reaches hull.
    applyCommandDamage(w, 60);
    expect(p.shield).toBe(0);
    expect(p.hull).toBeLessThan(hullBefore);
  });
});

// Small helper so the damage test doesn't need a live hostile in arc.
const applyCommandDamage = (w: World, amount: number): void => {
  applyDamage(w.player, amount, false, w.player.power.shields, w.events);
};

describe('open-sector director', () => {
  it('spawns the first fleet on proximity, not on a level load', () => {
    const w = createWorld();
    expect(w.enemies.length).toBe(0);

    // Teleport onto the first objective the way a warp would.
    w.player.pos = { ...w.landmarks[0]!.pos };
    step(w, TICK_DT);
    applyCommand(w, { c: 'acceptObjective' });
    step(w, TICK_DT);

    expect(w.encounterLive).toBe(true);
    expect(w.enemies.length).toBe(CAMPAIGN[0]!.enemies.length);
  });

  it('advances seamlessly when a fleet is wiped and only wins at the last system', () => {
    const w = createWorld();

    for (let m = 0; m < CAMPAIGN.length; m++) {
      w.player.pos = { ...w.landmarks[m]!.pos };
      step(w, TICK_DT);
      // Arriving hails the bridge; the fight starts only once the crew accepts.
      expect(w.objectiveOffered).toBe(true);
      applyCommand(w, { c: 'acceptObjective' });
      step(w, TICK_DT);
      expect(w.encounterLive).toBe(true);

      for (const e of w.enemies) e.hull = 0;
      for (const e of w.enemies) e.alive = false;
      step(w, TICK_DT);

      // No reload, no outcome screen — except at the very end.
      if (m < CAMPAIGN.length - 1) {
        expect(w.phase).toBe('playing');
        expect(w.missionIndex).toBe(m + 1);
      }
    }

    expect(w.phase).toBe('victory');
  });

  it('banks credits and xp for each kill', () => {
    const w = createWorld();
    w.player.pos = { ...w.landmarks[0]!.pos };
    step(w, TICK_DT);
    applyCommand(w, { c: 'acceptObjective' });
    step(w, TICK_DT);

    const before = w.player.credits;
    // Kill through the damage path so the kill events the reward hook listens for fire.
    for (const e of w.enemies) applyDamage(e, e.hull, true, 0, w.events);
    step(w, TICK_DT);

    expect(w.player.credits).toBeGreaterThan(before);
    expect(w.player.xp).toBeGreaterThan(0);
  });
});

describe('docking', () => {
  it('repairs and resupplies at the starbase, and refuses at speed', () => {
    const w = createWorld();
    const base = w.landmarks.find((l) => l.dockable)!;
    w.player.pos = { ...base.pos };
    w.player.hull = 10;
    w.player.torpedoAmmo = 0;

    // Too fast to dock.
    w.player.speed = 2000;
    applyCommand(w, { c: 'dock' });
    expect(w.player.docked).toBe(false);

    w.player.speed = 0;
    applyCommand(w, { c: 'dock' });
    expect(w.player.docked).toBe(true);
    expect(w.player.hull).toBe(w.player.maxHull);
    expect(w.player.torpedoAmmo).toBeGreaterThan(0);
  });
});

describe('warp', () => {
  it('jumps a fixed distance along the bow and spends the charge', () => {
    const w = createWorld();
    const from = { ...w.player.pos };

    applyCommand(w, { c: 'warp' });
    expect(dist(from, w.player.pos)).toBeCloseTo(18000, 0);
    expect(w.player.warpCharge).toBe(0);

    // Second jump is refused until it spools back up.
    const after = { ...w.player.pos };
    applyCommand(w, { c: 'warp' });
    expect(w.player.pos).toEqual(after);
  });
});

describe('snapshot', () => {
  it('is structurally cloneable for the worker and network boundary', () => {
    const w = createWorld();
    w.player.pos = { ...w.landmarks[0]!.pos };
    step(w, TICK_DT);
    applyCommand(w, { c: 'acceptObjective' });
    step(w, TICK_DT);

    const snap = snapshot(w);
    expect(() => structuredClone(snap)).not.toThrow();
    expect(snap.contacts.length).toBeGreaterThan(0);
    expect(snap.objective?.name).toBe(CAMPAIGN[0]!.landmarkName);
  });
});
