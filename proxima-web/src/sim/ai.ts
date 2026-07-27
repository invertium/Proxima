// Hostile behaviour, ported from Ships/EnemyShip.cpp.
//
// Three shapes share one tick: interceptors that dive past and loop out (strafe runs),
// frigates that hold a standoff ring and lob torpedo volleys, and capitals that close
// and slug. Yaw-only — combat stays on a plane, as in the C++ build.

import { fireBeam } from './combat';
import {
  DIFFICULTY_SCALE,
  ENEMIES,
  ENEMY_TORPEDO_DAMAGE,
  ENEMY_TORPEDO_SPEED,
  MIN_SEPARATION,
  STRAFE_BREAKOFF_DISTANCE,
  STRAFE_LEAD,
  STRAFE_PASS_DISTANCE,
  TORPEDO_LIFE,
  VOLLEY_GAP,
  VOLLEY_SIZE,
} from './data';
import { addScaled, bearingTo, clamp, DEG, dist, forward, vec } from './math';
import type { EnemyShip, World } from './types';

const BEAM_ARC = 90;

export const stepEnemy = (world: World, e: EnemyShip, dt: number): void => {
  const def = ENEMIES[e.enemyType];
  const player = world.player;

  if (def.passive || !player.alive) {
    e.aiState = 'idle';
    return;
  }

  const range = dist(e.pos, player.pos);
  const inEngageRange = range <= def.engageRange;

  if (e.graceTimer > 0) e.graceTimer = Math.max(0, e.graceTimer - dt);

  // ── Aim ───────────────────────────────────────────────────────────────────────
  // Strafers lead a lateral offset so a run is a fly-by rather than a head-on ram,
  // and hold their heading flat through the overshoot instead of orbiting.
  if (range > 1 && e.aiState !== 'overshoot') {
    let aimX = player.pos.x;
    let aimZ = player.pos.z;

    if (def.strafeRuns) {
      // Lateral is the in-plane perpendicular of the bearing to the player.
      const nx = (player.pos.x - e.pos.x) / range;
      const nz = (player.pos.z - e.pos.z) / range;
      aimX += -nz * e.strafeSide * STRAFE_LEAD;
      aimZ += nx * e.strafeSide * STRAFE_LEAD;
    }

    const bearing = bearingTo(e.pos, e.heading, vec(aimX, 0, aimZ));
    const maxTurn = def.turnRateDeg * DEG * dt;
    e.heading += clamp(bearing, -maxTurn, maxTurn);
  }

  // ── Move ──────────────────────────────────────────────────────────────────────
  if (def.strafeRuns) {
    // Never stop: dive past at full speed, loop out, come back on the other side.
    if (e.aiState === 'overshoot') {
      if (range > STRAFE_BREAKOFF_DISTANCE) {
        e.strafeSide = -e.strafeSide;
        e.aiState = 'approach';
      }
    } else if (range < STRAFE_PASS_DISTANCE) {
      e.aiState = 'overshoot';
    } else {
      e.aiState = inEngageRange ? 'engage' : 'approach';
    }
    addScaled(e.pos, forward(e.heading), def.moveSpeed * dt);
  } else if (range > def.standoffDistance) {
    e.aiState = inEngageRange ? 'engage' : 'approach';
    addScaled(e.pos, forward(e.heading), def.moveSpeed * dt);
  } else {
    e.aiState = 'engage';
  }

  // Never bore through the player: if a move brought this ship inside the separation
  // bubble, slide it straight back out, harder the closer it got.
  if (range > 1 && range < MIN_SEPARATION) {
    const encroach = (MIN_SEPARATION - range) / MIN_SEPARATION;
    const awayX = (e.pos.x - player.pos.x) / range;
    const awayZ = (e.pos.z - player.pos.z) / range;
    e.pos.x += awayX * def.moveSpeed * encroach * dt;
    e.pos.z += awayZ * def.moveSpeed * encroach * dt;
  }

  // Keep a fleet from stacking into one point.
  for (const other of world.enemies) {
    if (other === e || !other.alive) continue;
    const d = dist(e.pos, other.pos);
    if (d > 0 && d < MIN_SEPARATION) {
      const push = (MIN_SEPARATION - d) / MIN_SEPARATION;
      e.pos.x += ((e.pos.x - other.pos.x) / d) * push * def.moveSpeed * dt;
      e.pos.z += ((e.pos.z - other.pos.z) / d) * push * def.moveSpeed * dt;
    }
  }

  // ── Fire ──────────────────────────────────────────────────────────────────────
  // Strafers hold fire through the loop-out; volley ships open a torpedo salvo
  // instead of an instant beam.
  if (inEngageRange && e.graceTimer <= 0 && e.aiState !== 'overshoot') {
    e.fireCooldown -= dt;
    if (e.fireCooldown <= 0) {
      if (def.torpedoVolleys) {
        e.volleyRemaining = VOLLEY_SIZE;
        e.volleyTimer = 0;
      } else {
        const scale = DIFFICULTY_SCALE[world.difficulty].damage;
        fireBeam(world, e, player, def.beamDamage * scale, BEAM_ARC, def.engageRange, false);
      }
      e.fireCooldown = def.fireInterval;
    }
  } else {
    e.fireCooldown = def.fireInterval;
  }

  // A started volley always finishes, even if the player slips out of range mid-salvo.
  if (e.volleyRemaining > 0) {
    e.volleyTimer -= dt;
    if (e.volleyTimer <= 0) {
      e.volleyRemaining -= 1;
      e.volleyTimer = VOLLEY_GAP;
      launchTorpedo(world, e);
    }
  }
};

const launchTorpedo = (world: World, e: EnemyShip): void => {
  const scale = DIFFICULTY_SCALE[world.difficulty].damage;
  world.torpedoes.push({
    id: world.nextId++,
    pos: { ...e.pos },
    heading: bearingTo(e.pos, 0, world.player.pos),
    speed: ENEMY_TORPEDO_SPEED,
    damage: ENEMY_TORPEDO_DAMAGE * scale,
    life: TORPEDO_LIFE,
    friendly: false,
    targetId: world.player.id,
  });
};
