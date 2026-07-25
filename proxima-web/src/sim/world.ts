// The authoritative simulation. Pure state + a fixed-step `step()` — no Three.js, no
// DOM, no timers. That is what lets it run in a Web Worker on the host, replay in a
// test, and be verified headlessly in CI without launching a browser.

import { stepEnemy } from './ai';
import { applyDamage, fireBeam, inArc, inRange } from './combat';
import {
  ALARM_HULL_FRACTION,
  BEAM_RANGE,
  CAMPAIGN,
  COLLISION_RADIUS,
  DAMAGE_CHANCE,
  DIFFICULTY_SCALE,
  DOCK_MAX_SPEED,
  DOCK_RANGE,
  ENEMIES,
  MAX_PER_SYSTEM,
  MAX_SHIELD,
  RAM_DAMAGE,
  SCAN_DURATION,
  SECTOR_SPAN,
  SHIELD_CHARGE_RATE,
  SHIPS,
  STRAFE_ACCELERATION,
  TORPEDO_ARC_DEG,
  TORPEDO_DAMAGE,
  TORPEDO_LIFE,
  TORPEDO_RELOAD,
  TORPEDO_SPEED,
  TRIGGER_RADIUS,
  TURRET_INTERVAL,
  TURRET_RANGE,
  WARP_CHARGE_RATE,
  WARP_DISTANCE,
  WELDS_PER_SYSTEM_REPAIR,
  WELD_HULL_REPAIR,
  rankFromXp,
  shipDef,
  upgradeCost,
  upgradeDef,
  upgradeRankReq,
} from './data';
import { effectiveStats } from './stats';
import { DEG, addScaled, clamp, dist, forward, interpConstantTo, makeRng, vec } from './math';
import type {
  Command,
  DamageSystem,
  Difficulty,
  EnemyShip,
  Landmark,
  PlayerShip,
  PlayerShipType,
  ShipSystem,
  Snapshot,
  World,
} from './types';

/**
 * Reactor allocation scales what a system can actually deliver. Nominal power (1.0)
 * is the ship's rated figure; starving a system halves it, overfeeding it adds ~40%.
 */
const powerScale = (p: number): number => 0.6 + 0.4 * clamp(p, 0, MAX_PER_SYSTEM);

const sectorPos = (mapX: number, mapY: number) =>
  vec((mapX - 0.5) * SECTOR_SPAN, 0, (mapY - 0.5) * SECTOR_SPAN);

export const createWorld = (
  opts: { seed?: number; difficulty?: Difficulty; shipType?: PlayerShipType; missionIndex?: number } = {},
): World => {
  const seed = opts.seed ?? 1;
  const def = shipDef(opts.shipType ?? 'interceptor');
  const home = sectorPos(CAMPAIGN[0]!.mapX, CAMPAIGN[0]!.mapY);

  const landmarks: Landmark[] = CAMPAIGN.map((m, i) => ({
    id: `sys${i}`,
    name: m.landmarkName,
    kind: m.landmarkKind,
    pos: sectorPos(m.mapX, m.mapY),
    radius: (m.landmarkKind === 'sun' ? 9000 : 4200) * m.landmarkScale,
    color: m.landmarkColor,
    dockable: false,
  }));

  // The friendly starbase sits just off the home planet — repair, resupply, drydock.
  landmarks.push({
    id: 'starbase',
    name: 'Haven Starbase',
    kind: 'station',
    pos: vec(home.x + 9000, 0, home.z + 5000),
    radius: 900,
    color: 0x9fd8ff,
    dockable: true,
  });

  const player: PlayerShip = {
    kind: 'player',
    id: 0,
    shipType: def.type,
    pos: vec(home.x + 3000, 0, home.z),
    heading: 0,
    speed: 0,
    strafeSpeed: 0,
    hull: def.maxHull,
    maxHull: def.maxHull,
    shield: MAX_SHIELD,
    maxShield: MAX_SHIELD,
    alive: true,
    power: { engines: 1, weapons: 1, shields: 1 },
    beamCharge: 1,
    torpedoAmmo: def.torpedoAmmo,
    torpedoReload: 0,
    warpCharge: 1,
    docked: false,
    dockedAt: null,
    targetId: null,
    credits: 0,
    xp: 0,
    upgrades: {},
    ownedShips: [def.type],
    damaged: { engine: false, weapons: false, sensors: false },
    repairWelds: 0,
    turretCooldown: 0,
    scanTargetId: null,
    scanProgress: 0,
    scanning: false,
    scanned: [],
  };

  return {
    tick: 0,
    time: 0,
    phase: 'playing',
    difficulty: opts.difficulty ?? 'captain',
    seed,
    rng: makeRng(seed),
    intent: { throttle: 0, turn: 0, strafe: 0 },
    pending: [],
    player,
    enemies: [],
    torpedoes: [],
    landmarks,
    missionIndex: opts.missionIndex ?? 0,
    encounterLive: false,
    encounterTime: 0,
    killsThisEncounter: 0,
    firedComms: new Set(),
    commsLog: [],
    events: [],
    nextId: 1,
  };
};

