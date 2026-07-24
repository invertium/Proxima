// All tunable content lives here as plain data. Balancing, adding a hull, or adding a
// mission is a table edit — no code path changes, no engine import, no editor.
//
// Every number is ported verbatim from the Unreal build (ShipCatalogue.h,
// EnemyShip.h, WeaponComponent.h, HealthComponent.h, MissionSubsystem.cpp) so the
// browser build plays at the tuning the C++ version shipped with.

import type { Difficulty, EnemyDef, EnemyType, MissionDef, ShipDef } from './types';

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

/** Proximity that spawns the active mission's fleet (M23 open-sector director). */
export const TRIGGER_RADIUS = 14000;

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
  scout: {
    type: 'scout',
    name: 'Pact Scout',
    model: 'corvette',
    scale: 420,
    maxHull: 45,
    maxShield: 0,
    moveSpeed: 1400,
    turnRateDeg: 65,
    standoffDistance: 4500,
    engageRange: 12000,
    fireInterval: 2.5,
    beamDamage: 8,
    rewardCredits: 80,
    rewardXp: 30,
    armoredBeamMultiplier: 1,
    passive: false,
  },
  gunship: {
    type: 'gunship',
    name: 'Pact Gunship',
    model: 'gunboat',
    scale: 620,
    maxHull: 110,
    maxShield: 30,
    moveSpeed: 1100,
    turnRateDeg: 50,
    standoffDistance: 6000,
    engageRange: 12000,
    fireInterval: 2.5,
    beamDamage: 12,
    rewardCredits: 140,
    rewardXp: 55,
    armoredBeamMultiplier: 1,
    passive: false,
  },
  cruiser: {
    type: 'cruiser',
    name: 'Pact Cruiser',
    model: 'cruiser',
    scale: 1100,
    maxHull: 260,
    maxShield: 90,
    moveSpeed: 850,
    turnRateDeg: 32,
    standoffDistance: 7500,
    engageRange: 14000,
    fireInterval: 3.2,
    beamDamage: 18,
    rewardCredits: 300,
    rewardXp: 120,
    armoredBeamMultiplier: 0.5,
    passive: false,
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
  },
};

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
