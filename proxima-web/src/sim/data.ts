// All tunable content lives here as plain data. Balancing, adding a hull, or adding a
// mission is a table edit — no code path changes, no engine import, no editor.
//
// Every number is ported verbatim from the Unreal build (ShipCatalogue.h,
// EnemyShip.h, WeaponComponent.h, HealthComponent.h, MissionSubsystem.cpp) so the
// browser build plays at the tuning the C++ version shipped with.

import type { Difficulty, EnemyDef, EnemyType, MissionDef, ShipDef, UpgradeDef } from './types';

// ── Global constants (Components/*.h) ───────────────────────────────────────────

export const TICK_HZ = 60;
export const TICK_DT = 1 / TICK_HZ;

export const BEAM_RANGE = 15000;
export const BEAM_ARC_DEG = 70;
export const BEAM_DRAW_TIME = 0.2;

export const TORPEDO_ARC_DEG = 110;
export const TORPEDO_DAMAGE = 60;
export const TORPEDO_SPEED = 5000;
export const TORPEDO_RELOAD = 4;
export const TORPEDO_LIFE = 8;

export const MAX_SHIELD = 50;
export const SHIELD_MITIGATION_SCALE = 0.35;
export const MAX_MITIGATION = 0.8;
export const SHIELD_CHARGE_RATE = 4;

export const REACTOR_BUDGET = 3.0;
export const MAX_PER_SYSTEM = 2.0;

export const WARP_DISTANCE = 18000;
export const WARP_CHARGE_RATE = 0.16;

export const DOCK_MAX_SPEED = 250;
export const DOCK_RANGE = 3500;

export const COLLISION_RADIUS = 650;
export const RAM_DAMAGE = 48;

/** World size the normalised mission mapX/mapY are projected onto. */
export const SECTOR_SPAN = 220000;

/** Proximity that hails the crew at the active objective (M23 open-sector director). */
export const TRIGGER_RADIUS = 18000;

// ── Sector events (Core/MissionSubsystem.h) ─────────────────────────────────────

export const EVENT_ROLL_INTERVAL = 25;
export const EVENT_CHANCE = 0.4;
export const DISTRESS_DURATION = 150;
export const INTERDICTION_DURATION = 180;
export const SALVAGE_DURATION = 120;
export const SALVAGE_COLLECT_RANGE = 1500;
export const SALVAGE_CREDITS = 60;

// ── Contracts (Core/MissionSubsystem.h) ─────────────────────────────────────────

export const CONTRACT_VISIT_RANGE = 9000;
export const PIRATE_CALLSIGNS = ['KRAIT', 'DUSKRUNNER', 'RED HARROW', 'VULTURE', 'IRONJAW'];

// ── Damage control (Components/DamageControlComponent.h) ────────────────────────

/** Chance a hull-reaching hit knocks out one still-working system. */
export const DAMAGE_CHANCE = 0.35;
/** What a knocked-out system runs at until it's welded back. */
export const DAMAGED_MULTIPLIER = 0.5;
export const WELDS_PER_SYSTEM_REPAIR = 3;
/** Hull fraction below which the bridge alarm sounds. */
export const ALARM_HULL_FRACTION = 0.3;
/** Hull restored by a weld once nothing is broken. */
export const WELD_HULL_REPAIR = 4;

// ── Science (Components/ScienceComponent.h) ─────────────────────────────────────

export const SCAN_DURATION = 2.5;
export const SCAN_RANGE = 40000;

// ── Progression (Core/SpaceGameInstance.h) ──────────────────────────────────────

export const XP_PER_RANK = 400;
export const rankFromXp = (xp: number): number => 1 + Math.floor(xp / XP_PER_RANK);

// ── Auto-turret + strafe (Components/WeaponComponent.h, ShipMovementComponent.h) ─

export const TURRET_RANGE = 12000;
export const TURRET_INTERVAL = 1.6;
export const STRAFE_ACCELERATION = 2600;

export const DIFFICULTY_SCALE: Record<Difficulty, { damage: number; hull: number }> = {
  ensign: { damage: 0.7, hull: 0.8 },
  captain: { damage: 1.0, hull: 1.0 },
  admiral: { damage: 1.35, hull: 1.3 },
};

