import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { cn } from '@/lib/utils';
import { SHIPS, UPGRADES, upgradeCost, upgradeRankReq } from '@/sim/data';
import type { Snapshot } from '@/sim/types';
import { km, pct } from '@/stations/lib/utils';

import type { StationPanelProps } from '../StationPanelProps';

interface EngineeringDrydockProps {
  readonly player: Snapshot['player'];
  readonly send: StationPanelProps['send'];
}

export function EngineeringDrydock({ player, send }: EngineeringDrydockProps) {
  return (
    <div className="rounded-lg border border-[#1e3a5f] bg-[#06101c] p-3">
      <div className="mb-3 flex items-center justify-between gap-2 text-[11px] tracking-[0.18em] text-[#7dd3fc]">
        <span>DRYDOCK</span>
        <span>
          {player.credits} CR · RANK {player.rank}
        </span>
      </div>
      {!player.docked ? (
        <p className="text-xs leading-5 text-[#9ab6da]">Dock at a starbase to open the drydock.</p>
      ) : (
        <div className="space-y-4">
          <div>
            <div className="mb-2 text-[11px] tracking-[0.16em] text-[#8da6c8]">UPGRADES</div>
            <div className="space-y-2">
              {UPGRADES.map((upgrade) => {
                const tier = player.upgrades[upgrade.id] ?? 0;
                const maxed = tier >= upgrade.maxTier;
                const cost = upgradeCost(upgrade, tier);
                const requiredRank = upgradeRankReq(tier);
                const canBuy = !maxed && player.credits >= cost && player.rank >= requiredRank;

                return (
                  <div key={upgrade.id} className="rounded-lg border border-[#1e293b] bg-[#08101c] p-3">
                    <div className="flex items-start justify-between gap-3">
                      <div>
                        <div className="text-sm font-semibold text-[#dbe7ff]">{upgrade.name}</div>
                        <div className="mt-1 text-xs text-[#8da6c8]">
                          {maxed ? 'MAX' : `${cost} cr · rank ${requiredRank} · +${upgrade.magnitudePerTier}${upgrade.unit}`}
                        </div>
                      </div>
                      <Badge variant="outline" className="border-[#33537d] bg-[#102033] text-[#c9def8]">
                        TIER {tier}/{upgrade.maxTier}
                      </Badge>
                    </div>
                    <div className="mt-3 flex justify-end">
                      <Button type="button" size="sm" disabled={!canBuy} onClick={() => send({ c: 'buyUpgrade', id: upgrade.id })}>
                        BUY
                      </Button>
                    </div>
                  </div>
                );
              })}
            </div>
          </div>

          <div>
            <div className="mb-2 text-[11px] tracking-[0.16em] text-[#8da6c8]">HULLS</div>
            <div className="space-y-2">
              {SHIPS.map((ship) => {
                const owned = player.ownedShips.includes(ship.type);
                const active = ship.type === player.shipType;
                const canBuy = owned || (player.credits >= ship.cost && player.rank >= ship.rankReq);
                const statusText = active
                  ? 'Current hull'
                  : owned
                    ? 'Owned — ready to swap'
                    : `${ship.cost} cr · rank ${ship.rankReq}`;

                return (
                  <div key={ship.type} className="rounded-lg border border-[#1e293b] bg-[#08101c] p-3">
                    <div className="flex items-start justify-between gap-3">
                      <div>
                        <div className="text-sm font-semibold text-[#dbe7ff]">{ship.name}</div>
                        <p className="mt-1 text-xs leading-5 text-[#8da6c8]">{ship.blurb}</p>
                      </div>
                      <Badge
                        variant="outline"
                        className={cn(
                          active
                            ? 'border-emerald-500/45 bg-emerald-950/35 text-emerald-200'
                            : owned
                              ? 'border-[#33537d] bg-[#102033] text-[#c9def8]'
                              : 'border-[#5c4420] bg-[#1e1407] text-[#fcd34d]',
                        )}
                      >
                        {active ? 'ACTIVE' : owned ? 'OWNED' : 'FOR SALE'}
                      </Badge>
                    </div>
                    <div className="mt-2 grid grid-cols-3 gap-2 text-[11px] text-[#9ab6da]">
                      <div className="rounded-md bg-[#102033] px-2 py-1">TOP {km(ship.maxSpeed)}</div>
                      <div className="rounded-md bg-[#102033] px-2 py-1">HULL {pct(ship.maxHull, 240)}%</div>
                      <div className="rounded-md bg-[#102033] px-2 py-1">TORP {ship.torpedoAmmo}</div>
                    </div>
                    <div className="mt-3 flex items-center justify-between gap-3">
                      <span className="text-xs text-[#8da6c8]">{statusText}</span>
                      <Button
                        type="button"
                        size="sm"
                        disabled={active || !canBuy}
                        onClick={() => send({ c: 'buyShip', type: ship.type })}
                      >
                        BUY
                      </Button>
                    </div>
                  </div>
                );
              })}
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
