import { create } from 'zustand'

import { Button } from '@/components/ui/button'
import { Progress } from '@/components/ui/progress'
import { cn } from '@/lib/utils'
import { rankFromXp, SCAN_DURATION } from '@/sim/data'
import type { Snapshot } from '@/sim/types'
import { useGameStore } from '@/store/game'

import type { StationPanelProps } from '../StationPanelProps'

type ScopeMode = 'tactical' | 'map'
type Contact = Snapshot['contacts'][number]
type CommsEntry = Snapshot['comms'][number]

interface ScienceScopeStore {
  readonly mode: ScopeMode
  readonly toggleMode: () => void
  readonly resetMode: () => void
}

const EMPTY_CONTACTS: readonly Contact[] = []
const EMPTY_COMMS: readonly CommsEntry[] = []

const km = (n: number): string => `${(n / 1000).toFixed(1)} km`

const useScienceScopeStore = create<ScienceScopeStore>((set) => ({
  mode: 'tactical',
  toggleMode: () => set((state) => ({ mode: state.mode === 'map' ? 'tactical' : 'map' })),
  resetMode: () => set({ mode: 'tactical' }),
}))

function Readout({
  label,
  value,
  tone,
}: {
  readonly label: string
  readonly value: string
  readonly tone?: 'normal' | 'danger' | 'accent'
}) {
  return (
    <div className="rounded-lg border border-[#1b314d] bg-[#08111d] px-3 py-2">
      <div className="mb-1 text-[10px] tracking-[0.18em] text-[#6f88a9]">{label}</div>
      <div
        className={cn(
          'text-sm font-semibold text-[#dbe7ff]',
          tone === 'danger' && 'text-[#fda4af]',
          tone === 'accent' && 'text-[#7dd3fc]',
        )}
      >
        {value}
      </div>
    </div>
  )
}

