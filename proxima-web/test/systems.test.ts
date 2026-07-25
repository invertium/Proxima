// M1 — ship systems: damage control, science, upgrades, rank, turret, strafe.

import { describe, expect, it } from 'vitest';
import { applyCommand, createWorld, rollSystemDamage, snapshot, step } from '../src/sim/world';
import { effectiveStats, upgradeBonus } from '../src/sim/stats';
import {
  ALARM_HULL_FRACTION,
  DAMAGED_MULTIPLIER,
  SCAN_DURATION,
  TICK_DT,
  TURRET_INTERVAL,
  UPGRADES,
  WELDS_PER_SYSTEM_REPAIR,
  XP_PER_RANK,
  rankFromXp,
  upgradeCost,
  upgradeRankReq,
} from '../src/sim/data';
import type { Command, World } from '../src/sim/types';

const run = (world: World, seconds: number, each?: () => Command[]): void => {
  const ticks = Math.round(seconds / TICK_DT);
  for (let i = 0; i < ticks; i++) {
    for (const cmd of each?.() ?? []) applyCommand(world, cmd);
    step(world, TICK_DT);
  }
};

/** Puts the player at the starbase with money and rank, ready to buy. */
const atDrydock = (credits = 100000, xp = XP_PER_RANK * 5): World => {
  const w = createWorld();
  const base = w.landmarks.find((l) => l.dockable)!;
  w.player.pos = { ...base.pos };
  w.player.speed = 0;
  w.player.credits = credits;
  w.player.xp = xp;
  applyCommand(w, { c: 'dock' });
  return w;
};

describe('rank', () => {
  it('is one plus XP over the per-rank step', () => {
    expect(rankFromXp(0)).toBe(1);
    expect(rankFromXp(XP_PER_RANK - 1)).toBe(1);
    expect(rankFromXp(XP_PER_RANK)).toBe(2);
    expect(rankFromXp(XP_PER_RANK * 3)).toBe(4);
  });
});

describe('upgrades', () => {
  it('prices and rank-gates each tier the way the catalogue says', () => {
    const beam = UPGRADES.find((u) => u.id === 'beam_damage')!;
    expect(upgradeCost(beam, 0)).toBe(150);
    expect(upgradeCost(beam, 1)).toBe(300);
    expect(upgradeCost(beam, 2)).toBe(450);
    expect(upgradeRankReq(0)).toBe(1);
    expect(upgradeRankReq(2)).toBe(3);
  });

  it('actually moves the stat it advertises', () => {
    const w = atDrydock();
    const before = effectiveStats(w.player).beamDamage;

    applyCommand(w, { c: 'buyUpgrade', id: 'beam_damage' });

    expect(w.player.upgrades['beam_damage']).toBe(1);
    expect(effectiveStats(w.player).beamDamage).toBeCloseTo(before + 8);
  });

  it('refuses a purchase without the credits', () => {
    const w = atDrydock(10);
    applyCommand(w, { c: 'buyUpgrade', id: 'hull' });
    expect(w.player.upgrades['hull']).toBeUndefined();
  });

  it('refuses a tier the crew has not earned the rank for', () => {
    // Rank 1 buys tier 1 only; tier 2 needs rank 2, i.e. 400 XP.
    const w = atDrydock(100000, 0);
    applyCommand(w, { c: 'buyUpgrade', id: 'hull' });
    expect(w.player.upgrades['hull']).toBe(1);

    applyCommand(w, { c: 'buyUpgrade', id: 'hull' });
    expect(w.player.upgrades['hull']).toBe(1);
  });

  it('stops at the catalogue max tier', () => {
    const w = atDrydock();
    for (let i = 0; i < 6; i++) applyCommand(w, { c: 'buyUpgrade', id: 'beam_damage' });
    expect(w.player.upgrades['beam_damage']).toBe(3);
  });

  it('raises the hull ceiling and gives the new plating immediately', () => {
    const w = atDrydock();
    const beforeMax = w.player.maxHull;

    applyCommand(w, { c: 'buyUpgrade', id: 'hull' });
    step(w, TICK_DT);

    expect(w.player.maxHull).toBe(beforeMax + 40);
    expect(w.player.hull).toBe(w.player.maxHull);
  });

  it('grows the reactor budget, letting the crew allocate more total power', () => {
    const w = atDrydock();
    applyCommand(w, { c: 'buyUpgrade', id: 'reactor' });

    applyCommand(w, { c: 'power', system: 'engines', v: 2 });
    applyCommand(w, { c: 'power', system: 'weapons', v: 2 });
    const total = w.player.power.engines + w.player.power.weapons + w.player.power.shields;

    expect(effectiveStats(w.player).reactorBudget).toBeCloseTo(3.5);
    expect(total).toBeLessThanOrEqual(3.5 + 1e-6);
    expect(total).toBeGreaterThan(3.0);
  });

  it('only grants strafe once the thrusters are bought', () => {
    const w = atDrydock();
    expect(effectiveStats(w.player).strafeSpeed).toBe(0);

    applyCommand(w, { c: 'buyUpgrade', id: 'strafe' });
    expect(effectiveStats(w.player).strafeSpeed).toBe(950);
  });

  it('sums bonuses across tiers', () => {
    const w = atDrydock();
    applyCommand(w, { c: 'buyUpgrade', id: 'beam_damage' });
    applyCommand(w, { c: 'buyUpgrade', id: 'beam_damage' });
    expect(upgradeBonus(w.player, 'beamDamage')).toBe(16);
  });
});