// ── Player hulls (Core/ShipCatalogue.h) ─────────────────────────────────────────
//
// `model` resolves to public/assets/ships/<model>.glb — the TRELLIS output goes in
// unmodified. `scale` is the catalogue scale × 1000 (the GLBs are normalised to ~1
// unit; the Unreal build reached the same size via import_uniform_scale).

export const SHIPS: ShipDef[] = [
  {
    type: 'interceptor',
    name: 'Interceptor',
    blurb: 'Fast, agile, light hull.',
    model: 'interceptor',
    scale: 700,
    maxSpeed: 2100,
    acceleration: 1500,
    turnRate: 75,
    maxHull: 80,
    beamDamage: 20,
    beamRecharge: 0.55,
    torpedoAmmo: 3,
    cost: 0,
    rankReq: 0,
  },
  {
    type: 'cruiser',
    name: 'Cruiser',
    blurb: 'Slow, tough, hits hard.',
    model: 'cruiser',
    scale: 1040,
    maxSpeed: 1300,
    acceleration: 900,
    turnRate: 42,
    maxHull: 160,
    beamDamage: 34,
    beamRecharge: 0.3,
    torpedoAmmo: 6,
    cost: 0,
    rankReq: 0,
  },
  {
    type: 'corvette',
    name: 'Corvette',
    blurb: 'Glass cannon: blistering speed, paper hull.',
    model: 'corvette',
    scale: 590,
    maxSpeed: 2500,
    acceleration: 1800,
    turnRate: 92,
    maxHull: 60,
    beamDamage: 16,
    beamRecharge: 0.75,
    torpedoAmmo: 2,
    cost: 1200,
    rankReq: 2,
  },
  {
    type: 'gunboat',
    name: 'Gunboat',
    blurb: 'Heavy hull and big guns, ponderous turn.',
    model: 'gunboat',
    scale: 1470,
    maxSpeed: 1100,
    acceleration: 800,
    turnRate: 36,
    maxHull: 240,
    beamDamage: 42,
    beamRecharge: 0.26,
    torpedoAmmo: 8,
    cost: 1800,
    rankReq: 3,
  },
];

export const shipDef = (type: ShipDef['type']): ShipDef => SHIPS.find((s) => s.type === type) ?? SHIPS[0]!;

// ── Hostiles (Ships/EnemyShip.h) ────────────────────────────────────────────────

export const ENEMIES: Record<EnemyType, EnemyDef> = {
  // Interceptor: fast, fragile, never stops — dives past and loops back for another run.
  scout: {
    type: 'scout',
    name: 'Pact Scout',
    model: 'corvette',
    scale: 420,
    maxHull: 50,
    maxShield: 20,
    moveSpeed: 1900,
    turnRateDeg: 80,
    standoffDistance: 4500,
    engageRange: 10000,
    fireInterval: 1.6,
    beamDamage: 5,
    rewardCredits: 40,
    rewardXp: 15,
    armoredBeamMultiplier: 1,
    passive: false,
    strafeRuns: true,
    torpedoVolleys: false,
  },
  // Frigate: holds a standoff ring and lobs slow torpedo volleys the helm can outrun.
  gunship: {
    type: 'gunship',
    name: 'Pact Gunship',
    model: 'gunboat',
    scale: 620,
    maxHull: 100,
    maxShield: 50,
    moveSpeed: 1100,
    turnRateDeg: 50,
    standoffDistance: 6000,
    engageRange: 12000,
    fireInterval: 9,
    beamDamage: 8,
    rewardCredits: 80,
    rewardXp: 30,
    armoredBeamMultiplier: 1,
    passive: false,
    strafeRuns: false,
    torpedoVolleys: true,
  },
  // Capital: slow, heavily shielded, and armoured until Science finds the weakpoint.
  cruiser: {
    type: 'cruiser',
    name: 'Pact Cruiser',
    model: 'cruiser',
    scale: 1100,
    maxHull: 220,
    maxShield: 110,
    moveSpeed: 700,
    turnRateDeg: 32,
    standoffDistance: 7500,
    engageRange: 14000,
    fireInterval: 3.2,
    beamDamage: 14,
    rewardCredits: 200,
    rewardXp: 80,
    armoredBeamMultiplier: 0.5,
    passive: false,
    strafeRuns: false,
    torpedoVolleys: false,
  },
  derelict: {
    type: 'derelict',
    name: 'Derelict Raider',
    model: 'corvette',
    scale: 420,
    maxHull: 40,
    maxShield: 0,
    moveSpeed: 0,
    turnRateDeg: 0,
    standoffDistance: 0,
    engageRange: 0,
    fireInterval: 999,
    beamDamage: 0,
    rewardCredits: 40,
    rewardXp: 20,
    armoredBeamMultiplier: 1,
    passive: true,
    strafeRuns: false,
    torpedoVolleys: false,
  },
};

