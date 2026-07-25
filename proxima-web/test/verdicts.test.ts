// Every refusal carries a reason.
//
// The C++ answered each station request with {ok:false, reason:"target outside firing
// arc"}. The port dropped refused commands silently, which leaves a crew unable to tell
// a bad shot from a broken link — the worst possible failure mode on a console you
// operate by feel.

import { describe, expect, it } from 'vitest';
import { applyCommand, createWorld, queueCommand, step } from '../src/sim/world';
import { applyDamage } from '../src/sim/combat';
import { ENEMIES, TICK_DT } from '../src/sim/data';
import type { Command, EnemyShip, World } from '../src/sim/types';

const refusal = (w: World, cmd: Command): string | null => {
  const v = applyCommand(w, cmd);
  return v.ok ? null : v.reason;
};

const withTarget = (range: number, heading = 0): { w: World; e: EnemyShip } => {
  const w = createWorld({ seed: 5 });
  w.player.pos = { x: 0, y: 0, z: 0 };
  w.player.heading = heading;
  w.missionIndex = 99;

  const def = ENEMIES.gunship;
  const e: EnemyShip = {
    kind: 'enemy',
    id: w.nextId++,
    enemyType: 'gunship',
    pos: { x: range, y: 0, z: 0 },
    heading: Math.PI,
    hull: 500,
    maxHull: 500,
    shield: 0,
    maxShield: def.maxShield,
    alive: true,
    fireCooldown: 9999,
    graceTimer: 9999,
    rewarded: false,
    callsign: 'VIPER-1',
    aiState: 'idle',
    strafeSide: 1,
    volleyRemaining: 0,
    volleyTimer: 0,
  };
  w.enemies.push(e);
  return { w, e };
};

describe('weapons refusals', () => {
  it('says when nothing is locked', () => {
    const { w } = withTarget(3000);
    expect(refusal(w, { c: 'fireBeam' })).toBe('no target locked');
  });

  it('says when the beam is still charging', () => {
    const { w, e } = withTarget(3000);
    w.player.targetId = e.id;
    w.player.beamCharge = 0.4;
    expect(refusal(w, { c: 'fireBeam' })).toBe('beam still charging');
  });

  it('distinguishes out of arc from out of range', () => {
    const far = withTarget(90000);
    far.w.player.targetId = far.e.id;
    expect(refusal(far.w, { c: 'fireBeam' })).toBe('target out of beam range');

    // Same range, bow pointed the other way.
    const behind = withTarget(3000, Math.PI);
    behind.w.player.targetId = behind.e.id;
    expect(refusal(behind.w, { c: 'fireBeam' })).toBe('target outside firing arc');
  });

  it('says when the tubes are empty or reloading', () => {
    const { w, e } = withTarget(3000);
    w.player.targetId = e.id;

    w.player.torpedoReload = 2.4;
    expect(refusal(w, { c: 'fireTorpedo' })).toMatch(/reloading/);

    w.player.torpedoReload = 0;
    w.player.torpedoAmmo = 0;
    expect(refusal(w, { c: 'fireTorpedo' })).toBe('tubes empty');
  });
});

describe('helm refusals', () => {
  it('says why docking failed', () => {
    const w = createWorld();
    w.missionIndex = 99;
    expect(refusal(w, { c: 'dock' })).toBe('no starbase in docking range');

    const base = w.landmarks.find((l) => l.dockable)!;
    w.player.pos = { ...base.pos };
    w.player.speed = 2000;
    expect(refusal(w, { c: 'dock' })).toBe('too fast to dock — all stop first');
  });

  it('reports the warp charge rather than doing nothing', () => {
    const w = createWorld();
    w.player.warpCharge = 0.42;
    expect(refusal(w, { c: 'warp' })).toBe('warp core at 42%');
  });

  it('says when thrusters are not installed', () => {
    const w = createWorld();
    expect(refusal(w, { c: 'strafe', v: 1 })).toBe('manoeuvring thrusters not installed');
  });

  it('says when there are no orders to accept', () => {
    const w = createWorld();
    w.missionIndex = 99;
    expect(refusal(w, { c: 'acceptObjective' })).toBe('no orders to accept');
  });
});

describe('engineering refusals', () => {
  it('explains a rejected drydock purchase', () => {
    const w = createWorld();
    expect(refusal(w, { c: 'buyUpgrade', id: 'hull' })).toBe(
      'drydock only available while docked',
    );

    const base = w.landmarks.find((l) => l.dockable)!;
    w.player.pos = { ...base.pos };
    w.player.speed = 0;
    applyCommand(w, { c: 'dock' });
    expect(refusal(w, { c: 'buyUpgrade', id: 'hull' })).toBe('needs 200 credits');
  });

  it('explains a rejected weld', () => {
    const w = createWorld();
    expect(refusal(w, { c: 'weld', phase: 0.05 })).toBe('missed the green');

    applyCommand(w, { c: 'weld', phase: 0.5 });
    expect(refusal(w, { c: 'weld', phase: 0.5 })).toBe('welder still cycling');
  });

  it('says when the reactor has no headroom left', () => {
    const w = createWorld();
    // engines/weapons/shields all start at 1.0 against a budget of 3.0.
    expect(refusal(w, { c: 'power', system: 'engines', v: 2 })).toMatch(/reactor at capacity/);
  });
});

describe('ack plumbing', () => {
  it('surfaces refusals on the tick, tagged with the sender id', () => {
    const w = createWorld();
    w.missionIndex = 99;

    queueCommand(w, { c: 'fireBeam' }, 4242);
    step(w, TICK_DT);

    expect(w.acks).toHaveLength(1);
    expect(w.acks[0]).toMatchObject({ id: 4242, ok: false, reason: 'no target locked' });
  });

  it('stays quiet when a command succeeds', () => {
    const w = createWorld();
    w.missionIndex = 99;

    queueCommand(w, { c: 'turn', v: 1 }, 7);
    step(w, TICK_DT);

    expect(w.acks).toHaveLength(0);
  });

  it('explains itself once the ship is lost', () => {
    const w = createWorld();
    applyDamage(w.player, 9999, true, 0, w.events);
    step(w, TICK_DT);

    expect(refusal(w, { c: 'fireBeam' })).toBe('the run is over');
  });
});
