// Shared simulation vocabulary. This module is the contract between the sim, the
// renderer, and the crew stations — it imports nothing, so all three can depend on it.
//
// UNITS: distances are Unreal centimetres, carried over verbatim from the C++ build so
// every ported constant transfers without rescaling (a ship hull is ~1000 units long,
// beam range 15000, the sector spans ~200000). One sim unit = one Three.js unit.

import type { Vec3 } from './math';

export type Station = 'helm' | 'weapons' | 'engineering' | 'science';

export type ShipSystem = 'engines' | 'weapons' | 'shields';

export type GamePhase = 'playing' | 'victory' | 'defeat';

export type Difficulty = 'ensign' | 'captain' | 'admiral';

export type PlayerShipType = 'interceptor' | 'cruiser' | 'corvette' | 'gunboat';

export type EnemyType = 'scout' | 'gunship' | 'cruiser' | 'derelict';

export type LandmarkKind = 'planet' | 'sun' | 'station';

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
  /** Cruisers halve incoming beam damage until their shield pool is stripped. */
  armoredBeamMultiplier: number;
  /** Derelicts never fight — the tutorial target. */
  passive: boolean;
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
}

export interface EnemyShip extends Combatant {
  kind: 'enemy';
  enemyType: EnemyType;
  fireCooldown: number;
  /** Hostiles hold fire briefly after spawning so an encounter never opens with a volley. */
  graceTimer: number;
  /** Set once its bounty has been paid, so the payout can't double-fire or be missed. */
  rewarded: boolean;
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
  | { t: 'warp'; from: Vec3; to: Vec3 };

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
  | { c: 'buyShip'; type: PlayerShipType };

export interface World {
  tick: number;
  time: number;
  phase: GamePhase;
  difficulty: Difficulty;
  seed: number;
  /** Helm intent, held per-world so several sims can run in one process (tests, replays). */
  intent: { throttle: number; turn: number; strafe: number };
  /** Commands land here and are drained at a fixed point in the tick, keeping order deterministic. */
  pending: Command[];
  player: PlayerShip;
  enemies: EnemyShip[];
  torpedoes: Torpedo[];
  landmarks: Landmark[];
  missionIndex: number;
  /** True once the active mission's fleet has spawned and is still alive. */
  encounterLive: boolean;
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
    shipType: PlayerShipType;
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
  }[];
  landmarks: { id: string; name: string; kind: LandmarkKind; pos: Vec3; radius: number; color: number }[];
  objective: { name: string; pos: Vec3; range: number } | null;
  comms: { sender: string; text: string; at: number }[];
}
