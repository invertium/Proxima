// M7 — full-campaign replay. The regression net that unit tests can't be.
//
// A scripted crew flies the whole campaign start to victory with no browser and no
// GPU. It catches the class of breakage unit tests miss: a mission that can't be
// reached, an encounter that never clears, a comms beat that never fires, progression
// that stops paying out, or a balance change that makes the campaign unwinnable.

import { describe, expect, it } from 'vitest';
import { applyCommand, createWorld, snapshot, step } from '../src/sim/world';
import { BEAM_RANGE, CAMPAIGN, DOCK_MAX_SPEED, DOCK_RANGE, TICK_DT } from '../src/sim/data';
import { bearingTo, dist } from '../src/sim/math';
import type { Command, World } from '../src/sim/types';

/**
 * A competent-but-simple bridge crew: fly to the objective, accept when hailed, hold
 * the bow on the nearest hostile and fire whatever has a solution, weld what breaks,
 * scan armoured targets, and go home to repair between fights.
 *
 * Deliberately not an optimal player. It does not disengage mid-encounter — an earlier
 * version did, and it made things much worse: fleeing a live fleet just oscillates
 * between the fight and the starbase and never resolves. A human kites; this bot
 * trades. That is why the win assertion below is a threshold rather than "always".
 */
const autopilot = (w: World): Command[] => {
  const cmds: Command[] = [];
  const snap = snapshot(w);
  const p = w.player;

  // Commit to the fight the moment the bridge is hailed.
  if (snap.objective?.offered) cmds.push({ c: 'acceptObjective' });

  // Engineering never idles: a broken system is worse than anything else it could do.
  if (snap.player.repairTarget) cmds.push({ c: 'weld' });

  const hostiles = w.enemies.filter((e) => e.alive);

  if (hostiles.length > 0) {
    const target = hostiles.reduce((a, b) => (dist(p.pos, a.pos) < dist(p.pos, b.pos) ? a : b));
    if (p.targetId !== target.id) cmds.push({ c: 'target', id: target.id });

    const bearing = bearingTo(p.pos, p.heading, target.pos);
    cmds.push({ c: 'turn', v: Math.abs(bearing) < 0.05 ? 0 : Math.sign(bearing) });

    // Hold beam range. Closing further invites rams and hands strafers an easy pass.
    const range = dist(p.pos, target.pos);
    cmds.push({ c: 'throttle', v: range > BEAM_RANGE * 0.7 ? 1 : range < 5000 ? -0.5 : 0 });

    // Science earns its seat here: an unscanned cruiser takes half beam damage, so
    // resolving it is worth more than any amount of extra trigger-pulling.
    if (!p.scanned.includes(target.id) && p.scanTargetId !== target.id) {
      cmds.push({ c: 'scan', id: target.id });
    }

    if (p.beamCharge >= 1) cmds.push({ c: 'fireBeam' });
    if (p.torpedoAmmo > 0 && p.torpedoReload <= 0) cmds.push({ c: 'fireTorpedo' });

    cmds.push({ c: 'power', system: 'engines', v: 0.5 });
    cmds.push({ c: 'power', system: 'shields', v: 2 });
    return cmds;
  }

  // Docking repairs, rearms, and opens the drydock — so the replay covers that whole
  // path end to end, which no unit test reaches.
  const base = w.landmarks.find((l) => l.dockable)!;

  if (p.docked) {
    const hullTier = p.upgrades['hull'] ?? 0;
    const shieldTier = p.upgrades['shields'] ?? 0;
    if (hullTier < 3 && p.credits >= 200 * (hullTier + 1)) cmds.push({ c: 'buyUpgrade', id: 'hull' });
    else if (shieldTier < 3 && p.credits >= 200 * (shieldTier + 1)) cmds.push({ c: 'buyUpgrade', id: 'shields' });
    else cmds.push({ c: 'dock' }); // undock and carry on
    return cmds;
  }

  // Between fights only: pressing on at half hull lets the next fleet finish the job.
  if (p.hull < p.maxHull * 0.5) {
    const range = dist(p.pos, base.pos);
    const bearing = bearingTo(p.pos, p.heading, base.pos);
    cmds.push({ c: 'turn', v: Math.abs(bearing) < 0.05 ? 0 : Math.sign(bearing) });

    if (range <= DOCK_RANGE * 0.8) {
      // Docking refuses above DOCK_MAX_SPEED, so brake before asking.
      cmds.push({ c: 'throttle', v: 0 });
      if (Math.abs(p.speed) <= DOCK_MAX_SPEED) cmds.push({ c: 'dock' });
    } else {
      cmds.push({ c: 'throttle', v: 1 });
      if (p.warpCharge >= 1 && Math.abs(bearing) < 0.2 && range > 25000) cmds.push({ c: 'warp' });
    }
    return cmds;
  }

  // Transit: point at the objective and burn, warping when the charge is up.
  const objective = w.landmarks[w.missionIndex];
  if (!objective) return cmds;

  const bearing = bearingTo(p.pos, p.heading, objective.pos);
  cmds.push({ c: 'turn', v: Math.abs(bearing) < 0.05 ? 0 : Math.sign(bearing) });
  cmds.push({ c: 'throttle', v: 1 });
  cmds.push({ c: 'power', system: 'engines', v: 1.5 });

  // Warping only helps once the bow is roughly right, or it throws the course away.
  if (p.warpCharge >= 1 && Math.abs(bearing) < 0.2 && dist(p.pos, objective.pos) > 25000) {
    cmds.push({ c: 'layInCourse' });
  }
  return cmds;
};