// ── Hostile AI (Ships/EnemyShip.h) ──────────────────────────────────────────────

/** Closer than this and a strafer commits to its pass. */
export const STRAFE_PASS_DISTANCE = 2800;
/** Further than this after a pass and it turns around for another run. */
export const STRAFE_BREAKOFF_DISTANCE = 9000;
/** How far a strafer leads its aim laterally, so a run is a fly-by rather than a ram. */
export const STRAFE_LEAD = 1400;
/** Hostiles slide back out rather than boring through the player's hull. */
export const MIN_SEPARATION = 1300;
export const VOLLEY_SIZE = 3;
export const VOLLEY_GAP = 0.6;
export const ENEMY_TORPEDO_SPEED = 1900;
export const ENEMY_TORPEDO_DAMAGE = 7;
/** Seconds of held fire after spawning, so an encounter never opens with a volley. */
export const SPAWN_GRACE = 12;

// ── Drydock upgrades (Core/UpgradeCatalogue.h) ──────────────────────────────────
//
// Tier t+1 costs baseCost*(t+1) and requires crew rank t+1, so progression is gated
// by both credits and XP. The last two are one-time modules the starter hull lacks.

export const UPGRADES: UpgradeDef[] = [
  { id: 'beam_damage', name: 'Beam Damage', unit: 'dmg', stat: 'beamDamage', magnitudePerTier: 8, maxTier: 3, baseCost: 150 },
  { id: 'beam_recharge', name: 'Beam Recharge', unit: '/s', stat: 'beamRecharge', magnitudePerTier: 0.15, maxTier: 3, baseCost: 150 },
  { id: 'fire_arc', name: 'Targeting Arc', unit: '°', stat: 'fireArc', magnitudePerTier: 15, maxTier: 3, baseCost: 120 },
  { id: 'hull', name: 'Hull Plating', unit: 'hull', stat: 'maxHull', magnitudePerTier: 40, maxTier: 3, baseCost: 200 },
  { id: 'shields', name: 'Shield Capacity', unit: 'shld', stat: 'maxShield', magnitudePerTier: 30, maxTier: 3, baseCost: 200 },
  { id: 'torpedo', name: 'Torpedo Tubes', unit: 'rds', stat: 'torpedoAmmo', magnitudePerTier: 2, maxTier: 3, baseCost: 180 },
  { id: 'reactor', name: 'Reactor Output', unit: 'pwr', stat: 'reactorBudget', magnitudePerTier: 0.5, maxTier: 3, baseCost: 250 },
  { id: 'strafe', name: 'Manoeuvring Thrusters', unit: 'uu/s', stat: 'strafeSpeed', magnitudePerTier: 950, maxTier: 1, baseCost: 160 },
  { id: 'turret', name: 'Auto-Turret', unit: 'dmg', stat: 'turret', magnitudePerTier: 12, maxTier: 1, baseCost: 240 },
];

export const upgradeDef = (id: string): UpgradeDef | undefined => UPGRADES.find((u) => u.id === id);

/** Credit cost to buy the next tier up from `currentTier` (0-based). */
export const upgradeCost = (u: UpgradeDef, currentTier: number): number => u.baseCost * (currentTier + 1);

