// M4 — campaign flow: save/load round-tripping, migration, difficulty, skirmish,
// and the defeat path.

import { describe, expect, it } from 'vitest';
import { applyCommand, createWorld, snapshot, step } from '../src/sim/world';
import { SAVE_VERSION, applySave, migrate, toSave } from '../src/sim/save';
import { applyDamage } from '../src/sim/combat';
import { DIFFICULTY_SCALE, ENEMIES, TICK_DT, WAVE_INTERVAL } from '../src/sim/data';
import type { World } from '../src/sim/types';

const run = (w: World, seconds: number): void => {
  for (let i = 0; i < Math.round(seconds / TICK_DT); i++) step(w, TICK_DT);
};

/** Plays a world into a non-trivial mid-campaign state worth persisting. */
const midCampaign = (): World => {
  const w = createWorld({ seed: 77, difficulty: 'admiral' });
  const base = w.landmarks.find((l) => l.dockable)!;

  w.player.pos = { ...base.pos };
  w.player.speed = 0;
  w.player.credits = 5000;
  w.player.xp = 1600;
  applyCommand(w, { c: 'dock' });
  applyCommand(w, { c: 'buyUpgrade', id: 'beam_damage' });
  applyCommand(w, { c: 'buyUpgrade', id: 'hull' });
  applyCommand(w, { c: 'buyShip', type: 'gunboat' });
  w.missionIndex = 2;
  return w;
};

describe('save/load', () => {
  it('round-trips everything the campaign depends on', () => {
    const w = midCampaign();
    const save = toSave(w);

    const restored = createWorld({ seed: save.seed, difficulty: save.difficulty, shipType: save.shipType });
    applySave(restored, save);

    expect(restored.missionIndex).toBe(w.missionIndex);
    expect(restored.difficulty).toBe('admiral');
    expect(restored.player.shipType).toBe('gunboat');
    expect(restored.player.ownedShips).toEqual(w.player.ownedShips);
    expect(restored.player.credits).toBe(w.player.credits);
    expect(restored.player.xp).toBe(w.player.xp);
    expect(restored.player.upgrades).toEqual(w.player.upgrades);
  });

  it('keeps a signed contract across a reload', () => {
    const w = createWorld();
    const base = w.landmarks.find((l) => l.dockable)!;
    w.player.pos = { ...base.pos };
    w.player.speed = 0;
    applyCommand(w, { c: 'dock' });
    w.offer = { type: 'patrol', targetA: 1, targetB: 2, stage: 0, ship: '', reward: 140 };
    applyCommand(w, { c: 'acceptContract' });

    const restored = createWorld();
    applySave(restored, toSave(w));

    expect(restored.contract?.type).toBe('patrol');
    expect(restored.contract?.targetA).toBe(1);
  });

  it('drops a bounty target id on load, since that ship no longer exists', () => {
    const w = createWorld();
    const base = w.landmarks.find((l) => l.dockable)!;
    w.player.pos = { ...base.pos };
    w.player.speed = 0;
    applyCommand(w, { c: 'dock' });
    w.offer = { type: 'bounty', targetA: 1, targetB: -1, stage: 0, ship: 'KRAIT', reward: 220 };
    applyCommand(w, { c: 'acceptContract' });
    expect(w.bountyId).not.toBeNull();

    const restored = createWorld();
    applySave(restored, toSave(w));
    expect(restored.bountyId).toBeNull();
    expect(restored.contract?.type).toBe('bounty');
  });

  it('is a plain JSON value, so any store can hold it', () => {
    const save = toSave(midCampaign());
    expect(JSON.parse(JSON.stringify(save))).toEqual(save);
  });
});