describe('drydock hulls', () => {
  it('charges once, then lets the crew switch back for free', () => {
    const w = atDrydock();
    const start = w.player.credits;

    applyCommand(w, { c: 'buyShip', type: 'gunboat' });
    expect(w.player.shipType).toBe('gunboat');
    expect(w.player.credits).toBe(start - 1800);

    applyCommand(w, { c: 'buyShip', type: 'interceptor' });
    applyCommand(w, { c: 'buyShip', type: 'gunboat' });
    expect(w.player.shipType).toBe('gunboat');
    expect(w.player.credits).toBe(start - 1800);
  });

  it('rank-gates the expensive hulls', () => {
    const w = atDrydock(100000, 0); // rank 1; gunboat needs rank 3
    applyCommand(w, { c: 'buyShip', type: 'gunboat' });
    expect(w.player.shipType).toBe('interceptor');
  });
});

describe('damage control', () => {
  it('a hull hit can knock out a system, and only ever one at a time per hit', () => {
    const w = createWorld({ seed: 5 });
    let damagedEvents = 0;

    // Roll many times; with p=0.35 this is certain to break all three eventually.
    for (let i = 0; i < 200; i++) {
      w.events.length = 0;
      rollSystemDamage(w, 10);
      damagedEvents += w.events.filter((e) => e.t === 'systemDamaged').length;
      expect(w.events.filter((e) => e.t === 'systemDamaged').length).toBeLessThanOrEqual(1);
    }

    expect(damagedEvents).toBe(3); // three systems exist; each breaks once
    expect(w.player.damaged).toEqual({ engine: true, weapons: true, sensors: true });
  });

  it('never breaks anything on a hit the shields fully absorbed', () => {
    const w = createWorld({ seed: 5 });
    for (let i = 0; i < 100; i++) rollSystemDamage(w, 0);
    expect(w.player.damaged).toEqual({ engine: false, weapons: false, sensors: false });
  });

  it('halves the stat belonging to the broken system', () => {
    const w = createWorld();
    const healthy = effectiveStats(w.player);

    w.player.damaged.engine = true;
    w.player.damaged.weapons = true;
    w.player.damaged.sensors = true;
    const hurt = effectiveStats(w.player);

    expect(hurt.maxSpeed).toBeCloseTo(healthy.maxSpeed * DAMAGED_MULTIPLIER);
    expect(hurt.beamRecharge).toBeCloseTo(healthy.beamRecharge * DAMAGED_MULTIPLIER);
    expect(hurt.scanRange).toBeCloseTo(healthy.scanRange * DAMAGED_MULTIPLIER);
  });

  it('takes exactly three welds to repair a system, in a fixed order', () => {
    const w = createWorld();
    w.player.damaged.weapons = true;
    w.player.damaged.sensors = true;

    // Engine is undamaged, so weapons is first in order.
    for (let i = 0; i < WELDS_PER_SYSTEM_REPAIR - 1; i++) {
      applyCommand(w, { c: 'weld' });
      expect(w.player.damaged.weapons).toBe(true);
    }
    applyCommand(w, { c: 'weld' });
    expect(w.player.damaged.weapons).toBe(false);
    expect(w.player.damaged.sensors).toBe(true);

    for (let i = 0; i < WELDS_PER_SYSTEM_REPAIR; i++) applyCommand(w, { c: 'weld' });
    expect(w.player.damaged.sensors).toBe(false);
  });

  it('spends welds on hull only once everything works', () => {
    const w = createWorld();
    w.player.hull = 20;

    applyCommand(w, { c: 'weld' });
    expect(w.player.hull).toBeGreaterThan(20);

    // With something broken, the same weld goes to the system instead.
    w.player.damaged.engine = true;
    const hull = w.player.hull;
    applyCommand(w, { c: 'weld' });
    expect(w.player.hull).toBe(hull);
  });

  it('reports hull-critical to the crew', () => {
    const w = createWorld();
    w.player.hull = w.player.maxHull * (ALARM_HULL_FRACTION - 0.05);
    step(w, TICK_DT);
    expect(snapshot(w).player.hullCritical).toBe(true);
  });
});

