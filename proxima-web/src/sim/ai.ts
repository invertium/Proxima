// Hostile behaviour. Ported from Ships/EnemyShip.cpp: close to a standoff ring, hold
// the bow on the player, keep off each other, and fire on an interval once in range.

import { ENEMIES } from './data';
import { DEG, addScaled, bearingTo, clamp, dist, forward } from './math';
import { fireBeam } from './combat';
import type { EnemyShip, World } from './types';

const SEPARATION = 1300;
const BEAM_ARC = 90;

export const stepEnemy = (world: World, e: EnemyShip, dt: number): void => {
  const def = ENEMIES[e.enemyType];
  const player = world.player;

  if (def.passive || !player.alive) return;

  if (e.graceTimer > 0) e.graceTimer -= dt;

  const range = dist(e.pos, player.pos);

  // Turn the bow toward the player, rate-limited.
  const bearing = bearingTo(e.pos, e.heading, player.pos);
  const maxTurn = def.turnRateDeg * DEG * dt;
  e.heading += clamp(bearing, -maxTurn, maxTurn);

  // Hold a standoff ring: close when far, back off when the player crowds us.
  const gap = range - def.standoffDistance;
  const approach = clamp(gap / def.standoffDistance, -1, 1);
  const dir = forward(e.heading);
  addScaled(e.pos, dir, def.moveSpeed * approach * dt);

  // Cheap separation so a fleet doesn't stack into one point.
  for (const other of world.enemies) {
    if (other === e || !other.alive) continue;
    const d = dist(e.pos, other.pos);
    if (d > 0 && d < SEPARATION) {
      const push = (SEPARATION - d) / SEPARATION;
      e.pos.x += ((e.pos.x - other.pos.x) / d) * push * def.moveSpeed * dt;
      e.pos.z += ((e.pos.z - other.pos.z) / d) * push * def.moveSpeed * dt;
    }
  }

  // Fire control.
  e.fireCooldown -= dt;
  if (e.graceTimer <= 0 && e.fireCooldown <= 0 && range <= def.engageRange) {
    const scale = world.difficulty === 'ensign' ? 0.7 : world.difficulty === 'admiral' ? 1.35 : 1;
    if (fireBeam(world, e, player, def.beamDamage * scale, BEAM_ARC, def.engageRange, false)) {
      e.fireCooldown = def.fireInterval;
    }
  }
};
