// Damage, firing arcs, and projectiles. Ported from Components/HealthComponent.cpp,
// WeaponComponent.cpp and TorpedoLauncherComponent.cpp.

import { DEG, bearingTo, dist } from './math';
import type { Combatant, PlayerShip, SimEvent, World } from './types';
import { MAX_MITIGATION, SHIELD_MITIGATION_SCALE } from './data';

/**
 * Shield power mitigates incoming damage before the shield pool absorbs anything
 * (DECISIONS D11) — more Shields power means softer hits, capped at MAX_MITIGATION.
 */
export const shieldMitigation = (shieldPower: number): number =>
  Math.min(Math.max(shieldPower * SHIELD_MITIGATION_SCALE, 0), MAX_MITIGATION);

/**
 * Applies damage the way the C++ HealthComponent did: mitigation scales the hit, the
 * shield pool absorbs what it can, and only the overflow reaches hull. Torpedoes set
 * `bypassShield` and land their full payload directly on hull.
 *
 * Returns the damage that actually reached hull.
 */
export const applyDamage = (
  target: Combatant,
  amount: number,
  bypassShield: boolean,
  shieldPower: number,
  events: SimEvent[],
): number => {
  // Docked ships are combat-safe: the starbase is meant to be a refuge, and in the
  // C++ build docking set invulnerability explicitly.
  if (amount <= 0 || !target.alive || target.invulnerable) return 0;

  const preHull = target.hull;

  if (bypassShield) {
    target.hull = Math.max(0, target.hull - amount);
  } else {
    const effective = amount * (1 - shieldMitigation(shieldPower));
    const toShield = Math.min(target.shield, effective);
    target.shield -= toShield;
    target.hull = Math.max(0, target.hull - (effective - toShield));
  }

  const hullLoss = preHull - target.hull;
  if (hullLoss > 0) events.push({ t: 'hit', pos: { ...target.pos }, damage: hullLoss });

  if (target.hull <= 0 && target.alive) {
    target.alive = false;
    events.push({ t: 'kill', pos: { ...target.pos }, id: target.id });
  }
  return hullLoss;
};

/** True when `to` sits inside a cone of `arcDeg` centred on the firer's bow. */
export const inArc = (from: Combatant, to: Combatant, arcDeg: number): boolean =>
  Math.abs(bearingTo(from.pos, from.heading, to.pos)) <= arcDeg * 0.5 * DEG;

export const inRange = (from: Combatant, to: Combatant, range: number): boolean =>
  dist(from.pos, to.pos) <= range;

/**
 * Resolves a beam shot: arc and range gated. `armorMultiplier` below 1 means the target
 * is armoured — the caller decides whether that armour still applies (for cruisers it
 * lifts once Science has scanned the weakpoint).
 */
export const fireBeam = (
  world: World,
  shooter: Combatant,
  target: Combatant,
  damage: number,
  arcDeg: number,
  range: number,
  friendly: boolean,
  armorMultiplier = 1,
): boolean => {
  if (!target.alive || !inArc(shooter, target, arcDeg) || !inRange(shooter, target, range)) {
    return false;
  }

  const dealt = damage * armorMultiplier;

  world.events.push({ t: 'beam', from: { ...shooter.pos }, to: { ...target.pos }, friendly });

  // Only the player runs a shield-power allocation; hostiles mitigate at a flat 0.
  const shieldPower = (target as PlayerShip).power?.shields ?? 0;
  applyDamage(target, dealt, false, shieldPower, world.events);
  return true;
};
