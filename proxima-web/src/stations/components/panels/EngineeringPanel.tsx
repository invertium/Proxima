import { useCallback, useEffect, useRef } from 'react';

import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { cn } from '@/lib/utils';
import { WELD_GREEN_MAX, WELD_GREEN_MIN, WELD_SWEEP_PERIOD } from '@/sim/data';
import type { DamageSystem, ShipSystem } from '@/sim/types';
import { pct } from '@/stations/lib/utils';
import { useGameStore } from '@/store/game';
import { LiveSlider } from '../LiveSlider';
import type { StationPanelProps } from '../StationPanelProps';
import { EngineeringDrydock } from './EngineeringDrydock';

const POWER_SYSTEMS = [
  { key: 'engines', label: 'ENGINES' },
  { key: 'weapons', label: 'WEAPONS' },
  { key: 'shields', label: 'SHIELDS' },
] as const satisfies readonly { readonly key: ShipSystem; readonly label: string }[];

const DAMAGE_SYSTEMS = [
  { key: 'engine', label: 'ENGINE' },
  { key: 'weapons', label: 'WEAPONS' },
  { key: 'sensors', label: 'SENSORS' },
] as const satisfies readonly { readonly key: DamageSystem; readonly label: string }[];

const PRESETS = [
  { label: 'COMBAT', power: { engines: 0.4, weapons: 1.3, shields: 1.3 } },
  { label: 'TRAVEL', power: { engines: 1.6, weapons: 0.7, shields: 0.7 } },
  { label: 'BALANCED', power: { engines: 1, weapons: 1, shields: 1 } },
] as const;

const SWEEP_PERIOD_MS = WELD_SWEEP_PERIOD * 1000;
const SWEEP_CLASS =
  'relative h-10 overflow-hidden rounded-full border border-[#1e3a5f] bg-[#08101c]';
const SWEEP_HIT_CLASS =
  'border-emerald-400 shadow-[0_0_0_1px_rgba(74,222,128,0.95),0_0_18px_rgba(74,222,128,0.45)]';
const SWEEP_MISS_CLASS =
  'border-rose-400 shadow-[0_0_0_1px_rgba(251,113,133,0.95),0_0_18px_rgba(251,113,133,0.45)]';

const damageChipClassName = (damaged: boolean): string =>
  cn(
    'justify-between gap-2 rounded-full border px-2.5 py-1 font-medium tracking-[0.14em]',
    damaged
      ? 'border-rose-500/60 bg-rose-950/55 text-rose-200'
      : 'border-emerald-500/45 bg-emerald-950/35 text-emerald-200',
  );

