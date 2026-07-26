// Campaign persistence. Pure serialisation — no storage API, no DOM — so the same
// code runs in the worker, in a test, and against whatever the host chooses to write
// to (IndexedDB in the browser, a file in a replay harness).
//
// Ported from Core/SpaceSaveGame.h. Only campaign progress is saved: the sector, the
// fleet, and anything in flight are rebuilt on load, exactly as the C++ build did.

import type { Contract, Difficulty, PlayerShipType, World } from './types';

/**
 * Bump when a field changes meaning. `migrate` below decides what to do with older
 * saves — silently loading a stale shape is how a campaign gets quietly corrupted.
 */
export const SAVE_VERSION = 1;

export interface SaveGame {
  version: number;
  missionIndex: number;
  difficulty: Difficulty;
  shipType: PlayerShipType;
  ownedShips: PlayerShipType[];
  credits: number;
  xp: number;
  upgrades: Record<string, number>;
  contract: Contract | null;
  /** Kept so a reloaded campaign lays out its fleets the same way. */
  seed: number;
}

export const toSave = (world: World): SaveGame => ({
  version: SAVE_VERSION,
  missionIndex: world.missionIndex,
  difficulty: world.difficulty,
  shipType: world.player.shipType,
  ownedShips: [...world.player.ownedShips],
  credits: world.player.credits,
  xp: world.player.xp,
  upgrades: { ...world.player.upgrades },
  contract: world.contract ? { ...world.contract } : null,
  seed: world.seed,
});

/**
 * Applies a save onto a freshly created world. The caller builds the world with the
 * saved seed/difficulty/hull first; this restores the progression on top.
 */
export const applySave = (world: World, save: SaveGame): void => {
  world.missionIndex = save.missionIndex;
  world.difficulty = save.difficulty;
  world.contract = save.contract ? { ...save.contract } : null;

  const p = world.player;
  p.shipType = save.shipType;
  p.ownedShips = [...save.ownedShips];
  p.credits = save.credits;
  p.xp = save.xp;
  p.upgrades = { ...save.upgrades };

  // A bounty contract's target is a live ship that no longer exists after a reload.
  // Dropping the id lets the contract re-arm rather than waiting on a ghost forever.
  world.bountyId = null;
};

/**
 * Brings an older save up to the current shape, or returns null if it can't be read.
 * Returning null is a valid outcome: better a fresh campaign than a corrupted one.
 */
export const migrate = (raw: unknown): SaveGame | null => {
  if (typeof raw !== 'object' || raw === null) return null;
  const save = raw as Partial<SaveGame>;

  if (typeof save.version !== 'number' || save.version > SAVE_VERSION) return null;
  if (typeof save.missionIndex !== 'number') return null;

  return {
    version: SAVE_VERSION,
    missionIndex: save.missionIndex,
    difficulty: save.difficulty ?? 'captain',
    shipType: save.shipType ?? 'interceptor',
    ownedShips: save.ownedShips?.length ? save.ownedShips : [save.shipType ?? 'interceptor'],
    credits: save.credits ?? 0,
    xp: save.xp ?? 0,
    upgrades: save.upgrades ?? {},
    contract: save.contract ?? null,
    seed: save.seed ?? 1,
  };
};
