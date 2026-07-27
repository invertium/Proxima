// Sector life: opportunistic events, station-board contracts, gravity wells, salvage.
//
// These are what make the space between objectives worth flying through. Ported from
// MissionSubsystem.cpp (RollEvent/StartEvent/CheckEvent, GenerateOffer/CheckContract)
// and World/GravityField.cpp.

import {
  CALLSIGN_POOL,
  CAMPAIGN,
  CONTRACT_VISIT_RANGE,
  DISTRESS_CREDITS,
  DISTRESS_DURATION,
  ENEMIES,
  EVENT_CHANCE,
  EVENT_ROLL_INTERVAL,
  INTERDICTION_CREDITS,
  INTERDICTION_DURATION,
  PIRATE_CALLSIGNS,
  SALVAGE_COLLECT_RANGE,
  SALVAGE_CREDITS,
  SALVAGE_DURATION,
  SPAWN_GRACE,
} from './data';
import type { Vec3 } from './math';
import { addScaled, dist, forward, vec } from './math';
import type {
  Contract,
  ContractType,
  EnemyShip,
  EnemyType,
  SectorEvent,
  Verdict,
  World,
} from './types';
import { no, OK } from './types';

// ── Gravity ─────────────────────────────────────────────────────────────────────

/**
 * A soft drift toward nearby bodies. The profile is a band: zero at the surface (so it
 * never pins a ship against something it's already resting on) and zero at the edge of
 * influence, peaking through the middle of the well. Always weaker than thrust, so it
 * bends a course without ever trapping anything.
 */
export const gravityPullAt = (world: World, at: Vec3): Vec3 => {
  const pull = vec();

  for (const body of world.landmarks) {
    if (body.kind === 'station') continue;

    const surface = body.radius;
    const influence = body.radius * 6;
    const dx = body.pos.x - at.x;
    const dz = body.pos.z - at.z;
    const d = Math.hypot(dx, dz);
    if (d >= influence || d <= 1) continue;

    const t = Math.min(Math.max((d - surface) / Math.max(1, influence - surface), 0), 1);
    const falloff = 4 * t * (1 - t);
    const peak = body.kind === 'sun' ? 260 : 140;

    pull.x += (dx / d) * peak * falloff;
    pull.z += (dz / d) * peak * falloff;
  }
  return pull;
};

/** Mirrors world.ts nextCallsign; kept here so sector spawns don't reach into it. */
const callsign = (world: World, type: string): string => {
  const n = (world.typeOrdinals[type] ?? 0) + 1;
  world.typeOrdinals[type] = n;
  return `${CALLSIGN_POOL[type] ?? 'CONTACT'}-${n}`;
};

// ── Events ──────────────────────────────────────────────────────────────────────

const spawnEventShips = (world: World, at: Vec3, types: EnemyType[]): void => {
  types.forEach((type, i) => {
    const def = ENEMIES[type];
    const angle = (i / types.length) * Math.PI * 2;
    const e: EnemyShip = {
      kind: 'enemy',
      id: world.nextId++,
      enemyType: type,
      pos: vec(at.x + Math.cos(angle) * 2200, 0, at.z + Math.sin(angle) * 2200),
      heading: angle + Math.PI,
      hull: def.maxHull,
      maxHull: def.maxHull,
      shield: def.maxShield,
      maxShield: def.maxShield,
      alive: true,
      fireCooldown: def.fireInterval,
      graceTimer: SPAWN_GRACE * 0.25,
      rewarded: false,
      callsign: callsign(world, type),
      aiState: 'approach',
      strafeSide: i % 2 === 0 ? 1 : -1,
      volleyRemaining: 0,
      volleyTimer: 0,
    };
    world.enemies.push(e);
    world.eventFleet.push(e.id);
  });
};

