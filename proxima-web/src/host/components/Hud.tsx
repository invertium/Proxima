import { useEffect, useMemo, useState } from 'react';

import { onServerMessage } from '@/host/main';
import type { Snapshot } from '@/sim/types';
import { useGameStore } from '@/store/game';

type Contact = Snapshot['contacts'][number];

const EMPTY_CONTACTS: readonly Contact[] = [];

interface BarProps {
  readonly label: string;
  readonly value: number;
  readonly max: number;
  readonly color: string;
}

function Bar({ label, value, max, color }: BarProps) {
  const pct = max > 0 ? Math.max(0, Math.min(1, value / max)) * 100 : 0;
  return (
    <div className="my-0.5 flex items-center gap-2 text-[11px] text-[#d7e7ff]">
      <span className="w-[52px] tracking-wide text-[#7d9dc4]">{label}</span>
      <div className="h-[7px] flex-1 overflow-hidden rounded-full bg-[#10203a]">
        <div style={{ width: `${pct}%`, background: color }} className="h-full transition-none" />
      </div>
      <b className="min-w-[38px] text-right font-semibold">{Math.round(value)}</b>
    </div>
  );
}

function Stat({ label, value }: { readonly label: string; readonly value: string | number }) {
  return (
    <div className="flex items-center gap-1.5">
      <span className="text-[#7d9dc4]">{label}</span>
      <b className="font-semibold text-[#d7e7ff]">{value}</b>
    </div>
  );
}