// ── Commands ────────────────────────────────────────────────────────────────────

/**
 * Queues a command for the next tick. This is the only entry point stations and the
 * pilot use; draining at a fixed point in the tick is what keeps ordering deterministic
 * regardless of when messages actually arrive over the network.
 */
export const queueCommand = (world: World, cmd: Command): void => {
  world.pending.push(cmd);
};

export const applyCommand = (world: World, cmd: Command): void => {
  const p = world.player;
  if (world.phase !== 'playing' || !p.alive) return;

  switch (cmd.c) {
    case 'throttle':
      world.intent.throttle = clamp(cmd.v, -1, 1);
      break;
    case 'turn':
      world.intent.turn = clamp(cmd.v, -1, 1);
      break;
    case 'strafe':
      world.intent.strafe = clamp(cmd.v, -1, 1);
      break;
    case 'target':
      p.targetId = cmd.id;
      break;
    case 'fireBeam':
      tryFireBeam(world);
      break;
    case 'fireTorpedo':
      tryFireTorpedo(world);
      break;
    case 'power': {
      // The reactor is a hard cap on the total (D11): a system can only take what the
      // other two leave unclaimed. The budget itself grows with Reactor Output.
      const others = (Object.keys(p.power) as ShipSystem[])
        .filter((s) => s !== cmd.system)
        .reduce((sum, s) => sum + p.power[s], 0);
      const headroom = Math.max(0, effectiveStats(p).reactorBudget - others);
      p.power[cmd.system] = Math.min(clamp(cmd.v, 0, MAX_PER_SYSTEM), headroom);
      break;
    }
    case 'buyUpgrade':
      tryBuyUpgrade(world, cmd.id);
      break;
    case 'weld':
      weld(world);
      break;
    case 'scan':
      // Retargeting restarts the scan; the crew can't bank progress across contacts.
      p.scanTargetId = cmd.id;
      p.scanProgress = 0;
      p.scanning = cmd.id !== null;
      break;
    case 'dock':
      tryDock(world);
      break;
    case 'warp':
      tryWarp(world);
      break;
    case 'buyShip':
      tryBuyShip(world, cmd.type);
      break;
  }
};

const target = (world: World): EnemyShip | null =>
  world.enemies.find((e) => e.id === world.player.targetId && e.alive) ?? null;

const tryFireBeam = (world: World): void => {
  const p = world.player;
  const t = target(world);
  if (!t || p.beamCharge < 1) return;

  const stats = effectiveStats(p);
  const damage = stats.beamDamage * powerScale(p.power.weapons);
  if (fireBeam(world, p, t, damage, stats.beamArcDeg, BEAM_RANGE, true, ENEMIES[t.enemyType].armoredBeamMultiplier)) {
    p.beamCharge = 0;
  }
};

const tryFireTorpedo = (world: World): void => {
  const p = world.player;
  if (p.torpedoAmmo <= 0 || p.torpedoReload > 0) return;

  const t = target(world);
  if (!t || !inArc(p, t, TORPEDO_ARC_DEG)) return;

  p.torpedoAmmo -= 1;
  p.torpedoReload = TORPEDO_RELOAD;
  world.torpedoes.push({
    id: world.nextId++,
    pos: { ...p.pos },
    heading: p.heading,
    speed: TORPEDO_SPEED,
    damage: TORPEDO_DAMAGE,
    life: TORPEDO_LIFE,
    friendly: true,
  });
};

const tryDock = (world: World): void => {
  const p = world.player;
  if (p.docked) {
    p.docked = false;
    p.dockedAt = null;
    return;
  }
  if (Math.abs(p.speed) > DOCK_MAX_SPEED) return;

  const base = world.landmarks.find((l) => l.dockable && dist(l.pos, p.pos) <= DOCK_RANGE);
  if (!base) return;

  p.docked = true;
  p.dockedAt = base.id;
  p.speed = 0;
  p.hull = p.maxHull;
  p.shield = p.maxShield;
  p.torpedoAmmo = shipDef(p.shipType).torpedoAmmo;
  world.events.push({ t: 'dock', station: base.name });
  pushComms(world, 'STARBASE', 'Docking clamps engaged. Hull repaired, tubes reloaded. Drydock is open, Captain.');
};

