// Shared simulation vocabulary. This module is the contract between the sim, the
// renderer, and the crew stations — it imports nothing, so all three can depend on it.
//
// UNITS: distances are Unreal centimetres, carried over verbatim from the C++ build so
// every ported constant transfers without rescaling (a ship hull is ~1000 units long,
// beam range 15000, the sector spans ~200000). One sim unit = one Three.js unit.

import type { Rng, Vec3 } from './math';

export type Station = 'helm' | 'weapons' | 'engineering' | 'science';

export type ShipSystem = 'engines' | 'weapons' | 'shields';

/**
 * Systems that take *combat* damage — distinct from ShipSystem, which is the reactor's
 * power rows. A damaged system runs at DAMAGED_MULTIPLIER until Engineering welds it.
 */
export type DamageSystem = 'engine' | 'weapons' | 'sensors';

/** The stat an upgrade path moves. Mirrors EUpgradeStat. */
export type UpgradeStat =
  | 'beamDamage'
  | 'beamRecharge'
  | 'fireArc'
  | 'maxHull'
  | 'maxShield'
  | 'torpedoAmmo'
  | 'reactorBudget'
  | 'strafeSpeed'
  | 'turret';

export interface UpgradeDef {
  id: string;
  name: string;
  unit: string;
  stat: UpgradeStat;
  magnitudePerTier: number;
  maxTier: number;
  /** Tier t+1 costs baseCost*(t+1) and requires rank t+1. */
  baseCost: number;
}

export type GamePhase = 'playing' | 'victory' | 'defeat';

/** Campaign runs the story; skirmish is endless waves for practice. */
export type GameMode = 'campaign' | 'skirmish';

export type Difficulty = 'ensign' | 'captain' | 'admiral';

export type PlayerShipType = 'interceptor' | 'cruiser' | 'corvette' | 'gunboat';

export type EnemyType = 'scout' | 'gunship' | 'cruiser' | 'derelict';

/** Hostile AI states. Overshoot is the strafer's loop-out after a firing pass. */
export type EnemyAIState = 'idle' | 'approach' | 'engage' | 'overshoot';

export type LandmarkKind = 'planet' | 'sun' | 'station';

/** Opportunistic things the sector throws at the crew between objectives. */
export type SectorEvent = 'none' | 'distress' | 'interdiction' | 'salvage';

/** Station board work. One may be active at a time; it persists in the save. */
export type ContractType = 'none' | 'bounty' | 'patrol' | 'delivery';

export interface Contract {
  type: ContractType;
  /** System index the contract points at (and, for patrol, the first leg). */
  targetA: number;
  /** Second patrol leg, or -1. */
  targetB: number;
  /** 0 = outbound leg, 1 = second leg / return. */
  stage: number;
  /** Bounty target's callsign. */
  ship: string;
  reward: number;
}

/** A player hull: visuals + base stats + drydock price. Cost 0 = owned from the start. */
export interface ShipDef {
  type: PlayerShipType;
  name: string;
  blurb: string;
  /** Path under public/assets/ships/. Dropping a new GLB here is the whole import step. */
  model: string;
  /** Uniform scale applied to the GLB so every hull reads at its intended length. */
  scale: number;
  maxSpeed: number;
  acceleration: number;
  /** Degrees per second. */
  turnRate: number;
  maxHull: number;
  beamDamage: number;
  beamRecharge: number;
  torpedoAmmo: number;
  cost: number;
  rankReq: number;
}

export interface EnemyDef {
  type: EnemyType;
  name: string;
  model: string;
  scale: number;
  maxHull: number;
  maxShield: number;
  moveSpeed: number;
  turnRateDeg: number;
  standoffDistance: number;
  engageRange: number;
  fireInterval: number;
  beamDamage: number;
  rewardCredits: number;
  rewardXp: number;
  /**
   * Armoured hulls (the cruiser) take only this fraction of beam damage until Science
   * scans the weakpoint — which is what makes the Science station matter in a fight.
   */
  armoredBeamMultiplier: number;
  /** Derelicts never fight — the tutorial target. */
  passive: boolean;
  /** Interceptors dive past the player and loop out instead of holding a standoff ring. */
  strafeRuns: boolean;
  /** Frigates open a slow torpedo volley the helm can outrun, rather than an instant beam. */
  torpedoVolleys: boolean;
}

export interface Landmark {
  id: string;
  name: string;
  kind: LandmarkKind;
  pos: Vec3;
  radius: number;
  color: number;
  /** Only stations can be docked with. */
  dockable: boolean;
}

