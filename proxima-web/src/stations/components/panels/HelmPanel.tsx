import { useCallback, useEffect, useRef } from 'react';

import { create } from 'zustand';

import { Button } from '@/components/ui/button';
import { cn } from '@/lib/utils';
import { DOCK_RANGE, REVERSE_THROTTLE_MIN } from '@/sim/data';
import type { Command } from '@/sim/types';
import { useGameStore } from '@/store/game';

import { LiveSlider } from '../LiveSlider';
import type { StationPanelProps } from '../StationPanelProps';

type ScopeMode = 'tactical' | 'map';

interface HelmScopeStore {
  readonly mode: ScopeMode;
  readonly toggleMode: () => void;
  readonly resetMode: () => void;
}

const useHelmScopeStore = create<HelmScopeStore>((set) => ({
  mode: 'tactical',
  toggleMode: () => set((state) => ({ mode: state.mode === 'map' ? 'tactical' : 'map' })),
  resetMode: () => set({ mode: 'tactical' }),
}));

const km = (n: number): string => `${(n / 1000).toFixed(1)} km`;
const pct = (v: number, max: number): number => (max > 0 ? Math.round((v / max) * 100) : 0);

function ReadoutRow({
  label,
  value,
  tone = 'normal',
}: {
  readonly label: string;
  readonly value: string;
  readonly tone?: 'normal' | 'accent' | 'danger' | 'success';
}) {
  return (
    <>
      <dt className="text-[11px] tracking-[0.16em] text-[#7d9dc4]">{label}</dt>
      <dd
        className={cn(
          'text-right text-sm font-semibold text-[#dbe7ff]',
          tone === 'accent' && 'text-[#7dd3fc]',
          tone === 'danger' && 'text-[#fda4af]',
          tone === 'success' && 'text-[#86efac]',
        )}
      >
        {value}
      </dd>
    </>
  );
}

const HOLD_BUTTON_CLASS = 'h-10 tracking-[0.18em] [touch-action:none]';

