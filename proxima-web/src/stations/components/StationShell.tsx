import { type ReactNode, useEffect, useRef, useState } from 'react';
import { cn } from '@/lib/utils';
import type { Command, Snapshot, Station } from '@/sim/types';
import { gameStore, useGameStore } from '@/store/game';
import { Scope } from '../ui/scope';
import { EngineeringPanel } from './EngineeringPanel';
import { HelmPanel, useHelmScopeStore } from './HelmPanel';
import { SciencePanel, useScienceScopeStore } from './SciencePanel';
import { WeaponsPanel } from './WeaponsPanel';

export type StationConnectionState = 'connecting' | 'pin-required' | 'linked' | 'waiting';

interface StationShellProps {
  readonly connectionState: StationConnectionState;
  readonly send: (cmd: Command, id?: number) => void;
}

const STATION_LABELS = {
  helm: 'HELM',
  weapons: 'WPN',
  engineering: 'ENG',
  science: 'SCI',
} as const satisfies Record<Station, string>;

const STATIONS = [
  'helm',
  'weapons',
  'engineering',
  'science',
] as const satisfies readonly Station[];

const stationFromHash = (hash: string): Station => {
  const station = hash.slice(1);

  switch (station) {
    case 'helm':
    case 'weapons':
    case 'engineering':
    case 'science':
      return station;
    default:
      return 'helm';
  }
};

const statusMeta = (
  connectionState: StationConnectionState,
): { readonly text: string; readonly className: string } => {
  switch (connectionState) {
    case 'pin-required':
      return {
        text: 'PIN REQUIRED',
        className: 'border-[#7f1d1d] bg-[#2a0f17] text-[#fda4af]',
      };
    case 'connecting':
      return {
        text: 'NO LINK — RECONNECTING…',
        className: 'border-[#1e3a5f] bg-[#08101c] text-[#93c5fd]',
      };
    case 'waiting':
      return {
        text: 'LINKED — WAITING FOR SHIP',
        className: 'border-[#7c5a1b] bg-[#221506] text-[#fcd34d]',
      };
    case 'linked':
      return {
        text: 'LINKED',
        className: 'border-[#14532d] bg-[#052013] text-[#86efac]',
      };
    default:
      return assertNever(connectionState);
  }
};

const noticeText = (connectionState: StationConnectionState): string | null => {
  switch (connectionState) {
    case 'pin-required':
      return 'That session PIN was wrong. The pilot screen shows the current one.';
    case 'connecting':
      return 'Cannot reach the ship. Check you are on the same network as the host.';
    case 'waiting':
      return 'Connected, but nothing is flying yet. On the host machine, open the game and press LAUNCH.';
    case 'linked':
      return null;
    default:
      return assertNever(connectionState);
  }
};

const footerText = (
  phase: string | null,
  mode: string | null,
  skirmishWave: number,
  objective: { name: string; live?: boolean; offered?: boolean } | null,
): string => {
  if (phase === null) return 'AWAITING TELEMETRY';
  if (phase === 'victory') return 'THE VEIL IS SECURE';
  if (phase === 'defeat') return 'SHIP LOST';
  if (mode === 'skirmish') return `SKIRMISH — WAVE ${skirmishWave}`;
  if (objective?.live) return 'ENCOUNTER LIVE';
  if (objective?.offered) return `AWAITING ORDERS — ${objective.name}`;
  if (objective) return `EN ROUTE — ${objective.name}`;
  return 'SECTOR CLEAR';
};

const renderActivePanel = (activeTab: Station, send: StationShellProps['send']): ReactNode => {
  switch (activeTab) {
    case 'helm':
      return <HelmPanel send={send} />;
    case 'weapons':
      return <WeaponsPanel send={send} />;
    case 'engineering':
      return <EngineeringPanel send={send} />;
    case 'science':
      return <SciencePanel send={send} />;
    default:
      return assertNever(activeTab);
  }
};

function assertNever(value: never): never {
  throw new Error(`Unexpected station value: ${String(value)}`);
}