export interface CommsBeat {
  sender: string;
  text: string;
  /** Fires this many seconds after the encounter goes live. */
  atSeconds?: number;
  /** Fires after this many kills in the encounter. */
  onKill?: number;
}

export interface MissionDef {
  name: string;
  enemies: EnemyType[];
  briefSender: string;
  briefText: string;
  /** Normalised sector coordinates (0..1), scaled by SECTOR_SPAN into world XZ. */
  mapX: number;
  mapY: number;
  landmarkName: string;
  landmarkKind: LandmarkKind;
  landmarkColor: number;
  landmarkScale: number;
  comms: CommsBeat[];
}

/** Anything with a hull that can be shot. */
export interface Combatant {
  id: number;
  pos: Vec3;
  heading: number;
  hull: number;
  maxHull: number;
  shield: number;
  maxShield: number;
  alive: boolean;
}

export interface PlayerShip extends Combatant {
  kind: 'player';
  shipType: PlayerShipType;
  speed: number;
  strafeSpeed: number;
  /** Reactor allocation per system, 0..MAX_PER_SYSTEM, summing to <= REACTOR_BUDGET. */
  power: Record<ShipSystem, number>;
  beamCharge: number;
  torpedoAmmo: number;
  torpedoReload: number;
  warpCharge: number;
  docked: boolean;
  /** Set while docked at a station, for the drydock UI. */
  dockedAt: string | null;
  targetId: number | null;
  credits: number;
  xp: number;
  /** Purchased tier per upgrade id; absent means tier 0. */
  upgrades: Record<string, number>;
  /** Hulls bought at the drydock and therefore switchable to. */
  ownedShips: PlayerShipType[];
  /** Which combat systems are currently knocked out. */
  damaged: Record<DamageSystem, boolean>;
  /** Welds credited toward the current repair target; 3 completes one. */
  repairWelds: number;
  /** Auto-turret cooldown, only meaningful once the module is bought. */
  turretCooldown: number;
  scanTargetId: number | null;
  scanProgress: number;
  scanning: boolean;
  /** Contacts whose hull/shield numbers have been revealed by a completed scan. */
  scanned: number[];
}

export interface EnemyShip extends Combatant {
  kind: 'enemy';
  enemyType: EnemyType;
  fireCooldown: number;
  /** Hostiles hold fire briefly after spawning so an encounter never opens with a volley. */
  graceTimer: number;
  /** Set once its bounty has been paid, so the payout can't double-fire or be missed. */
  rewarded: boolean;
  aiState: EnemyAIState;
  /** Which side a strafer leads its pass on; flipped each run so passes cross. */
  strafeSide: number;
  volleyRemaining: number;
  volleyTimer: number;
}

export interface Torpedo {
  id: number;
  pos: Vec3;
  heading: number;
  speed: number;
  damage: number;
  life: number;
  friendly: boolean;
}

/** Transient one-frame events the renderer and stations consume (beams, hits, kills). */
export type SimEvent =
  | { t: 'beam'; from: Vec3; to: Vec3; friendly: boolean }
  | { t: 'hit'; pos: Vec3; damage: number }
  | { t: 'kill'; pos: Vec3; id: number }
  | { t: 'comms'; sender: string; text: string }
  | { t: 'dock'; station: string }
  | { t: 'warp'; from: Vec3; to: Vec3 }
  | { t: 'systemDamaged'; system: DamageSystem }
  | { t: 'systemRepaired'; system: DamageSystem }
  | { t: 'scanComplete'; id: number }
  | { t: 'eventStart'; kind: SectorEvent; pos: Vec3 }
  | { t: 'eventEnd'; kind: SectorEvent; success: boolean }
  | { t: 'salvage'; pos: Vec3; credits: number }
  | { t: 'contractComplete'; reward: number };

/** Commands are the only way anything mutates the world — stations send these. */
export type Command =
  | { c: 'throttle'; v: number }
  | { c: 'turn'; v: number }
  | { c: 'strafe'; v: number }
  | { c: 'target'; id: number | null }
  | { c: 'fireBeam' }
  | { c: 'fireTorpedo' }
  | { c: 'power'; system: ShipSystem; v: number }
  | { c: 'dock' }
  | { c: 'warp' }
  | { c: 'buyShip'; type: PlayerShipType }
  | { c: 'buyUpgrade'; id: string }
  /** Engineering's repair sweep: fixes the current damaged system, else restores hull. */
  | { c: 'weld' }
  | { c: 'scan'; id: number | null }
  /** Arriving at a system hails first; the crew commits to the fight with this. */
  | { c: 'acceptObjective' }
  | { c: 'acceptContract' };

