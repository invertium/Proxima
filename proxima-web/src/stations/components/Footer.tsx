import { cn } from '@/lib/utils';
import type { RelayStation } from '@/net/transport';
import { useGameStore } from '@/store/game';

interface FooterProps {
  relay: RelayStation;
}

export function Footer({ relay }: FooterProps) {
  const snapshot = useGameStore((state) => state.snapshot);
  const phase = snapshot?.phase;
  const mode = snapshot?.mode;
  const wave = snapshot?.skirmishWave ?? 0;
  const objective = snapshot?.objective ?? null;

  if (!phase) {
    return null;
  }

  let phaseText: string;
  if (phase === 'victory') {
    phaseText = 'THE VEIL IS SECURE';
  } else if (phase === 'defeat') {
    phaseText = 'SHIP LOST';
  } else if (mode === 'skirmish') {
    phaseText = `SKIRMISH — WAVE ${wave}`;
  } else if (objective?.live) {
    phaseText = 'ENCOUNTER LIVE';
  } else if (objective?.offered) {
    phaseText = `AWAITING ORDERS — ${objective.name}`;
  } else if (objective) {
    phaseText = `EN ROUTE — ${objective.name}`;
  } else {
    phaseText = 'SECTOR CLEAR';
  }

  const isOver = phase !== 'playing';
  const isVictory = phase === 'victory';

  const sendGame = (action: 'restart' | 'new') => {
    relay.send({ m: 'game', action });
  };

  return (
    <div className="mt-5 pt-3 border-t border-[#1e3a5f]">
      <p
        className={cn(
          'text-center text-[11px] tracking-[0.12em] mb-3',
          isOver && isVictory
            ? 'text-[#4ade80] text-base'
            : isOver
              ? 'text-[#f87171] text-base'
              : 'text-[#7d9dc4]',
        )}
      >
        {phaseText}
      </p>
      {isOver && (
        <div className="grid grid-cols-2 gap-1.5">
          <button
            type="button"
            onClick={() => sendGame('restart')}
            className="px-2 py-1 text-xs font-semibold bg-[#1e3a5f] text-[#f87171] border border-[#f87171] rounded hover:bg-[#2a4a7f] transition-colors"
          >
            RETRY FROM LAST SAVE
          </button>
          <button
            type="button"
            onClick={() => sendGame('new')}
            className="px-2 py-1 text-xs font-semibold bg-[#1e3a5f] text-[#7d9dc4] border border-[#7d9dc4] rounded hover:bg-[#2a4a7f] transition-colors"
          >
            NEW CAMPAIGN
          </button>
        </div>
      )}
    </div>
  );
}
