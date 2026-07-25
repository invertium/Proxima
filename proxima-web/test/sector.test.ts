// M3 — sector life: objective offers, events, contracts, gravity, salvage.

import { describe, expect, it } from 'vitest';
import { applyCommand, createWorld, snapshot, step } from '../src/sim/world';
import { gravityPullAt } from '../src/sim/sector';
import { applyDamage } from '../src/sim/combat';
import {
  CAMPAIGN,
  CONTRACT_VISIT_RANGE,
  SALVAGE_COLLECT_RANGE,
  SALVAGE_CREDITS,
  SALVAGE_DURATION,
  TICK_DT,
  TRIGGER_RADIUS,
} from '../src/sim/data';
import type { Contract, World } from '../src/sim/types';

const run = (w: World, seconds: number): void => {
  for (let i = 0; i < Math.round(seconds / TICK_DT); i++) step(w, TICK_DT);
};

const dockedAtBase = (w: World): void => {
  const base = w.landmarks.find((l) => l.dockable)!;
  w.player.pos = { ...base.pos };
  w.player.speed = 0;
  applyCommand(w, { c: 'dock' });
};

describe('objective offer', () => {
  it('hails on arrival and waits for the crew to accept', () => {
    const w = createWorld();
    w.player.pos = { ...w.landmarks[0]!.pos };

    step(w, TICK_DT);
    expect(w.objectiveOffered).toBe(true);
    expect(w.encounterLive).toBe(false);
    expect(w.enemies.length).toBe(0);
    expect(snapshot(w).objective?.offered).toBe(true);

    applyCommand(w, { c: 'acceptObjective' });
    step(w, TICK_DT);

    expect(w.encounterLive).toBe(true);
    expect(w.enemies.length).toBe(CAMPAIGN[0]!.enemies.length);
  });

  it('does not hail from outside the trigger radius', () => {
    const w = createWorld();
    const l = w.landmarks[0]!;
    w.player.pos = { x: l.pos.x + TRIGGER_RADIUS + l.radius + 5000, y: 0, z: l.pos.z };

    run(w, 1);
    expect(w.objectiveOffered).toBe(false);
  });
});

describe('gravity', () => {
  it('pulls toward a body, and not at all from outside its influence', () => {
    // Isolate one body: the sector is dense enough that a point outside this planet's
    // well can still sit inside a neighbour's, which would mask the falloff.
    const w = createWorld();
    const planet = w.landmarks[0]!;
    w.landmarks = [planet];

    const near = gravityPullAt(w, { x: planet.pos.x + planet.radius * 2, y: 0, z: planet.pos.z });
    expect(near.x).toBeLessThan(0); // dragged back toward the body

    const far = gravityPullAt(w, { x: planet.pos.x + planet.radius * 20, y: 0, z: planet.pos.z });
    expect(Math.hypot(far.x, far.z)).toBe(0);
  });

  it('is zero at the surface, so a body never pins a ship against it', () => {
    const w = createWorld();
    const planet = w.landmarks[0]!;
    const atSurface = gravityPullAt(w, { x: planet.pos.x + planet.radius, y: 0, z: planet.pos.z });
    expect(Math.hypot(atSurface.x, atSurface.z)).toBeCloseTo(0, 6);
  });

  it('is always weak enough to fly out of', () => {
    const w = createWorld();
    const planet = w.landmarks[0]!;
    let strongest = 0;
    for (let r = planet.radius; r < planet.radius * 6; r += 100) {
      const pull = gravityPullAt(w, { x: planet.pos.x + r, y: 0, z: planet.pos.z });
      strongest = Math.max(strongest, Math.hypot(pull.x, pull.z));
    }
    // Far below the interceptor's 2100 top speed — a drift, never a trap.
    expect(strongest).toBeLessThan(400);
  });

  it('does not drag the ship off the docking clamps', () => {
    const w = createWorld();
    dockedAtBase(w);
    const pos = { ...w.player.pos };
    run(w, 5);
    expect(w.player.pos).toEqual(pos);
  });
});

describe('salvage', () => {
  it('pays out when the ship closes on the pod', () => {
    const w = createWorld();
    w.missionIndex = 99; // keep the director out of it
    w.activeEvent = 'salvage';
    w.eventPos = { x: w.player.pos.x + 5000, y: 0, z: w.player.pos.z };
    w.eventDeadline = w.time + SALVAGE_DURATION;

    const before = w.player.credits;
    run(w, 1);
    expect(w.player.credits).toBe(before); // still out of tractor range

    w.player.pos = { x: w.eventPos.x - SALVAGE_COLLECT_RANGE * 0.5, y: 0, z: w.eventPos.z };
    step(w, TICK_DT);

    expect(w.player.credits).toBe(before + SALVAGE_CREDITS);
    expect(w.activeEvent).toBe('none');
  });

  it('expires unclaimed once its window closes', () => {
    const w = createWorld();
    w.missionIndex = 99;
    w.activeEvent = 'salvage';
    w.eventPos = { x: w.player.pos.x + 400000, y: 0, z: w.player.pos.z };
    w.eventDeadline = w.time + 2;

    const before = w.player.credits;
    run(w, 3);

    expect(w.activeEvent).toBe('none');
    expect(w.player.credits).toBe(before);
  });
});