const tryWarp = (world: World): void => {
  const p = world.player;
  if (p.warpCharge < 1 || p.docked) return;

  const from = { ...p.pos };
  addScaled(p.pos, forward(p.heading), WARP_DISTANCE);
  p.warpCharge = 0;
  world.events.push({ t: 'warp', from, to: { ...p.pos } });
};

const tryBuyShip = (world: World, type: PlayerShipType): void => {
  const p = world.player;
  if (!p.docked) return;

  const def = SHIPS.find((s) => s.type === type);
  if (!def || def.type === p.shipType) return;

  // Already-owned hulls are a free switch; a new one costs credits and crew rank.
  const owned = p.ownedShips.includes(def.type);
  if (!owned) {
    if (p.credits < def.cost || rankFromXp(p.xp) < def.rankReq) return;
    p.credits -= def.cost;
    p.ownedShips.push(def.type);
  }

  p.shipType = def.type;
  const stats = effectiveStats(p);
  p.maxHull = stats.maxHull;
  p.hull = stats.maxHull;
  p.maxShield = stats.maxShield;
  p.torpedoAmmo = stats.torpedoAmmo;
  pushComms(world, 'DRYDOCK', `${def.name} is yours, Captain. She's fuelled and ready.`);
};

const tryBuyUpgrade = (world: World, id: string): void => {
  const p = world.player;
  if (!p.docked) return;

  const def = upgradeDef(id);
  if (!def) return;

  const tier = p.upgrades[id] ?? 0;
  if (tier >= def.maxTier) return;
  if (p.credits < upgradeCost(def, tier)) return;
  if (rankFromXp(p.xp) < upgradeRankReq(tier)) return;

  p.credits -= upgradeCost(def, tier);
  p.upgrades[id] = tier + 1;

  // Capacity upgrades should be immediately useful, so top the pools up to the new
  // maximum rather than leaving the crew to go and find a repair.
  const stats = effectiveStats(p);
  if (def.stat === 'maxHull') {
    p.hull += def.magnitudePerTier;
    p.maxHull = stats.maxHull;
  }
  if (def.stat === 'maxShield') {
    p.shield += def.magnitudePerTier;
    p.maxShield = stats.maxShield;
  }
  if (def.stat === 'torpedoAmmo') p.torpedoAmmo += def.magnitudePerTier;

  pushComms(world, 'DRYDOCK', `${def.name} installed — tier ${tier + 1}.`);
};

/** The first damaged system in a fixed order, so repairs are predictable for the crew. */
const repairTarget = (p: PlayerShip): DamageSystem | null =>
  (['engine', 'weapons', 'sensors'] as DamageSystem[]).find((s) => p.damaged[s]) ?? null;

/**
 * Engineering's repair sweep. Welds go into fixing broken systems first — three per
 * system — and only once everything works do further welds patch hull.
 */
const weld = (world: World): void => {
  const p = world.player;
  const target = repairTarget(p);

  if (!target) {
    p.hull = Math.min(p.maxHull, p.hull + WELD_HULL_REPAIR);
    return;
  }

  p.repairWelds += 1;
  if (p.repairWelds >= WELDS_PER_SYSTEM_REPAIR) {
    p.repairWelds = 0;
    p.damaged[target] = false;
    world.events.push({ t: 'systemRepaired', system: target });
    pushComms(world, 'ENGINEERING', `${target.toUpperCase()} back online, Captain.`);
  }
};

/**
 * Every hit that actually reaches hull threatens a system. Using hull damage rather
 * than "were shields down?" means an exactly-shield-depleting hit can't break anything,
 * while a shield-bypassing torpedo correctly can.
 */
export const rollSystemDamage = (world: World, hullDamage: number): void => {
  const p = world.player;
  if (hullDamage <= 0 || p.hull <= 0) return;
  if (world.rng() >= DAMAGE_CHANCE) return;

  const working = (['engine', 'weapons', 'sensors'] as DamageSystem[]).filter((s) => !p.damaged[s]);
  if (working.length === 0) return;

  const hit = working[Math.floor(world.rng() * working.length)]!;
  p.damaged[hit] = true;
  world.events.push({ t: 'systemDamaged', system: hit });
  pushComms(world, 'ENGINEERING', `${hit.toUpperCase()} is offline — routing to repair.`);
};