export interface World {
  tick: number;
  time: number;
  phase: GamePhase;
  difficulty: Difficulty;
  mode: GameMode;
  /** Skirmish only: waves cleared so far. */
  skirmishWave: number;
  /** Skirmish only: seconds until the next wave arrives. */
  waveTimer: number;
  seed: number;
  /** The world's only randomness. Seeded, so damage rolls and fleet layouts replay. */
  rng: Rng;
  /** Helm intent, held per-world so several sims can run in one process (tests, replays). */
  intent: { throttle: number; turn: number; strafe: number };
  /** Commands land here and are drained at a fixed point in the tick, keeping order deterministic. */
  pending: Command[];
  player: PlayerShip;
  enemies: EnemyShip[];
  torpedoes: Torpedo[];
  landmarks: Landmark[];
  missionIndex: number;
  /** True once the player has reached the objective and been hailed, awaiting ACCEPT. */
  objectiveOffered: boolean;
  /** True once the active mission's fleet has spawned and is still alive. */
  encounterLive: boolean;
  /**
   * Ids belonging to the campaign fleet. Clearing an objective checks these, not every
   * hostile alive — otherwise a stray event raider or bounty target would silently
   * block the campaign from advancing.
   */
  fleetIds: number[];
  activeEvent: SectorEvent;
  /** Where the active event is happening. */
  eventPos: Vec3;
  /** Wall-clock sim time the event expires at. */
  eventDeadline: number;
  /** Ids of hostiles belonging to the event rather than the campaign fleet. */
  eventFleet: number[];
  /** Seconds until the next event roll. */
  eventRollTimer: number;
  /** The board offer while docked, or null. */
  offer: Contract | null;
  /** The signed contract, or null. */
  contract: Contract | null;
  /** Live bounty target's enemy id, when a bounty contract has spawned one. */
  bountyId: number | null;
  /** Tracks docking edges, so the board only refreshes on a fresh arrival. */
  wasDocked: boolean;
  encounterTime: number;
  killsThisEncounter: number;
  firedComms: Set<string>;
  commsLog: { sender: string; text: string; at: number }[];
  events: SimEvent[];
  nextId: number;
}

/** The trimmed, structurally-cloneable view the host broadcasts to crew stations. */
export interface Snapshot {
  tick: number;
  time: number;
  phase: GamePhase;
  mode: GameMode;
  skirmishWave: number;
  player: {
    pos: Vec3;
    heading: number;
    hull: number;
    maxHull: number;
    shield: number;
    maxShield: number;
    speed: number;
    maxSpeed: number;
    power: Record<ShipSystem, number>;
    beamCharge: number;
    torpedoAmmo: number;
    warpCharge: number;
    docked: boolean;
    targetId: number | null;
    credits: number;
    xp: number;
    rank: number;
    shipType: PlayerShipType;
    ownedShips: PlayerShipType[];
    upgrades: Record<string, number>;
    damaged: Record<DamageSystem, boolean>;
    repairWelds: number;
    /** Which system the next welds will fix, or null when everything works. */
    repairTarget: DamageSystem | null;
    hullCritical: boolean;
    scanTargetId: number | null;
    scanProgress: number;
    scanning: boolean;
    /** Effective numbers after upgrades and damage — what the stations should display. */
    stats: {
      maxSpeed: number;
      beamArcDeg: number;
      beamDamage: number;
      reactorBudget: number;
      strafeSpeed: number;
      turretDamage: number;
      scanRange: number;
    };
  };
  contacts: {
    id: number;
    name: string;
    pos: Vec3;
    heading: number;
    hull: number;
    maxHull: number;
    shield: number;
    hostile: boolean;
    inBeamArc: boolean;
    inTorpedoArc: boolean;
    range: number;
    /** Hull/shield numbers are only trustworthy once Science has scanned the contact. */
    scanned: boolean;
  }[];
  landmarks: { id: string; name: string; kind: LandmarkKind; pos: Vec3; radius: number; color: number }[];
  objective: { name: string; pos: Vec3; range: number; offered: boolean; live: boolean } | null;
  /** The live sector event, for the radar marker and the countdown. */
  event: { kind: SectorEvent; pos: Vec3; timeLeft: number } | null;
  /** Board posting while docked — signable with acceptContract. */
  offer: { text: string; reward: number } | null;
  /** The signed contract and how far along it is. */
  contract: { text: string; type: ContractType; stage: number; reward: number } | null;
  comms: { sender: string; text: string; at: number }[];
}