export function StationShell({ connectionState, send }: StationShellProps) {
  const snapshot = useGameStore((state) => state.snapshot);
  const phase = snapshot?.phase ?? null;
  const mode = snapshot?.mode ?? null;
  const skirmishWave = snapshot?.skirmishWave ?? 0;
  const objective = snapshot?.objective ?? null;
  const isRedAlert = snapshot?.alert === 'red';
  const helmScopeMode = useHelmScopeStore((state) => state.mode);
  const resetHelmScopeMode = useHelmScopeStore((state) => state.resetMode);
  const scienceScopeMode = useScienceScopeStore((state) => state.mode);
  const resetScienceScopeMode = useScienceScopeStore((state) => state.resetMode);
  const radarRef = useRef<HTMLCanvasElement>(null);
  const [pin, setPin] = useState('');
  const [activeTab, setActiveTab] = useState<Station>(() => stationFromHash(location.hash));

  useEffect(() => {
    const canvas = radarRef.current;
    if (canvas === null) return;

    const scope = new Scope(canvas);
    const drawSnapshot = (nextSnapshot: Snapshot | null): void => {
      if (nextSnapshot === null) return;
      scope.draw(
        nextSnapshot,
        activeTab === 'science'
          ? scienceScopeMode
          : activeTab === 'helm'
            ? helmScopeMode
            : 'tactical',
      );
    };

    drawSnapshot(gameStore.getState().snapshot);
    return gameStore.subscribe((state) => state.snapshot, drawSnapshot);
  }, [activeTab, helmScopeMode, scienceScopeMode]);

  useEffect(() => {
    if (activeTab !== 'helm') resetHelmScopeMode();
    if (activeTab !== 'science') resetScienceScopeMode();
  }, [activeTab, resetHelmScopeMode, resetScienceScopeMode]);

  useEffect(() => {
    const syncTab = (): void => {
      setActiveTab(stationFromHash(location.hash));
    };

    window.addEventListener('hashchange', syncTab);
    return () => {
      window.removeEventListener('hashchange', syncTab);
    };
  }, []);

  const showLivePanels = connectionState === 'linked';
  const { text: statusText, className: statusClass } = statusMeta(connectionState);
  const notice = noticeText(connectionState);

  const selectTab = (nextTab: Station): void => {
    setActiveTab(nextTab);
    location.hash = nextTab;
  };

  return (
    <div
      className={cn(
        'min-h-screen bg-[#05070f] font-mono text-[#dbe7ff]',
        isRedAlert && 'bg-[#14060a]',
      )}
    >
      <div className="mx-auto max-w-[560px] px-3 py-3 pb-10">
        <header className="mb-3 flex items-center justify-between gap-3">
          <h1 className="text-sm font-medium tracking-[0.28em] text-[#7dd3fc]">PROXIMA</h1>
          <div
            id="status"
            className={cn('rounded border px-2 py-1 text-[11px] tracking-[0.18em]', statusClass)}
          >
            {statusText}
          </div>
        </header>

        {connectionState === 'pin-required' ? (
          <form
            id="pinform"
            className="mb-3 rounded-lg border border-[#7f1d1d] bg-[#170a0f] p-3"
            onSubmit={(event) => {
              event.preventDefault();
              const value = pin.trim();
              if (!value) return;

              sessionStorage.setItem('proxima.pin', value);
              location.replace(`${location.pathname}${location.hash}`);
            }}
          >
            <label
              className="mb-2 block text-[11px] tracking-[0.18em] text-[#fda4af]"
              htmlFor="station-pin"
            >
              SESSION PIN
            </label>
            <div className="flex gap-2">
              <input
                id="pin"
                className="min-w-0 flex-1 rounded-md border border-[#7f1d1d] bg-[#05070f] px-3 py-2 text-sm outline-none ring-0 placeholder:text-[#6b7280] focus:border-[#fb7185]"
                inputMode="numeric"
                autoComplete="one-time-code"
                maxLength={12}
                value={pin}
                onChange={(event) => setPin(event.target.value)}
                placeholder="Enter crew PIN"
              />
              <button
                className="rounded-md border border-[#7f1d1d] bg-[#3f0f19] px-3 py-2 text-xs tracking-[0.16em] text-[#ffe4e6] transition hover:bg-[#561220]"
                type="submit"
              >
                LINK
              </button>
            </div>
          </form>
        ) : null}

        {notice ? (
          <div className="mb-3 rounded-lg border border-[#1e293b] bg-[#08101c] px-3 py-2 text-xs leading-5 text-[#9ab6da]">
            {notice}
          </div>
        ) : null}

        <button
          className={cn(
            'mb-3 w-full rounded-lg border px-3 py-2 text-sm tracking-[0.2em] transition',
            isRedAlert
              ? 'border-[#dc2626] bg-[#4c0b14] text-[#ffe4e6] hover:bg-[#5d0d18]'
              : 'border-[#1e3a5f] bg-[#08101c] text-[#fca5a5] hover:bg-[#0b1524]',
          )}
          type="button"
          onClick={() => send({ c: 'alert', state: 'toggle' })}
        >
          {isRedAlert ? 'STAND DOWN' : 'RED ALERT'}
        </button>

        <div className="mb-3 grid grid-cols-4 gap-1.5" id="tabs">
          {STATIONS.map((tab) => (
            <button
              key={tab}
              className={cn(
                'rounded-md border px-2 py-2 text-xs tracking-[0.18em] transition',
                activeTab === tab
                  ? 'border-[#7dd3fc] bg-[#102033] text-[#dbe7ff]'
                  : 'border-[#1e3a5f] bg-[#08101c] text-[#8da6c8] hover:bg-[#0b1524] hover:text-[#dbe7ff]',
              )}
              data-testid={`tab-${tab}`}
              type="button"
              aria-pressed={activeTab === tab}
              onClick={() => selectTab(tab)}
            >
              {STATION_LABELS[tab]}
            </button>
          ))}
        </div>

        <canvas
          ref={radarRef}
          className={cn(
            'mb-3 block aspect-square w-full rounded-lg border border-[#1e3a5f] bg-[#060d18]',
            !showLivePanels && 'hidden',
          )}
        />

        <div className={cn('mb-3 min-h-24', !showLivePanels && 'hidden')} id="panel">
          {renderActivePanel(activeTab, send)}
        </div>

        <footer className="rounded-lg border border-[#1e293b] bg-[#08101c] px-3 py-2 text-center text-[11px] tracking-[0.18em] text-[#8da6c8]">
          {footerText(phase, mode, skirmishWave, objective)}
        </footer>
      </div>
    </div>
  );
}