const startEvent = (
  world: World,
  kind: SectorEvent,
  comms: (s: string, t: string) => void,
): void => {
  const p = world.player;
  world.eventFleet = [];

  if (kind === 'distress') {
    // A convoy under attack near the closest *other* system — a timed detour.
    let best = -1;
    let bestDist = Infinity;
    world.landmarks.forEach((l, i) => {
      if (i === world.missionIndex || i >= CAMPAIGN.length) return;
      const d = dist(l.pos, p.pos);
      if (d < bestDist) {
        bestDist = d;
        best = i;
      }
    });
    if (best < 0) return;

    world.eventPos = vec(world.landmarks[best]!.pos.x, 0, world.landmarks[best]!.pos.z + 6000);
    spawnEventShips(world, world.eventPos, ['scout', 'scout']);
    world.eventDeadline = world.time + DISTRESS_DURATION;
    comms(
      'DISTRESS',
      `MAYDAY — supply convoy under raider attack near ${CAMPAIGN[best]!.landmarkName}! Anyone in range, please respond!`,
    );
  } else if (kind === 'interdiction') {
    // A two-ship ambush powering up dead ahead on the ship's course.
    world.eventPos = { ...p.pos };
    addScaled(world.eventPos, forward(p.heading), 11000);
    spawnEventShips(world, world.eventPos, ['scout', 'gunship']);
    world.eventDeadline = world.time + INTERDICTION_DURATION;
    comms(
      'TACTICAL',
      'Pirate interdiction! Two contacts powering up dead ahead on our course — they want our cargo, Captain.',
    );
  } else if (kind === 'salvage') {
    const angle = world.rng() * Math.PI * 2;
    world.eventPos = vec(p.pos.x + Math.cos(angle) * 9000, 0, p.pos.z + Math.sin(angle) * 9000);
    world.eventDeadline = world.time + SALVAGE_DURATION;
    comms(
      'SCIENCE',
      'Sensor ghost resolved: a free-floating cargo pod adrift close by. Close to 1,500 uu and we can tractor it in.',
    );
  } else {
    return;
  }

  world.activeEvent = kind;
  world.events.push({ t: 'eventStart', kind, pos: { ...world.eventPos } });
};

const endEvent = (world: World, success: boolean, comms: (s: string, t: string) => void): void => {
  const kind = world.activeEvent;
  if (kind === 'none') return;

  // Event hostiles leave with the event — they aren't the campaign fleet, and leaving
  // them behind would silently block the next objective's clear check.
  world.enemies = world.enemies.filter((e) => !world.eventFleet.includes(e.id));
  world.eventFleet = [];
  world.activeEvent = 'none';
  world.events.push({ t: 'eventEnd', kind, success });

  if (!success) {
    comms('OPS', kind === 'distress' ? 'The convoy went dark. We were too slow.' : 'Contact lost.');
  }
};

export const stepEvents = (
  world: World,
  dt: number,
  comms: (s: string, t: string) => void,
): void => {
  const p = world.player;

  if (world.activeEvent === 'none') {
    // Events never interrupt a live encounter or a finished campaign.
    if (world.encounterLive || world.phase !== 'playing' || !p.alive) return;

    world.eventRollTimer -= dt;
    if (world.eventRollTimer > 0) return;

    world.eventRollTimer = EVENT_ROLL_INTERVAL;
    if (world.rng() >= EVENT_CHANCE) return;

    const kinds: SectorEvent[] = ['distress', 'interdiction', 'salvage'];
    startEvent(world, kinds[Math.floor(world.rng() * kinds.length)]!, comms);
    return;
  }

  // Salvage resolves on proximity rather than on combat.
  if (world.activeEvent === 'salvage') {
    if (dist(p.pos, world.eventPos) <= SALVAGE_COLLECT_RANGE) {
      p.credits += SALVAGE_CREDITS;
      world.events.push({ t: 'salvage', pos: { ...world.eventPos }, credits: SALVAGE_CREDITS });
      comms('ENGINEERING', `Pod aboard — ${SALVAGE_CREDITS} credits of salvage, Captain.`);
      endEvent(world, true, comms);
      return;
    }
  } else {
    // Combat events resolve when their fleet is wiped.
    const alive = world.enemies.filter((e) => world.eventFleet.includes(e.id) && e.alive);
    if (alive.length === 0) {
      // Clearing a timed event pays on top of the kills — that bonus is the reason to
      // take the detour at all.
      const bonus = world.activeEvent === 'distress' ? DISTRESS_CREDITS : INTERDICTION_CREDITS;
      world.player.credits += bonus;
      comms(
        'OPS',
        world.activeEvent === 'distress'
          ? `Convoy is safe. ${bonus} credits from a grateful skipper.`
          : `Interdiction broken. ${bonus} credits salvaged.`,
      );
      endEvent(world, true, comms);
      return;
    }
  }

  if (world.time >= world.eventDeadline) endEvent(world, false, comms);
};

// ── Contracts ───────────────────────────────────────────────────────────────────

export const describeContract = (c: Contract): string => {
  const a = CAMPAIGN[c.targetA]?.landmarkName ?? '?';
  const b = CAMPAIGN[c.targetB]?.landmarkName ?? '?';
  switch (c.type) {
    case 'bounty':
      return `BOUNTY: destroy the raider ${c.ship} loitering at ${a} — ${c.reward} cr`;
    case 'patrol':
      return `PATROL: sweep ${a}, then ${b} — ${c.reward} cr`;
    case 'delivery':
      return `DELIVERY: run cargo to ${a}, then dock back here — ${c.reward} cr`;
    default:
      return 'No contract';
  }
};