export function Hud() {
  const snapshot = useGameStore((state) => state.snapshot);
  const player = snapshot?.player ?? null;
  const alert = snapshot?.alert;
  const mode = snapshot?.mode;
  const skirmishWave = snapshot?.skirmishWave;
  const objective = snapshot?.objective ?? null;
  const event = snapshot?.event ?? null;
  const contract = snapshot?.contract ?? null;
  const contacts = snapshot?.contacts ?? EMPTY_CONTACTS;
  
  const target = useMemo(() => {
    if (!player) return null;
    return contacts.find((contact) => contact.id === player.targetId) ?? null;
  }, [player, contacts]);

  const [refusalText, setRefusalText] = useState('');
  const [refusalUntil, setRefusalUntil] = useState(0);
  const [now, setNow] = useState(() => performance.now());

  useEffect(() => {
    return onServerMessage((msg) => {
      if (msg.m !== 'ack') return;
      const refusal = msg.acks.find((ack) => !ack.ok && ack.id === undefined && ack.reason);
      if (!refusal?.reason) return;
      const expiry = performance.now() + 2500;
      setRefusalText(refusal.reason);
      setRefusalUntil(expiry);
      setNow(performance.now());
    });
  }, []);

  useEffect(() => {
    if (refusalUntil <= performance.now()) return;
    const timer = window.setInterval(() => {
      const nextNow = performance.now();
      setNow(nextNow);
      if (nextNow >= refusalUntil) {
        window.clearInterval(timer);
      }
    }, 100);
    return () => {
      window.clearInterval(timer);
    };
  }, [refusalUntil]);

  const heading = useMemo(() => {
    if (!player) return 0;
    return Math.round((((player.heading * 180) / Math.PI + 360) % 360));
  }, [player]);
  const refusalVisible = refusalText.length > 0 && now < refusalUntil;

  if (!player) return null;

  return (
    <div className="pointer-events-none fixed top-4 left-4 z-20 w-[340px] rounded-2xl border border-white/10 bg-[#07111fcc] p-4 shadow-[0_14px_40px_rgba(0,0,0,0.42)] backdrop-blur-md">
      <div className="space-y-1.5 text-[11px] leading-relaxed text-[#b9cae7]">
        <Bar label="HULL" value={player.hull} max={player.maxHull} color="#4ade80" />
        <Bar label="SHIELD" value={player.shield} max={player.maxShield} color="#38bdf8" />
        <Bar label="BEAM" value={player.beamCharge} max={1} color="#f59e0b" />
        <Bar label="WARP" value={player.warpCharge} max={1} color="#a78bfa" />

        <div className="mt-2 grid grid-cols-4 gap-x-3 gap-y-1 rounded-xl bg-[#0b1730]/75 px-3 py-2 text-[11px]">
          <Stat label="SPD" value={Math.round(player.speed)} />
          <Stat label="HDG" value={heading} />
          <Stat label="TORP" value={player.torpedoAmmo} />
          <Stat label="CR" value={player.credits} />
        </div>

        {alert === 'red' ? (
          <div className="rounded-xl border border-[#ff3b30] bg-[#4a1216] px-3 py-2 text-[11px] font-medium tracking-wide text-[#fecaca]">
            RED ALERT — shields charging
          </div>
        ) : (
          <div className="rounded-xl border border-emerald-500/35 bg-emerald-950/40 px-3 py-2 text-[11px] text-emerald-200">
            GREEN ALERT — shields bleeding down · V to sound red alert
          </div>
        )}

        {mode === 'skirmish' ? (
          <div className="rounded-xl border border-sky-400/20 bg-[#0c1a34]/80 px-3 py-2 text-[#c7dbff]">
            <b className="mr-1 text-[#7dd3fc]">SKIRMISH</b>
            — WAVE {skirmishWave}
          </div>
        ) : null}

        {mode === 'campaign' && objective ? (
          <div className="rounded-xl border border-sky-400/20 bg-[#0c1a34]/80 px-3 py-2 text-[#c7dbff]">
            <b className="mr-1 text-[#7dd3fc]">OBJECTIVE:</b>
            {objective.name} — {(objective.range / 1000).toFixed(1)} km
            {objective.offered ? ' — press ENTER to ACCEPT' : ''}
          </div>
        ) : null}

        {event ? (
          <div className="rounded-xl border border-amber-400/25 bg-amber-950/30 px-3 py-2 text-amber-100">
            <b className="mr-1 text-amber-300">EVENT:</b>
            {event.kind.toUpperCase()} — {Math.ceil(event.timeLeft)}s
          </div>
        ) : null}

        {contract ? (
          <div className="rounded-xl border border-violet-400/20 bg-violet-950/25 px-3 py-2 text-violet-100">
            {contract.text}
          </div>
        ) : null}

        {player.repairTarget ? (
          <div className="rounded-xl border border-rose-400/20 bg-rose-950/25 px-3 py-2 text-rose-100">
            <b className="mr-1 text-rose-300">DAMAGE:</b>
            {player.repairTarget.toUpperCase()} offline — R to weld ({player.repairWelds}/3)
          </div>
        ) : null}

        {target ? (
          <div
            className={`rounded-xl border px-3 py-2 ${
              target.inBeamArc
                ? 'border-emerald-400/30 bg-emerald-950/20 text-emerald-100'
                : 'border-slate-400/20 bg-slate-950/30 text-slate-200'
            }`}
          >
            <b className="mr-1 text-[#7dd3fc]">TARGET:</b>
            {target.name} — hull {Math.round(target.hull)} — {(target.range / 1000).toFixed(1)} km{' '}
            {target.inBeamArc ? '[IN ARC]' : '[NO SOLUTION]'}
          </div>
        ) : (
          <div className="rounded-xl border border-slate-400/20 bg-slate-950/30 px-3 py-2 text-slate-200">
            NO TARGET — press TAB
          </div>
        )}

        {player.docked ? (
          <div className="rounded-xl border border-sky-400/20 bg-sky-950/25 px-3 py-2 text-sky-100">
            DOCKED — repaired and resupplied
          </div>
        ) : null}

        {refusalVisible ? (
          <div className="rounded-xl border border-red-400/30 bg-red-950/70 px-3 py-2 text-[11px] font-medium text-red-100 shadow-[0_0_24px_rgba(127,29,29,0.4)]">
            {refusalText}
          </div>
        ) : null}
      </div>
    </div>
  );
}