export function SciencePanel({ send }: StationPanelProps) {
  const snapshot = useGameStore((state) => state.snapshot)
  const contacts = snapshot?.contacts ?? EMPTY_CONTACTS
  const event = snapshot?.event ?? null
  const objective = snapshot?.objective ?? null
  const offer = snapshot?.offer ?? null
  const contract = snapshot?.contract ?? null
  const player = snapshot?.player ?? null
  const comms = snapshot?.comms ?? EMPTY_COMMS
  const scopeMode = useScienceScopeStore((state) => state.mode)
  const toggleScopeMode = useScienceScopeStore((state) => state.toggleMode)

  if (player === null) {
    return (
      <section className="rounded-lg border border-[#1e3a5f] bg-[#06101c] p-3 text-xs text-[#9ab6da]">
        AWAITING TELEMETRY
      </section>
    )
  }

  const isScanning = player.scanning && player.scanTargetId !== null
  const scanPercent = Math.round(player.scanProgress * 100)
  const scannedIds = player.scanned
  const activeContact = contacts.find((contact) => contact.id === player.scanTargetId) ?? null
  const recentComms = comms.slice(-5)
  const rank = rankFromXp(player.xp)
  const objectiveText = objective
    ? `${objective.name} · ${km(objective.range)}${
        objective.offered ? ' — ORDERS PENDING' : objective.live ? ' — ENGAGED' : ''
      }`
    : '—'
  const objectiveTone = objective?.offered ? 'danger' : objective?.live ? 'accent' : 'normal'
  const contractText = contract
    ? `ACTIVE — ${contract.text}`
    : offer
      ? `ON OFFER — ${offer.text}`
      : player.docked
        ? 'No postings.'
        : 'Dock to see board'
  const showAcceptContract = offer !== null && contract === null

  return (
    <section className="space-y-3 rounded-lg border border-[#1e3a5f] bg-[#06101c] p-3 text-xs text-[#9ab6da]">
      {objective?.offered ? (
        <Button
          type="button"
          variant="destructive"
          className="w-full tracking-[0.2em]"
          onClick={() => send({ c: 'acceptObjective' })}
        >
          {objective ? `ACCEPT ORDERS — ${objective.name}` : 'ACCEPT ORDERS'}
        </Button>
      ) : null}

      <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
        <Readout label="OBJECTIVE" value={objectiveText} tone={objectiveTone} />
        <Readout
          label="EVENT"
          value={event ? `${event.kind.toUpperCase()} · ${Math.ceil(event.timeLeft)}s` : 'NO EVENT'}
          tone={event ? 'accent' : 'normal'}
        />
      </div>

      <div className="rounded-lg border border-[#1e3a5f] bg-[#08111d] p-3">
        <div className="mb-2 text-[11px] tracking-[0.18em] text-[#7dd3fc]">CONTRACT BOARD</div>
        <p className="text-xs leading-5 text-[#9ab6da]">{contractText}</p>
        {showAcceptContract ? (
          <div className="mt-3">
            <Button type="button" variant="destructive" onClick={() => send({ c: 'acceptContract' })}>
              ACCEPT CONTRACT
            </Button>
          </div>
        ) : null}
      </div>

      <div className="grid grid-cols-2 gap-2">
        <button
          type="button"
          aria-pressed={scopeMode === 'map'}
          onClick={toggleScopeMode}
          className={cn(
            'rounded-md border px-3 py-2 text-xs tracking-[0.18em] transition',
            scopeMode === 'map'
              ? 'border-[#7dd3fc] bg-[#102033] text-[#dbe7ff]'
              : 'border-[#1e3a5f] bg-[#08101c] text-[#8da6c8] hover:bg-[#0b1524] hover:text-[#dbe7ff]',
          )}
        >
          SECTOR MAP
        </button>
        <button
          type="button"
          onClick={() => send({ c: 'scan', id: null })}
          disabled={!isScanning}
          className={cn(
            'rounded-md border px-3 py-2 text-xs tracking-[0.18em] transition',
            isScanning
              ? 'border-[#7f1d1d] bg-[#2a0f17] text-[#fecaca] hover:bg-[#3c1320]'
              : 'cursor-not-allowed border-[#1f2937] bg-[#0b1220] text-[#4b5563]',
          )}
        >
          CANCEL SCAN
        </button>
      </div>

      {isScanning ? (
        <div className="rounded-lg border border-[#1b314d] bg-[#08111d] p-3">
          <div className="mb-1 flex items-center justify-between gap-2 text-[11px] tracking-[0.16em] text-[#7dd3fc]">
            <span>SCANNING {activeContact?.name ?? 'CONTACT'}</span>
            <span>{scanPercent}%</span>
          </div>
          <Progress value={scanPercent} className="mb-1" />
          <p className="text-[11px] text-[#7d9dc4]">Scanning — hold the lock for {SCAN_DURATION}s.</p>
        </div>
      ) : null}

      <div>
        <div className="mb-2 text-[11px] tracking-[0.18em] text-[#7dd3fc]">CONTACTS</div>
        {contacts.length === 0 ? (
          <p className="rounded-lg border border-dashed border-[#1b314d] bg-[#08111d] px-3 py-2 text-[#6f88a9]">
            No contacts.
          </p>
        ) : (
          <div className="space-y-2">
            {contacts.map((contact) => {
              const isResolved = scannedIds.includes(contact.id)
              const inRange = contact.range <= player.stats.scanRange
              return (
                <button
                  key={contact.id}
                  type="button"
                  onClick={() => send({ c: 'scan', id: contact.id })}
                  disabled={!inRange && !isResolved}
                  className={cn(
                    'contact flex w-full items-start justify-between gap-3 rounded-lg border px-3 py-2 text-left transition',
                    player.scanTargetId === contact.id && 'border-[#38bdf8] bg-[#102033]',
                    player.scanTargetId !== contact.id && 'border-[#1b314d] bg-[#08111d] hover:bg-[#0c1726]',
                    !inRange && !isResolved && 'cursor-not-allowed border-[#1f2937] text-[#4b5563] hover:bg-[#08111d]',
                  )}
                >
                  <div className="min-w-0 flex-1">
                    <div className="flex flex-wrap items-center gap-x-2 gap-y-1">
                      <b className="text-sm text-[#dbe7ff]">{contact.name}</b>
                      <span className="text-[11px] uppercase tracking-[0.14em] text-[#7d9dc4]">
                        {contact.className}
                      </span>
                    </div>
                    <div className="mt-1 text-[11px] text-[#7d9dc4]">
                      {km(contact.range)} · {inRange ? 'in sensor range' : 'out of range'}
                    </div>
                  </div>
                  <em
                    className={cn(
                      'shrink-0 text-[11px] not-italic tracking-[0.16em]',
                      isResolved ? 'text-[#86efac]' : 'text-[#fbbf24]',
                    )}
                  >
                    {isResolved ? 'RESOLVED' : 'unresolved'}
                  </em>
                </button>
              )
            })}
          </div>
        )}
      </div>

      <div className="grid grid-cols-1 gap-2 sm:grid-cols-3">
        <Readout
          label="SENSORS"
          value={`${km(player.stats.scanRange)}${player.damaged.sensors ? ' (DAMAGED)' : ''}`}
          tone={player.damaged.sensors ? 'danger' : 'normal'}
        />
        <Readout label="XP / RANK" value={`${player.xp} · rank ${rank}`} tone="accent" />
        <Readout label="SCAN" value={isScanning ? `${scanPercent}% LOCK` : 'IDLE'} tone={isScanning ? 'accent' : 'normal'} />
      </div>

      <div>
        <div className="mb-2 text-[11px] tracking-[0.18em] text-[#7dd3fc]">COMMS</div>
        {recentComms.length === 0 ? (
          <p className="rounded-lg border border-dashed border-[#1b314d] bg-[#08111d] px-3 py-2 text-[#6f88a9]">
            Channel quiet.
          </p>
        ) : (
          <div className="space-y-2">
            {recentComms.map((entry) => (
              <div key={`${entry.at}-${entry.sender}-${entry.text}`} className="rounded-lg border border-[#1b314d] bg-[#08111d] px-3 py-2">
                <div className="mb-1 flex items-center justify-between gap-2 text-[10px] tracking-[0.16em] text-[#6f88a9]">
                  <b className="font-semibold text-[#dbe7ff]">{entry.sender}</b>
                  <span>T+{entry.at.toFixed(1)}s</span>
                </div>
                <p className="text-[11px] leading-5 text-[#9ab6da]">{entry.text}</p>
              </div>
            ))}
          </div>
        )}
      </div>
    </section>
  )
}

export { useScienceScopeStore }