const pushComms = (world: World, sender: string, text: string): void => {
  world.commsLog.push({ sender, text, at: world.time });
  if (world.commsLog.length > 40) world.commsLog.shift();
  world.events.push({ t: 'comms', sender, text });
};

// ── Fixed step ──────────────────────────────────────────────────────────────────

export const step = (world: World, dt: number): void => {
  // Events are drained by the caller after each step, so they are cleared here on entry
  // and everything raised during this tick — including by commands — survives to the end.
  world.events.length = 0;
  if (world.phase !== 'playing') {
    world.pending.length = 0;
    return;
  }

  world.tick += 1;
  world.time += dt;

  for (const cmd of world.pending) applyCommand(world, cmd);
  world.pending.length = 0;

  stepPlayer(world, dt);
  stepDirector(world, dt);

  // Any hull the player loses this tick — from any source — can knock out a system.
  // Measuring the delta here catches beams, torpedoes and rams with one hook.
  const hullBefore = world.player.hull;

  for (const e of world.enemies) {
    if (e.alive) stepEnemy(world, e, dt);
  }

  stepTorpedoes(world, dt);
  stepCollisions(world);
  rollSystemDamage(world, hullBefore - world.player.hull);
  resolveEncounter(world);

  if (world.player.hull <= 0) {
    world.player.alive = false;
    world.phase = 'defeat';
  }
};

const stepPlayer = (world: World, dt: number): void => {
  const p = world.player;
  if (!p.alive) return;

  const stats = effectiveStats(p);

  // Capacity upgrades and system damage both move the ceilings, so keep the pools
  // inside them rather than letting a stale max linger after a hull swap or a repair.
  p.maxHull = stats.maxHull;
  p.maxShield = stats.maxShield;
  p.hull = Math.min(p.hull, p.maxHull);
  p.shield = Math.min(p.shield, p.maxShield);

  if (p.docked) {
    world.intent.throttle = 0;
    p.speed = 0;
  } else {
    // Impulse feel: throttle sets a target speed the ship eases toward at a constant
    // rate, scaled by whatever the reactor is giving Engines.
    const maxSpeed = stats.maxSpeed * powerScale(p.power.engines);
    p.speed = interpConstantTo(p.speed, world.intent.throttle * maxSpeed, dt, stats.acceleration);
    p.heading += world.intent.turn * stats.turnRate * DEG * dt;
    addScaled(p.pos, forward(p.heading), p.speed * dt);

    // Lateral thrust only exists once Manoeuvring Thrusters are bought.
    const targetStrafe = world.intent.strafe * stats.strafeSpeed * powerScale(p.power.engines);
    p.strafeSpeed = interpConstantTo(p.strafeSpeed, targetStrafe, dt, STRAFE_ACCELERATION);
    if (p.strafeSpeed !== 0) {
      addScaled(p.pos, forward(p.heading + Math.PI / 2), p.strafeSpeed * dt);
    }
  }

  // Beam recharge, torpedo reload, warp spool, shield regen.
  p.beamCharge = Math.min(1, p.beamCharge + stats.beamRecharge * powerScale(p.power.weapons) * dt);
  if (p.torpedoReload > 0) p.torpedoReload = Math.max(0, p.torpedoReload - dt);
  p.warpCharge = Math.min(1, p.warpCharge + WARP_CHARGE_RATE * dt);
  if (p.shield < p.maxShield) {
    p.shield = Math.min(p.maxShield, p.shield + SHIELD_CHARGE_RATE * powerScale(p.power.shields) * dt);
  }

  stepTurret(world, stats.turretDamage, dt);
  stepScan(world, stats.scanRange, dt);
};

/** The bought auto-turret: fires on its own interval at anything in range, no arc. */
const stepTurret = (world: World, damage: number, dt: number): void => {
  const p = world.player;
  if (damage <= 0) return;

  p.turretCooldown -= dt;
  if (p.turretCooldown > 0) return;

  const victim = world.enemies
    .filter((e) => e.alive && dist(p.pos, e.pos) <= TURRET_RANGE)
    .sort((a, b) => dist(p.pos, a.pos) - dist(p.pos, b.pos))[0];
  if (!victim) return;

  p.turretCooldown = TURRET_INTERVAL;
  world.events.push({ t: 'beam', from: { ...p.pos }, to: { ...victim.pos }, friendly: true });
  applyDamage(victim, damage, false, 0, world.events);
};