/** Rolls a fresh board offer. Targets exclude home (index 0), which hosts the starbase. */
const generateOffer = (world: World): Contract => {
  const count = CAMPAIGN.length;
  const randomSystem = (exclude: number): number => {
    let pick = exclude;
    for (let guard = 0; guard < 16 && pick === exclude; guard++) {
      pick = 1 + Math.floor(world.rng() * (count - 1));
    }
    return pick;
  };

  const types: ContractType[] = ['bounty', 'patrol', 'delivery'];
  const type = types[Math.floor(world.rng() * types.length)]!;
  const targetA = randomSystem(-1);

  return {
    type,
    targetA,
    targetB: type === 'patrol' ? randomSystem(targetA) : -1,
    stage: 0,
    ship:
      type === 'bounty' ? PIRATE_CALLSIGNS[Math.floor(world.rng() * PIRATE_CALLSIGNS.length)]! : '',
    reward: type === 'bounty' ? 220 : type === 'patrol' ? 140 : 160,
  };
};

const spawnBountyShip = (world: World, c: Contract): void => {
  const at = world.landmarks[c.targetA];
  if (!at) return;

  const def = ENEMIES.gunship;
  const e: EnemyShip = {
    kind: 'enemy',
    id: world.nextId++,
    enemyType: 'gunship',
    pos: vec(at.pos.x + 5000, 0, at.pos.z + 3000),
    heading: 0,
    hull: def.maxHull,
    maxHull: def.maxHull,
    shield: def.maxShield,
    maxShield: def.maxShield,
    alive: true,
    fireCooldown: def.fireInterval,
    graceTimer: SPAWN_GRACE * 0.25,
    // A bounty target flies under the name on the contract, not a fleet callsign.
    callsign: c.ship || callsign(world, 'gunship'),
    rewarded: false,
    aiState: 'approach',
    strafeSide: 1,
    volleyRemaining: 0,
    volleyTimer: 0,
  };
  world.enemies.push(e);
  world.bountyId = e.id;
};

export const acceptContract = (world: World, comms: (s: string, t: string) => void): Verdict => {
  if (!world.player.docked) return no('the board is only signable at a starbase');
  if (world.contract) return no('a contract is already running');
  if (!world.offer) return no('nothing posted on the board');

  world.contract = { ...world.offer };
  world.offer = null;
  comms('STARBASE OPS', `Contract signed. ${describeContract(world.contract)}`);

  if (world.contract.type === 'bounty') spawnBountyShip(world, world.contract);
  return OK;
};

const completeContract = (world: World, comms: (s: string, t: string) => void): void => {
  const c = world.contract;
  if (!c) return;

  world.player.credits += c.reward;
  world.events.push({ t: 'contractComplete', reward: c.reward });
  comms('STARBASE OPS', `Contract closed — ${c.reward} credits transferred. Good flying, Captain.`);

  world.contract = null;
  world.bountyId = null;
};

export const stepContracts = (world: World, comms: (s: string, t: string) => void): void => {
  const p = world.player;
  if (!p.alive) return; // a wreck runs no errands; the contract survives for the retry

  // The board refreshes on each fresh docking, and only while nothing is signed.
  if (p.docked && !world.wasDocked && !world.contract) {
    world.offer = generateOffer(world);
    comms('STARBASE OPS', `Board posting: ${describeContract(world.offer)}`);
  }
  if (!p.docked) world.offer = null; // offers are only signable at the board
  world.wasDocked = p.docked;

  const c = world.contract;
  if (!c) return;

  const atSystem = (index: number): boolean => {
    const l = world.landmarks[index];
    return !!l && dist(p.pos, l.pos) <= CONTRACT_VISIT_RANGE;
  };

  if (c.type === 'patrol') {
    if (c.stage === 0 && atSystem(c.targetA)) {
      c.stage = 1;
      comms(
        'STARBASE OPS',
        `First waypoint swept — ${CAMPAIGN[c.targetA]?.landmarkName} reads clear. One leg to go: ${CAMPAIGN[c.targetB]?.landmarkName}.`,
      );
    } else if (c.stage === 1 && atSystem(c.targetB)) {
      completeContract(world, comms);
    }
  } else if (c.type === 'delivery') {
    if (c.stage === 0 && atSystem(c.targetA)) {
      c.stage = 1;
      comms(
        'STARBASE OPS',
        `Cargo delivered to ${CAMPAIGN[c.targetA]?.landmarkName}. Return to the starbase and dock to close the contract.`,
      );
    } else if (c.stage === 1 && p.docked) {
      completeContract(world, comms);
    }
  } else if (c.type === 'bounty') {
    const bounty = world.enemies.find((e) => e.id === world.bountyId);
    if (bounty && !bounty.alive) completeContract(world, comms);
  }
};