describe('combat events', () => {
  it('an interdiction spawns hostiles that do not block the campaign', () => {
    const w = createWorld();
    // Stand at the objective, hail, accept — then drop an event on top of it.
    w.player.pos = { ...w.landmarks[0]!.pos };
    step(w, TICK_DT);
    applyCommand(w, { c: 'acceptObjective' });
    step(w, TICK_DT);

    const fleetSize = w.enemies.length;
    w.activeEvent = 'interdiction';
    w.eventDeadline = w.time + 100;
    w.eventFleet = [];
    // Hand-place an event hostile so the test doesn't depend on the roll.
    const raider = { ...w.enemies[0]!, id: w.nextId++, alive: true, hull: 10 };
    w.enemies.push(raider);
    w.eventFleet.push(raider.id);

    expect(w.enemies.length).toBe(fleetSize + 1);

    // Wipe the campaign fleet only. The raider is still alive.
    for (const id of w.fleetIds) {
      const e = w.enemies.find((x) => x.id === id)!;
      applyDamage(e, e.hull, true, 0, w.events);
    }
    step(w, TICK_DT);

    // The objective must advance despite a live event hostile in the sector.
    expect(w.missionIndex).toBe(1);
    expect(w.enemies.some((e) => e.id === raider.id)).toBe(true);
  });
});

describe('contracts', () => {
  const sign = (w: World, c: Partial<Contract>): void => {
    dockedAtBase(w);
    w.offer = { type: 'patrol', targetA: 1, targetB: 2, stage: 0, ship: '', reward: 140, ...c };
    applyCommand(w, { c: 'acceptContract' });
  };

  it('posts a board offer on docking, and withdraws it on departure', () => {
    const w = createWorld();
    dockedAtBase(w);
    step(w, TICK_DT);

    expect(w.offer).not.toBeNull();
    expect(snapshot(w).offer?.text).toBeTruthy();

    applyCommand(w, { c: 'dock' }); // undock
    step(w, TICK_DT);
    expect(w.offer).toBeNull();
  });

  it('refuses to sign a second contract while one is running', () => {
    const w = createWorld();
    sign(w, { type: 'delivery', targetA: 1 });
    expect(w.contract?.type).toBe('delivery');

    w.offer = { type: 'bounty', targetA: 2, targetB: -1, stage: 0, ship: 'KRAIT', reward: 220 };
    applyCommand(w, { c: 'acceptContract' });
    expect(w.contract?.type).toBe('delivery');
  });

  it('patrol advances leg by leg and pays out at the second waypoint', () => {
    const w = createWorld();
    sign(w, { type: 'patrol', targetA: 1, targetB: 2 });
    applyCommand(w, { c: 'dock' }); // undock and fly

    const before = w.player.credits;

    w.player.pos = { ...w.landmarks[1]!.pos };
    step(w, TICK_DT);
    expect(w.contract?.stage).toBe(1);
    expect(w.player.credits).toBe(before);

    w.player.pos = { ...w.landmarks[2]!.pos };
    step(w, TICK_DT);
    expect(w.contract).toBeNull();
    expect(w.player.credits).toBe(before + 140);
  });

  it('delivery needs the cargo dropped and the ship docked home again', () => {
    const w = createWorld();
    sign(w, { type: 'delivery', targetA: 2, targetB: -1, reward: 160 });
    applyCommand(w, { c: 'dock' });

    const before = w.player.credits;

    // Docking home before delivering must not close it.
    dockedAtBase(w);
    step(w, TICK_DT);
    expect(w.contract).not.toBeNull();
    applyCommand(w, { c: 'dock' });

    w.player.pos = { ...w.landmarks[2]!.pos };
    step(w, TICK_DT);
    expect(w.contract?.stage).toBe(1);

    dockedAtBase(w);
    step(w, TICK_DT);
    expect(w.contract).toBeNull();
    expect(w.player.credits).toBe(before + 160);
  });

  it('bounty spawns its target and pays out when it dies', () => {
    const w = createWorld();
    sign(w, { type: 'bounty', targetA: 1, targetB: -1, ship: 'KRAIT', reward: 220 });

    expect(w.bountyId).not.toBeNull();
    const bounty = w.enemies.find((e) => e.id === w.bountyId)!;
    expect(bounty).toBeDefined();

    const before = w.player.credits;
    applyDamage(bounty, bounty.hull + bounty.shield, true, 0, w.events);
    step(w, TICK_DT);

    expect(w.contract).toBeNull();
    // The bounty reward lands on top of the kill bounty for the hull itself.
    expect(w.player.credits).toBeGreaterThanOrEqual(before + 220);
  });

  it('a waypoint only counts inside the visit range', () => {
    const w = createWorld();
    sign(w, { type: 'patrol', targetA: 1, targetB: 2 });
    applyCommand(w, { c: 'dock' });

    const l = w.landmarks[1]!;
    w.player.pos = { x: l.pos.x + CONTRACT_VISIT_RANGE * 2, y: 0, z: l.pos.z };
    step(w, TICK_DT);
    expect(w.contract?.stage).toBe(0);
  });
});