/** Science: hold a lock inside scan range for SCAN_DURATION to reveal a contact. */
const stepScan = (world: World, range: number, dt: number): void => {
  const p = world.player;
  if (!p.scanning || p.scanTargetId === null) return;

  const target = world.enemies.find((e) => e.id === p.scanTargetId && e.alive);
  if (!target || dist(p.pos, target.pos) > range) {
    // Losing the contact loses the progress — the scan has to be held.
    p.scanProgress = 0;
    return;
  }

  p.scanProgress += dt / SCAN_DURATION;
  if (p.scanProgress < 1) return;

  p.scanProgress = 1;
  p.scanning = false;
  if (!p.scanned.includes(target.id)) p.scanned.push(target.id);
  world.events.push({ t: 'scanComplete', id: target.id });
  pushComms(world, 'SCIENCE', `Scan complete: ${ENEMIES[target.enemyType].name}.`);
};

/**
 * The open-sector director (M23): no level reloads. When the player enters the active
 * mission's zone, that fleet spawns around its landmark and the comms beats start.
 */
const stepDirector = (world: World, dt: number): void => {
  const mission = CAMPAIGN[world.missionIndex];
  if (!mission) return;

  const landmark = world.landmarks[world.missionIndex];
  if (!landmark) return;

  if (!world.encounterLive) {
    if (dist(world.player.pos, landmark.pos) <= TRIGGER_RADIUS + landmark.radius) {
      spawnFleet(world, landmark.pos);
      world.encounterLive = true;
      world.encounterTime = 0;
      world.killsThisEncounter = 0;
      pushComms(world, mission.briefSender, mission.briefText);
    }
    return;
  }

  world.encounterTime += dt;

  // Timed and kill-gated comms beats, each fired once.
  for (const [i, beat] of mission.comms.entries()) {
    const key = `${world.missionIndex}:${i}`;
    if (world.firedComms.has(key)) continue;

    const timeHit = beat.atSeconds !== undefined && world.encounterTime >= beat.atSeconds;
    const killHit = beat.onKill !== undefined && world.killsThisEncounter >= beat.onKill;
    if (timeHit || killHit) {
      world.firedComms.add(key);
      pushComms(world, beat.sender, beat.text);
    }
  }
};

const spawnFleet = (world: World, around: { x: number; y: number; z: number }): void => {
  const mission = CAMPAIGN[world.missionIndex]!;
  const rng = makeRng(world.seed + world.missionIndex * 977);
  const scale = DIFFICULTY_SCALE[world.difficulty];

  mission.enemies.forEach((type, i) => {
    const def = ENEMIES[type];
    const angle = (i / mission.enemies.length) * Math.PI * 2 + rng() * 0.6;
    const radius = 7000 + rng() * 3000;
    const hull = def.maxHull * scale.hull;

    world.enemies.push({
      kind: 'enemy',
      id: world.nextId++,
      enemyType: type,
      pos: vec(around.x + Math.cos(angle) * radius, 0, around.z + Math.sin(angle) * radius),
      heading: angle + Math.PI,
      hull,
      maxHull: hull,
      shield: def.maxShield,
      maxShield: def.maxShield,
      alive: true,
      fireCooldown: def.fireInterval,
      graceTimer: 12,
      rewarded: false,
    });
  });
};

const stepTorpedoes = (world: World, dt: number): void => {
  for (const t of world.torpedoes) {
    t.life -= dt;
    addScaled(t.pos, forward(t.heading), t.speed * dt);

    const targets: (EnemyShip | PlayerShip)[] = t.friendly ? world.enemies : [world.player];
    for (const c of targets) {
      if (!c.alive || dist(t.pos, c.pos) > COLLISION_RADIUS) continue;
      // Torpedoes bypass shields entirely — the full payload lands on hull (M17).
      applyDamage(c, t.damage, true, 0, world.events);
      t.life = 0;
      break;
    }
  }
  world.torpedoes = world.torpedoes.filter((t) => t.life > 0);
};

/** Ramming (M22): both hulls take it, so collisions are a mistake, not a tactic. */
const stepCollisions = (world: World): void => {
  const p = world.player;
  if (!p.alive || p.docked) return;

  for (const e of world.enemies) {
    if (!e.alive || dist(p.pos, e.pos) > COLLISION_RADIUS * 2) continue;
    applyDamage(p, RAM_DAMAGE, false, p.power.shields, world.events);
    applyDamage(e, RAM_DAMAGE, false, 0, world.events);
  }
};