describe('save migration', () => {
  it('accepts a current save', () => {
    const save = toSave(createWorld());
    expect(migrate(JSON.parse(JSON.stringify(save)))?.version).toBe(SAVE_VERSION);
  });

  it('fills defaults for fields an older save lacks', () => {
    const migrated = migrate({ version: 1, missionIndex: 2 });
    expect(migrated).not.toBeNull();
    expect(migrated!.difficulty).toBe('captain');
    expect(migrated!.shipType).toBe('interceptor');
    expect(migrated!.ownedShips).toEqual(['interceptor']);
    expect(migrated!.credits).toBe(0);
  });

  it('refuses junk and saves from the future rather than loading them', () => {
    expect(migrate(null)).toBeNull();
    expect(migrate('nonsense')).toBeNull();
    expect(migrate({})).toBeNull();
    expect(migrate({ version: SAVE_VERSION + 1, missionIndex: 0 })).toBeNull();
  });
});

describe('difficulty', () => {
  it('scales hostile hull and damage', () => {
    const hullAt = (difficulty: 'ensign' | 'captain' | 'admiral'): number => {
      const w = createWorld({ difficulty });
      // The director only hails at the *active* objective, which is index 0 on a
      // fresh world.
      w.player.pos = { ...w.landmarks[0]!.pos };
      step(w, TICK_DT);
      applyCommand(w, { c: 'acceptObjective' });
      step(w, TICK_DT);
      return w.enemies[0]!.maxHull;
    };

    expect(hullAt('ensign')).toBeLessThan(hullAt('captain'));
    expect(hullAt('admiral')).toBeGreaterThan(hullAt('captain'));
    expect(hullAt('admiral') / hullAt('captain')).toBeCloseTo(DIFFICULTY_SCALE.admiral.hull, 3);
  });
});

describe('defeat', () => {
  it('ends the game when the hull is gone, and stops accepting commands', () => {
    const w = createWorld();
    applyDamage(w.player, w.player.hull + w.player.shield, true, 0, w.events);
    step(w, TICK_DT);

    expect(w.phase).toBe('defeat');
    expect(snapshot(w).phase).toBe('defeat');

    const pos = { ...w.player.pos };
    applyCommand(w, { c: 'throttle', v: 1 });
    run(w, 2);
    expect(w.player.pos).toEqual(pos);
  });
});

describe('skirmish', () => {
  it('spawns waves that grow, and never runs the campaign director', () => {
    const w = createWorld({ mode: 'skirmish' });
    expect(w.skirmishWave).toBe(0);

    step(w, TICK_DT);
    expect(w.skirmishWave).toBe(1);
    const firstWave = w.enemies.length;
    expect(firstWave).toBeGreaterThan(0);

    // Clear it; the next wave arrives after the interval and is at least as big.
    for (const e of w.enemies) applyDamage(e, e.hull + e.shield, true, 0, w.events);
    run(w, WAVE_INTERVAL + 0.2);

    expect(w.skirmishWave).toBe(2);
    expect(w.enemies.length).toBeGreaterThanOrEqual(firstWave);
    // No campaign progression in skirmish.
    expect(w.missionIndex).toBe(0);
    expect(w.phase).toBe('playing');
  });

  it('waits out the interval rather than spawning instantly', () => {
    const w = createWorld({ mode: 'skirmish' });
    step(w, TICK_DT);
    for (const e of w.enemies) applyDamage(e, e.hull + e.shield, true, 0, w.events);

    run(w, WAVE_INTERVAL * 0.5);
    expect(w.skirmishWave).toBe(1);
  });

  it('banks kill rewards without a campaign encounter', () => {
    const w = createWorld({ mode: 'skirmish' });
    step(w, TICK_DT);

    const before = w.player.credits;
    const victim = w.enemies[0]!;
    applyDamage(victim, victim.hull + victim.shield, true, 0, w.events);
    step(w, TICK_DT);

    expect(w.player.credits).toBe(before + ENEMIES[victim.enemyType].rewardCredits);
  });

  it('reports its wave to the crew', () => {
    const w = createWorld({ mode: 'skirmish' });
    step(w, TICK_DT);
    const snap = snapshot(w);
    expect(snap.mode).toBe('skirmish');
    expect(snap.skirmishWave).toBe(1);
  });
});