describe('science', () => {
  it('reveals a contact after holding the scan for its full duration', () => {
    const w = createWorld();
    w.player.pos = { ...w.landmarks[0]!.pos };
    step(w, TICK_DT);
    applyCommand(w, { c: 'acceptObjective' });
    step(w, TICK_DT);

    const contact = w.enemies[0]!;
    // Well inside scan range, but clear of the collision radius — parking on top of a
    // contact rams it, which would kill this one before the scan finished.
    w.player.pos = { x: contact.pos.x + 5000, y: 0, z: contact.pos.z };
    applyCommand(w, { c: 'scan', id: contact.id });

    run(w, SCAN_DURATION * 0.5);
    expect(snapshot(w).contacts.find((c) => c.id === contact.id)?.scanned).toBe(false);

    run(w, SCAN_DURATION * 0.6);
    expect(snapshot(w).contacts.find((c) => c.id === contact.id)?.scanned).toBe(true);
    expect(w.player.scanning).toBe(false);
  });

  it('loses progress if the contact drifts out of scan range', () => {
    const w = createWorld();
    w.player.pos = { ...w.landmarks[0]!.pos };
    step(w, TICK_DT);
    applyCommand(w, { c: 'acceptObjective' });
    step(w, TICK_DT);

    const contact = w.enemies[0]!;
    w.player.pos = { x: contact.pos.x + 5000, y: 0, z: contact.pos.z };
    applyCommand(w, { c: 'scan', id: contact.id });
    run(w, SCAN_DURATION * 0.5);
    expect(w.player.scanProgress).toBeGreaterThan(0);

    contact.pos = { x: contact.pos.x + 500000, y: 0, z: contact.pos.z };
    step(w, TICK_DT);
    expect(w.player.scanProgress).toBe(0);
  });
});

describe('auto-turret', () => {
  it('does nothing until bought, then fires on its own interval', () => {
    const w = atDrydock();
    applyCommand(w, { c: 'dock' }); // undock
    w.player.pos = { ...w.landmarks[0]!.pos };
    step(w, TICK_DT);
    applyCommand(w, { c: 'acceptObjective' });
    step(w, TICK_DT);

    const contact = w.enemies[0]!;
    w.player.pos = { x: contact.pos.x + 2000, y: 0, z: contact.pos.z };

    const hullBefore = contact.hull;
    run(w, TURRET_INTERVAL * 2);
    expect(contact.hull).toBe(hullBefore); // no turret installed

    applyCommand(w, { c: 'buyUpgrade', id: 'turret' });
    // buyUpgrade is dock-gated, so install it directly for the fire-rate check.
    w.player.upgrades['turret'] = 1;

    run(w, TURRET_INTERVAL * 2.5);
    expect(contact.hull).toBeLessThan(hullBefore);
  });
});
