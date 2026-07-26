import { useMemo } from 'react';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { cn } from '@/lib/utils';
import type { Command, Snapshot } from '@/sim/types';
import { useGameStore } from '@/store/game';

import { km } from '../../lib/utils';

interface WeaponsPanelProps {
  readonly send: (cmd: Command, id?: number) => void;
}

type Contact = Snapshot['contacts'][number];

const EMPTY_CONTACTS: Snapshot['contacts'] = [];

const getSolutionLabel = (contact: Contact): 'BEAM SOLUTION' | 'TORPEDO ARC ONLY' | 'SHIELDED' | 'NO SOLUTION' => {
  if (contact.shielded || (contact.shield > 0 && !contact.scanned)) {
    return 'SHIELDED';
  }
  if (contact.inBeamArc) {
    return 'BEAM SOLUTION';
  }
  if (contact.inTorpedoArc) {
    return 'TORPEDO ARC ONLY';
  }
  return 'NO SOLUTION';
};

const solutionClassName = (label: ReturnType<typeof getSolutionLabel>): string => {
  switch (label) {
    case 'BEAM SOLUTION':
      return 'text-[#4ade80]';
    case 'TORPEDO ARC ONLY':
      return 'text-[#fbbf24]';
    case 'SHIELDED':
      return 'text-[#f87171]';
    case 'NO SOLUTION':
      return 'text-[#94a3b8]';
    default:
      return assertNever(label);
  }
};

const assertNever = (value: never): never => {
  throw new Error(`Unexpected solution label: ${String(value)}`);
};

export function WeaponsPanel({ send }: WeaponsPanelProps) {
  const snapshot = useGameStore((state) => state.snapshot);
  const player = snapshot?.player ?? null;
  const contacts = snapshot?.contacts ?? EMPTY_CONTACTS;
  
  const targetId = player?.targetId ?? null;
  const target = useMemo(() => {
    if (!player) return null;
    return contacts.find((contact) => contact.id === player.targetId) ?? null;
  }, [player, contacts]);
  
  const beamCharge = player?.beamCharge ?? 0;
  const torpedoAmmo = player?.torpedoAmmo ?? 0;
  const torpedoReload = player?.torpedoReload ?? 0;
  const weaponsPower = player?.power.weapons ?? 0;
  const beamDamage = player?.stats.beamDamage ?? 0;
  const beamArcDeg = player?.stats.beamArcDeg ?? 0;
  const turretDamage = player?.stats.turretDamage ?? 0;
  const weaponsDamaged = player?.damaged.weapons ?? false;

  const beamChargePercent = Math.round(beamCharge * 100);
  const torpedoReloading = torpedoReload > 0;
  const beamDisabled = beamCharge < 1 || !target?.inBeamArc;
  const torpedoDisabled = torpedoAmmo <= 0 || torpedoReloading || !target?.inTorpedoArc;
  const turretStatus = turretDamage > 0 ? `${Math.round(turretDamage)} dmg auto` : 'OFFLINE';

  return (
    <section className="space-y-3 rounded-lg border border-[#1e3a5f] bg-[#06101c] p-3 text-xs text-[#9ab6da]">
      <div className="flex flex-wrap items-center justify-between gap-2">
        <div>
          <h2 className="text-sm font-semibold tracking-[0.24em] text-[#dbe7ff]">WEAPONS</h2>
          <p className="mt-1 text-[11px] text-[#7d9dc4]">
            {target ? `${target.name} targeted` : 'Select a contact to target'}
          </p>
        </div>
        <div className="flex flex-wrap gap-1.5">
          <Badge variant={weaponsDamaged ? 'destructive' : 'secondary'}>
            WPN PWR {weaponsPower}
          </Badge>
          {weaponsDamaged ? <Badge variant="destructive">DAMAGED</Badge> : null}
        </div>
      </div>

      <div className="space-y-1.5">
        {contacts.length > 0 ? (
          contacts.map((contact) => {
            const solutionLabel = getSolutionLabel(contact);

            return (
              <button
                key={contact.id}
                type="button"
                onClick={() => send({ c: 'target', id: contact.id })}
                className={cn(
                  'w-full rounded-lg border border-[#1e3a5f] bg-[#08101c] px-3 py-2 text-left transition hover:bg-[#0b1524]',
                  contact.id === targetId && 'border-[#fbbf24] bg-[#2a2110]',
                )}
              >
                <div className="flex items-start justify-between gap-3">
                  <div className="min-w-0 flex-1">
                    <b className="block truncate text-sm text-[#dbe7ff]">{contact.name}</b>
                    <span className="block text-[11px] text-[#7d9dc4]">
                      {contact.className} · {km(contact.range)}
                    </span>
                    {contact.scanned ? (
                      <span className="block text-[11px] text-[#9ab6da]">
                        hull {Math.round(contact.hull)}/{Math.round(contact.maxHull)} · shield {Math.round(contact.shield)}
                      </span>
                    ) : (
                      <span className="block text-[11px] text-[#9ab6da]">unscanned — Science can resolve it</span>
                    )}
                  </div>
                  <em
                    className={cn(
                      'shrink-0 pt-0.5 text-right text-[10px] not-italic tracking-[0.16em]',
                      solutionClassName(solutionLabel),
                    )}
                  >
                    {solutionLabel}
                  </em>
                </div>
              </button>
            );
          })
        ) : (
          <p className="rounded-lg border border-dashed border-[#1e3a5f] bg-[#08101c] px-3 py-4 text-center text-[#7d9dc4]">
            No contacts.
          </p>
        )}
      </div>

      <div className="grid grid-cols-2 gap-2">
        <Button
          type="button"
          onClick={() => send({ c: 'fireBeam' })}
          disabled={beamDisabled}
          className="h-10 border-[#854d0e] bg-[#2a1a08] text-[#fcd34d] hover:bg-[#3a240b]"
        >
          FIRE BEAM · {beamChargePercent}%
        </Button>
        <Button
          type="button"
          onClick={() => send({ c: 'fireTorpedo' })}
          disabled={torpedoDisabled}
          className="h-10 border-[#1d4ed8] bg-[#0a1733] text-[#bfdbfe] hover:bg-[#11204a]"
        >
          {torpedoReloading ? `TORPEDO · ${torpedoReload.toFixed(1)}s` : `TORPEDO · ${torpedoAmmo}`}
        </Button>
      </div>

      <dl className="grid grid-cols-2 gap-2 rounded-lg border border-[#1e293b] bg-[#08101c] p-3 text-[11px]">
        <div>
          <dt className="text-[#7d9dc4]">BEAM DMG</dt>
          <dd className="mt-1 text-sm font-semibold text-[#dbe7ff]">{Math.round(beamDamage)}</dd>
        </div>
        <div>
          <dt className="text-[#7d9dc4]">BEAM ARC</dt>
          <dd className="mt-1 text-sm font-semibold text-[#dbe7ff]">{Math.round(beamArcDeg)}°</dd>
        </div>
        <div className="col-span-2">
          <dt className="text-[#7d9dc4]">TURRET STATUS</dt>
          <dd className="mt-1 text-sm font-semibold text-[#dbe7ff]">{turretStatus}</dd>
        </div>
      </dl>
    </section>
  );
}