const resolveEncounter = (world: World): void => {
  if (!world.encounterLive) return;

  // Bank the bounty for anything that has died and not yet paid out. Keyed off the
  // hostile's own state rather than this tick's event list, so a kill is never missed
  // because of when in the tick the damage landed.
  for (const e of world.enemies) {
    if (e.alive || e.rewarded) continue;
    e.rewarded = true;
    const def = ENEMIES[e.enemyType];
    world.player.credits += def.rewardCredits;
    world.player.xp += def.rewardXp;
    world.killsThisEncounter += 1;
  }

  if (world.enemies.some((e) => e.alive)) return;

  // Fleet wiped: advance seamlessly, no reload, no outcome screen — unless this was
  // the last system, which is the campaign's only victory beat.
  world.enemies.length = 0;
  world.encounterLive = false;
  world.missionIndex += 1;

  if (world.missionIndex >= CAMPAIGN.length) {
    world.phase = 'victory';
    pushComms(world, 'CMDR VOSS', 'The Veil is secure. Well flown, Captain.');
  } else {
    const next = CAMPAIGN[world.missionIndex]!;
    pushComms(world, 'CMDR VOSS', `Sector cleared. Next objective: ${next.landmarkName}. Lay in a course.`);
  }
};

// ── Snapshot ────────────────────────────────────────────────────────────────────

/**
 * The trimmed view broadcast to crew stations. Structured-cloneable (no Set, no class
 * instances) so it crosses the worker boundary and the WebRTC/WebSocket transport
 * without a serialisation pass.
 */
export const snapshot = (world: World): Snapshot => {
  const p = world.player;
  const stats = effectiveStats(p);
  const objectiveLandmark = world.landmarks[world.missionIndex] ?? null;

  return {
    tick: world.tick,
    time: world.time,
    phase: world.phase,
    player: {
      pos: { ...p.pos },
      heading: p.heading,
      hull: p.hull,
      maxHull: p.maxHull,
      shield: p.shield,
      maxShield: p.maxShield,
      speed: p.speed,
      maxSpeed: stats.maxSpeed,
      power: { ...p.power },
      beamCharge: p.beamCharge,
      torpedoAmmo: p.torpedoAmmo,
      warpCharge: p.warpCharge,
      docked: p.docked,
      targetId: p.targetId,
      credits: p.credits,
      xp: p.xp,
      rank: rankFromXp(p.xp),
      shipType: p.shipType,
      ownedShips: [...p.ownedShips],
      upgrades: { ...p.upgrades },
      damaged: { ...p.damaged },
      repairWelds: p.repairWelds,
      repairTarget: repairTarget(p),
      hullCritical: p.hull / p.maxHull < ALARM_HULL_FRACTION,
      scanTargetId: p.scanTargetId,
      scanProgress: p.scanProgress,
      scanning: p.scanning,
      stats: {
        maxSpeed: stats.maxSpeed,
        beamArcDeg: stats.beamArcDeg,
        beamDamage: stats.beamDamage,
        reactorBudget: stats.reactorBudget,
        strafeSpeed: stats.strafeSpeed,
        turretDamage: stats.turretDamage,
        scanRange: stats.scanRange,
      },
    },
    contacts: world.enemies
      .filter((e) => e.alive)
      .map((e) => ({
        id: e.id,
        name: ENEMIES[e.enemyType].name,
        pos: { ...e.pos },
        heading: e.heading,
        hull: e.hull,
        maxHull: e.maxHull,
        shield: e.shield,
        hostile: !ENEMIES[e.enemyType].passive,
        inBeamArc: inArc(p, e, stats.beamArcDeg) && inRange(p, e, BEAM_RANGE),
        inTorpedoArc: inArc(p, e, TORPEDO_ARC_DEG),
        range: dist(p.pos, e.pos),
        scanned: p.scanned.includes(e.id),
      })),
    landmarks: world.landmarks.map((l) => ({
      id: l.id,
      name: l.name,
      kind: l.kind,
      pos: { ...l.pos },
      radius: l.radius,
      color: l.color,
    })),
    objective: objectiveLandmark
      ? { name: objectiveLandmark.name, pos: { ...objectiveLandmark.pos }, range: dist(p.pos, objectiveLandmark.pos) }
      : null,
    comms: world.commsLog.slice(-12),
  };
};
