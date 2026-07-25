// Effective player stats: the hull's base numbers, plus purchased upgrade tiers, minus
// whatever combat damage has knocked out.
//
// Everything that needs a number — movement, weapons, shields, the reactor, sensors,
// the HUD, the stations — reads it from here. Keeping the resolution in one place is
// what stops "does this upgrade actually do anything?" from becoming a per-system
// question, and it makes the whole progression system testable without a fight.

import {
  BEAM_ARC_DEG,
  DAMAGED_MULTIPLIER,
  MAX_SHIELD,
  REACTOR_BUDGET,
  SCAN_RANGE,
  UPGRADES,
  shipDef,
} from './data';
import type { DamageSystem, PlayerShip, UpgradeStat } from './types';

export interface EffectiveStats {
  maxSpeed: number;
  acceleration: number;
  turnRate: number;
  strafeSpeed: number;
  maxHull: number;
  maxShield: number;
  beamDamage: number;
  beamRecharge: number;
  beamArcDeg: number;
  torpedoAmmo: number;
  reactorBudget: number;
  turretDamage: number;
  scanRange: number;
  radarRange: number;
}

/** Total magnitude a player has bought along one upgrade path. */
export const upgradeBonus = (player: PlayerShip, stat: UpgradeStat): number => {
  let total = 0;
  for (const u of UPGRADES) {
    if (u.stat !== stat) continue;
    total += (player.upgrades[u.id] ?? 0) * u.magnitudePerTier;
  }
  return total;
};

const damageScale = (player: PlayerShip, system: DamageSystem): number =>
  player.damaged[system] ? DAMAGED_MULTIPLIER : 1;

export const effectiveStats = (player: PlayerShip): EffectiveStats => {
  const base = shipDef(player.shipType);

  // Engine damage halves top speed; weapons damage halves beam recharge; sensors
  // damage halves both radar and scan range.
  const engine = damageScale(player, 'engine');
  const weapons = damageScale(player, 'weapons');
  const sensors = damageScale(player, 'sensors');

  return {
    maxSpeed: base.maxSpeed * engine,
    acceleration: base.acceleration,
    turnRate: base.turnRate,
    // Strafe is a bought module: zero until Manoeuvring Thrusters are installed.
    strafeSpeed: upgradeBonus(player, 'strafeSpeed') * engine,
    maxHull: base.maxHull + upgradeBonus(player, 'maxHull'),
    maxShield: MAX_SHIELD + upgradeBonus(player, 'maxShield'),
    beamDamage: base.beamDamage + upgradeBonus(player, 'beamDamage'),
    beamRecharge: (base.beamRecharge + upgradeBonus(player, 'beamRecharge')) * weapons,
    beamArcDeg: BEAM_ARC_DEG + upgradeBonus(player, 'fireArc'),
    torpedoAmmo: base.torpedoAmmo + upgradeBonus(player, 'torpedoAmmo'),
    reactorBudget: REACTOR_BUDGET + upgradeBonus(player, 'reactorBudget'),
    // Also a bought module — zero means no turret is installed at all.
    turretDamage: upgradeBonus(player, 'turret'),
    scanRange: SCAN_RANGE * sensors,
    radarRange: 30000 * sensors,
  };
};