interface ReplayResult {
  world: World;
  simSeconds: number;
  missionsCleared: number;
  commsFired: number;
}

const playCampaign = (opts: { seed: number; maxSeconds?: number }): ReplayResult => {
  const w = createWorld({ seed: opts.seed, difficulty: 'ensign' });
  const maxTicks = Math.round((opts.maxSeconds ?? 1200) / TICK_DT);

  let cleared = 0;
  let lastMission = 0;

  for (let i = 0; i < maxTicks; i++) {
    for (const cmd of autopilot(w)) applyCommand(w, cmd);
    step(w, TICK_DT);

    if (w.missionIndex !== lastMission) {
      cleared += 1;
      lastMission = w.missionIndex;
    }
    if (w.phase !== 'playing') break;
  }

  return { world: w, simSeconds: w.time, missionsCleared: cleared, commsFired: w.firedComms.size };
};

/** A seed the scripted crew reliably wins on, used for the detail assertions. */
const GOOD_SEED = 11;

describe('full campaign replay', () => {
  it('is completable: a scripted crew reaches victory', () => {
    const { world, missionsCleared, simSeconds } = playCampaign({ seed: GOOD_SEED });

    expect(world.phase).toBe('victory');
    expect(world.missionIndex).toBe(CAMPAIGN.length);
    expect(missionsCleared).toBe(CAMPAIGN.length);
    // Sanity: it should take real time, not be won by an accident on tick two.
    expect(simSeconds).toBeGreaterThan(60);
  }, 60000);

  it('pays out progression across the run', () => {
    const { world } = playCampaign({ seed: GOOD_SEED });

    expect(world.player.credits).toBeGreaterThan(0);
    // The four campaign fleets are worth 345 XP in total, so landing near that means
    // every fleet actually paid out rather than one carrying the run.
    expect(world.player.xp).toBeGreaterThanOrEqual(300);
  }, 60000);

  it('fires the scripted comms beats rather than leaving the crew in silence', () => {
    const { world, commsFired } = playCampaign({ seed: GOOD_SEED });

    expect(commsFired).toBeGreaterThan(0);
    expect(world.commsLog.length).toBeGreaterThan(4);
  }, 60000);

  /**
   * A threshold, not "always". The bot trades rather than kiting, so an ambush mission
   * can legitimately kill it — that's a statement about this bot, not about the game.
   * The value is the cliff: a change that actually breaks the campaign drops this to
   * near zero, which is exactly what should fail CI.
   */
  it('wins the large majority of seeds, so victory is not a lucky layout', () => {
    const seeds = [3, 7, 11, 41, 55, 63, 88, 202];
    const wins = seeds.filter((seed) => playCampaign({ seed }).world.phase === 'victory').length;

    expect(wins).toBeGreaterThanOrEqual(Math.ceil(seeds.length * 0.75));
  }, 300000);

  it('replays identically from the same seed', () => {
    const a = playCampaign({ seed: 7 });
    const b = playCampaign({ seed: 7 });

    expect(a.world.tick).toBe(b.world.tick);
    expect(a.world.phase).toBe(b.world.phase);
    expect(snapshot(a.world)).toEqual(snapshot(b.world));
  }, 120000);
});