export function EngineeringPanel({ send }: StationPanelProps) {
  const snapshot = useGameStore((state) => state.snapshot);
  const player = snapshot?.player ?? null;

  const markerRef = useRef<HTMLDivElement>(null);
  const sweepRef = useRef<HTMLDivElement>(null);
  const flashTimeoutRef = useRef<number | null>(null);

  const currentPhase = useCallback((): number => {
    return (performance.now() % SWEEP_PERIOD_MS) / SWEEP_PERIOD_MS;
  }, []);

  const flashSweep = useCallback((credited: boolean): void => {
    const sweep = sweepRef.current;
    if (sweep === null) return;

    if (flashTimeoutRef.current !== null) {
      window.clearTimeout(flashTimeoutRef.current);
    }

    sweep.className = SWEEP_CLASS;
    void sweep.offsetWidth;
    sweep.className = credited
      ? `${SWEEP_CLASS} ${SWEEP_HIT_CLASS}`
      : `${SWEEP_CLASS} ${SWEEP_MISS_CLASS}`;

    flashTimeoutRef.current = window.setTimeout(() => {
      if (sweepRef.current !== null) {
        sweepRef.current.className = SWEEP_CLASS;
      }
      flashTimeoutRef.current = null;
    }, 180);
  }, []);

  useEffect(() => {
    let rafId = 0;

    const tick = (): void => {
      const pctLeft = currentPhase() * 100;
      if (markerRef.current !== null) {
        markerRef.current.style.left = `${pctLeft}%`;
      }
      rafId = requestAnimationFrame(tick);
    };

    rafId = requestAnimationFrame(tick);
    return () => {
      cancelAnimationFrame(rafId);
      if (flashTimeoutRef.current !== null) {
        window.clearTimeout(flashTimeoutRef.current);
      }
    };
  }, [currentPhase]);

  if (player === null) {
    return (
      <section className="rounded-lg border border-[#1e3a5f] bg-[#06101c] p-3 text-xs text-[#9ab6da]">
        AWAITING ENGINEERING TELEMETRY
      </section>
    );
  }

  const reactorUsed = POWER_SYSTEMS.reduce((sum, system) => sum + player.power[system.key], 0);
  const reactorLoadPct = pct(reactorUsed, player.stats.reactorBudget);
  const weldHint = player.repairTarget
    ? `Repairing ${player.repairTarget.toUpperCase()} — ${player.repairWelds}/3 welds. Release in the green.`
    : 'Release in the green to patch hull.';

  const applyPreset = (power: (typeof PRESETS)[number]['power']): void => {
    for (const system of POWER_SYSTEMS) {
      send({ c: 'power', system: system.key, v: 0 });
    }
    send({ c: 'power', system: 'engines', v: power.engines });
    send({ c: 'power', system: 'weapons', v: power.weapons });
    send({ c: 'power', system: 'shields', v: power.shields });
  };

  const handleWeld = (): void => {
    const phase = currentPhase();
    flashSweep(phase >= WELD_GREEN_MIN && phase <= WELD_GREEN_MAX);
    send({ c: 'weld', phase });
  };

  return (
    <section className="space-y-3 text-[#dbe7ff]">
      <div className="rounded-lg border border-[#1e3a5f] bg-[#06101c] p-3">
        <div className="mb-2 flex items-center justify-between text-[11px] tracking-[0.18em] text-[#7dd3fc]">
          <span>REACTOR CONTROL</span>
          <span>
            {reactorUsed.toFixed(1)} / {player.stats.reactorBudget.toFixed(1)} · {reactorLoadPct}%
          </span>
        </div>
        <div className="mb-3 h-2 overflow-hidden rounded-full bg-[#10203a]">
          <div
            className="h-full bg-[#7dd3fc] transition-none"
            style={{ width: `${reactorLoadPct}%` }}
          />
        </div>
        <div className="space-y-3">
          {POWER_SYSTEMS.map((system) => (
            <div key={system.key} className="space-y-1.5">
              <div className="flex items-center justify-between text-[11px] tracking-[0.14em] text-[#9ab6da]">
                <span>{system.label}</span>
                <b
                  className={cn(
                    'font-semibold text-[#dbe7ff]',
                    player.power[system.key] <= 0.01 && 'text-rose-300',
                  )}
                >
                  {player.power[system.key].toFixed(1)}
                </b>
              </div>
              <LiveSlider
                label={`${system.label} POWER`}
                value={player.power[system.key]}
                onChange={(v) => send({ c: 'power', system: system.key, v })}
                min={0}
                max={2}
                step={0.1}
              />
            </div>
          ))}
        </div>
        <div className="mt-3 grid grid-cols-3 gap-2">
          {PRESETS.map((preset) => (
            <Button
              key={preset.label}
              type="button"
              size="sm"
              variant="outline"
              onClick={() => applyPreset(preset.power)}
            >
              {preset.label}
            </Button>
          ))}
        </div>
      </div>

      <div className="rounded-lg border border-[#1e3a5f] bg-[#06101c] p-3">
        <div className="mb-3 text-[11px] tracking-[0.18em] text-[#7dd3fc]">DAMAGE CONTROL</div>
        <div className="mb-3 flex flex-wrap gap-2">
          {DAMAGE_SYSTEMS.map((system) => {
            const damaged = player.damaged[system.key];
            return (
              <Badge key={system.key} variant="outline" className={damageChipClassName(damaged)}>
                <span>{system.label}</span>
                <span>{damaged ? 'BAD' : 'OK'}</span>
              </Badge>
            );
          })}
        </div>
        <div className="grid gap-3 md:grid-cols-[1fr_auto] md:items-center">
          <div>
            <div ref={sweepRef} className={SWEEP_CLASS}>
              <div
                className="absolute inset-y-0 rounded-full bg-emerald-400/25"
                style={{
                  left: `${WELD_GREEN_MIN * 100}%`,
                  width: `${(WELD_GREEN_MAX - WELD_GREEN_MIN) * 100}%`,
                }}
              />
              <div
                ref={markerRef}
                className="absolute top-1/2 h-6 w-[2px] -translate-x-1/2 -translate-y-1/2 bg-[#f8fafc]"
              />
            </div>
            <p className="mt-2 text-xs leading-5 text-[#9ab6da]">{weldHint}</p>
          </div>
          <Button type="button" size="lg" className="tracking-[0.22em]" onClick={handleWeld}>
            WELD
          </Button>
        </div>
      </div>

      <EngineeringDrydock player={player} send={send} />
    </section>
  );
}