/** Crew rank needed for the next tier: tier 1 needs rank 1, tier 2 rank 2, and so on. */
export const upgradeRankReq = (currentTier: number): number => currentTier + 1;

// ── Campaign (Core/MissionSubsystem.cpp BuildCampaign) ──────────────────────────

export const CAMPAIGN: MissionDef[] = [
  {
    name: 'Shakedown Cruise',
    enemies: ['derelict'],
    briefSender: 'CMDR VOSS',
    briefText:
      'Welcome to the bridge, Captain. Before real orders, a shakedown — get the crew talking to each other.',
    mapX: 0.16,
    mapY: 0.52,
    landmarkName: 'Haven',
    landmarkKind: 'planet',
    landmarkColor: 0x5999ff,
    landmarkScale: 1.0,
    comms: [
      {
        sender: 'CMDR VOSS',
        text: 'ENGINEERING (console 3): reactor power is a balance — feed one system and the others starve.',
        atSeconds: 6,
      },
      {
        sender: 'CMDR VOSS',
        text: 'See the STARBASE on your scope? Fly back and DOCK to repair, resupply, and visit the drydock.',
        atSeconds: 18,
      },
      {
        sender: 'TACTICAL',
        text: 'Sensors tag a derelict raider — reactor cold, no threat. WEAPONS, lock it and take the shot.',
        atSeconds: 30,
      },
      {
        sender: 'CMDR VOSS',
        text: "Clean kill. You're cleared for active duty, Captain — real orders inbound.",
        onKill: 1,
      },
    ],
  },
  {
    name: 'First Contact',
    enemies: ['scout', 'gunship'],
    briefSender: 'CMDR VOSS',
    briefText:
      'Contacts on the trade lane out of Tarsis. Crimson Pact markings. Intercept and find out what they want.',
    mapX: 0.4,
    mapY: 0.36,
    landmarkName: 'Tarsis',
    landmarkKind: 'planet',
    landmarkColor: 0x4df2d9,
    landmarkScale: 0.9,
    comms: [
      {
        sender: 'TACTICAL',
        text: "Contacts confirmed — those are Crimson Pact markings. They're powering weapons.",
        atSeconds: 2,
      },
      {
        sender: 'CMDR VOSS',
        text: "Scout's down — but it got a transmission off before it died. They know we're here.",
        onKill: 1,
      },
    ],
  },
  {
    name: 'Patrol Ambush',
    enemies: ['gunship', 'gunship', 'cruiser'],
    briefSender: 'CMDR VOSS',
    briefText:
      'The Korrin Belt patrol has gone silent. Sweep the belt and re-establish contact — carefully.',
    mapX: 0.62,
    mapY: 0.6,
    landmarkName: 'Korrin Belt',
    landmarkKind: 'planet',
    landmarkColor: 0xff8c33,
    landmarkScale: 1.1,
    comms: [
      {
        sender: 'TACTICAL',
        text: 'Ambush! Two gunships and a cruiser just powered up around us. All stations, engage.',
        atSeconds: 1.5,
      },
      {
        sender: 'CMDR VOSS',
        text: "Escorts are scrap. That cruiser's shields are layered thick — strip them before you commit torpedoes.",
        onKill: 2,
      },
    ],
  },
  {
    name: "Warlord's Reach",
    enemies: ['scout', 'scout', 'gunship', 'cruiser'],
    briefSender: 'CMDR VOSS',
    briefText:
      "We back-traced the patrol to the Pact's staging point. Their warlord's flagship is here with everything she has left. Break this fleet and the Crimson Pact is finished in the Veil. This is the one that matters.",
    mapX: 0.86,
    mapY: 0.4,
    landmarkName: 'Ember',
    landmarkKind: 'sun',
    landmarkColor: 0xffb333,
    landmarkScale: 1.0,
    comms: [
      {
        sender: 'CMDR VOSS',
        text: "All hands, battle stations. Whatever happens out there — it's been an honour flying with this crew. For the frontier.",
        atSeconds: 1.5,
      },
      {
        sender: 'TACTICAL',
        text: "Screen's down — just the flagship left. She's wounded and she knows it. Finish it, Captain.",
        onKill: 3,
      },
    ],
  },
];