export function HelmPanel({ send }: StationPanelProps) {
  const snapshot = useGameStore((state) => state.snapshot);
  const player = snapshot?.player;
  const landmarks = snapshot?.landmarks ?? [];
  const objective = snapshot?.objective ?? null;
  const scopeMode = useHelmScopeStore((state) => state.mode);
  const toggleScopeMode = useHelmScopeStore((state) => state.toggleMode);
  const heldRef = useRef<number | null>(null);
  const releaseRef = useRef<Command | null>(null);

  const stopHold = useCallback((): void => {
    if (heldRef.current !== null) {
      window.clearInterval(heldRef.current);
      heldRef.current = null;
    }

    const release = releaseRef.current;
    releaseRef.current = null;
    if (release !== null) {
      send(release);
    }
  }, [send]);

  const startHold = useCallback(
    (cmd: Command, stopCmd: Command): void => {
      stopHold();
      send(cmd);
      releaseRef.current = stopCmd;
      heldRef.current = window.setInterval(() => {
        send(cmd);
      }, 100);
    },
    [send, stopHold],
  );

  const setThrottle = useCallback((v: number): void => send({ c: 'throttle', v }), [send]);

  useEffect(() => stopHold, [stopHold]);

  /**
   * A held control must release when the console loses the pointer for reasons the
   * element never sees — the tab going to the background, the phone locking, the browser
   * taking the gesture. Without this the interval keeps firing `turn` and the rudder
   * stays hard over while nobody is touching anything.
   */
  useEffect(() => {
    const release = (): void => stopHold();
    window.addEventListener('blur', release);
    window.addEventListener('pagehide', release);
    document.addEventListener('visibilitychange', release);
    return () => {
      window.removeEventListener('blur', release);
      window.removeEventListener('pagehide', release);
      document.removeEventListener('visibilitychange', release);
    };
  }, [stopHold]);

  if (player === undefined) {
    return (
      <section className="rounded-lg border border-[#1e3a5f] bg-[#06101c] p-3 text-xs text-[#9ab6da]">
        AWAITING HELM TELEMETRY
      </section>
    );
  }

  const station = landmarks.find((landmark) => landmark.kind === 'station') ?? null;
  const stationRange =
    station === null
      ? null
      : Math.hypot(station.pos.x - player.pos.x, station.pos.z - player.pos.z);
  const inDockRange = stationRange !== null && stationRange <= DOCK_RANGE;
  const hasStrafe = player.stats.strafeSpeed > 0;
  const hullPercent = pct(player.hull, player.maxHull);
  // Clamped to the sim's own range, NOT to 0. `Math.max(0, …)` put the lever's floor at
  // full stop, so REVERSE_THROTTLE_MIN was unreachable by drag and REV left the ship
  // making sternway while the readout insisted 0%.
  const throttleValue = Math.max(REVERSE_THROTTLE_MIN, Math.min(1, player.throttle));
  const dockLabel = player.docked ? 'UNDOCK' : 'DOCK';
  const warpLabel = `WARP ${Math.round(player.warpCharge * 100)}%`;
  const warpDisabled = player.warpCharge < 1 || player.docked;
  const courseDisabled = player.warpCharge < 1 || objective === null || player.docked;
  const dockDisabled = !player.docked && !inDockRange;
  const objectiveText = objective ? `${objective.name} · ${km(objective.range)}` : '—';
  const stationText = player.docked
    ? 'DOCKED'
    : stationRange === null
      ? 'NO STARBASE'
      : `${km(stationRange)}${inDockRange ? ' — IN RANGE' : ''}`;

  return (
    <section className="space-y-3 text-[#dbe7ff]">
      <div className="rounded-lg border border-[#1e3a5f] bg-[#06101c] p-3 text-xs text-[#9ab6da]">
        <div className="flex items-center gap-2 mb-2">
          <span className="shrink-0 text-[11px] tracking-[0.18em] text-[#7dd3fc]">THROTTLE</span>
          <LiveSlider
            label="THROTTLE"
            value={throttleValue}
            onChange={(v) => send({ c: 'throttle', v })}
            min={REVERSE_THROTTLE_MIN}
            max={1}
            step={0.01}
          />
          <b
            data-testid="throttle-readout"
            className="w-12 shrink-0 text-right text-sm text-[#dbe7ff]"
          >
            {Math.round(throttleValue * 100)}%
          </b>
        </div>

        <div className="grid grid-cols-4 gap-1.5 mb-2">
          <Button type="button" size="sm" variant="outline" onClick={() => setThrottle(1)}>
            FULL
          </Button>
          <Button type="button" size="sm" variant="outline" onClick={() => setThrottle(0.5)}>
            HALF
          </Button>
          <Button type="button" size="sm" variant="outline" onClick={() => setThrottle(0)}>
            STOP
          </Button>
          <Button
            type="button"
            size="sm"
            variant="outline"
            onClick={() => setThrottle(REVERSE_THROTTLE_MIN)}
          >
            REV
          </Button>
        </div>

        <div className="grid grid-cols-2 gap-1.5 mb-2">
          <Button
            type="button"
            size="lg"
            variant="outline"
            className={HOLD_BUTTON_CLASS}
            onPointerDown={() => startHold({ c: 'turn', v: -1 }, { c: 'turn', v: 0 })}
            onPointerUp={stopHold}
            onPointerLeave={stopHold}
            onPointerCancel={stopHold}
          >
            PORT
          </Button>
          <Button
            type="button"
            size="lg"
            variant="outline"
            className={HOLD_BUTTON_CLASS}
            onPointerDown={() => startHold({ c: 'turn', v: 1 }, { c: 'turn', v: 0 })}
            onPointerUp={stopHold}
            onPointerLeave={stopHold}
            onPointerCancel={stopHold}
          >
            STBD
          </Button>
        </div>

        <div className="grid grid-cols-2 gap-1.5 mb-2">
          <Button
            type="button"
            size="lg"
            variant="outline"
            disabled={!hasStrafe}
            className={HOLD_BUTTON_CLASS}
            onPointerDown={() => startHold({ c: 'strafe', v: -1 }, { c: 'strafe', v: 0 })}
            onPointerUp={stopHold}
            onPointerLeave={stopHold}
            onPointerCancel={stopHold}
          >
            SLIDE LEFT
          </Button>
          <Button
            type="button"
            size="lg"
            variant="outline"
            disabled={!hasStrafe}
            className={HOLD_BUTTON_CLASS}
            onPointerDown={() => startHold({ c: 'strafe', v: 1 }, { c: 'strafe', v: 0 })}
            onPointerUp={stopHold}
            onPointerLeave={stopHold}
            onPointerCancel={stopHold}
          >
            SLIDE RIGHT
          </Button>
        </div>

        {!hasStrafe ? (
          <p className="mb-2 rounded-md border border-dashed border-[#1e3a5f] bg-[#08101c] px-3 py-2 text-[11px] text-[#7d9dc4]">
            Manoeuvring thrusters not installed — buy them at the drydock.
          </p>
        ) : null}

        <div className="grid grid-cols-2 gap-1.5 mb-2">
          <Button
            type="button"
            variant="outline"
            disabled={dockDisabled}
            onClick={() => send({ c: 'dock' })}
          >
            {dockLabel}
          </Button>
          <Button
            type="button"
            variant="outline"
            disabled={warpDisabled}
            onClick={() => send({ c: 'warp' })}
          >
            {warpLabel}
          </Button>
        </div>

        <div className="grid grid-cols-2 gap-1.5">
          <Button
            type="button"
            variant="outline"
            disabled={courseDisabled}
            onClick={() => send({ c: 'layInCourse' })}
          >
            LAY IN COURSE
          </Button>
          <Button
            type="button"
            variant="outline"
            aria-pressed={scopeMode === 'map'}
            className={cn(scopeMode === 'map' && 'border-[#7dd3fc] bg-[#102033] text-[#dbe7ff]')}
            onClick={toggleScopeMode}
          >
            SECTOR MAP
          </Button>
        </div>
      </div>

      <div className="rounded-lg border border-[#1e3a5f] bg-[#06101c] p-3">
        {/* Orders are on Science — Helm keeps only the bearing to steer to. */}
        <dl className="grid grid-cols-[auto_1fr] gap-x-3 gap-y-2">
          <ReadoutRow
            label="SPEED"
            value={`${Math.round(player.speed)} / ${Math.round(player.maxSpeed)}`}
            tone="accent"
          />
          <ReadoutRow
            label="HULL"
            value={`${hullPercent}%`}
            tone={player.hullCritical ? 'danger' : 'success'}
          />
          <ReadoutRow
            label="STARBASE"
            value={stationText}
            tone={player.docked || inDockRange ? 'success' : 'normal'}
          />
          <ReadoutRow
            label="OBJECTIVE"
            value={objectiveText}
            tone={objective ? 'accent' : 'normal'}
          />
        </dl>
      </div>
    </section>
  );
}

export { useHelmScopeStore };
