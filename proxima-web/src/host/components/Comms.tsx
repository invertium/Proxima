import { useMemo } from 'react';

import type { Snapshot } from '@/sim/types';
import { useGameStore } from '@/store/game';

type CommsEntry = Snapshot['comms'][number];

const EMPTY_COMMS: readonly CommsEntry[] = [];

export function Comms() {
  const snapshot = useGameStore((state) => state.snapshot);
  const allComms = snapshot?.comms ?? EMPTY_COMMS;
  const comms = useMemo(() => allComms.slice(-5), [allComms]);

  if (comms.length === 0) return null;

  return (
    <div className="pointer-events-none fixed bottom-4 left-4 z-20 flex w-[520px] max-h-[30vh] flex-col gap-1 overflow-hidden rounded-2xl border border-white/10 bg-[#07111fcc] px-4 py-3 shadow-[0_14px_40px_rgba(0,0,0,0.42)] backdrop-blur-md">
      {comms.map((comm, index) => (
        <p key={`${comm.at}-${index}`} className="text-sm leading-relaxed text-[#d7e7ff]">
          <b className="mr-2 text-[#38bdf8]">{comm.sender}</b>
          {comm.text}
        </p>
      ))}
    </div>
  );
}
