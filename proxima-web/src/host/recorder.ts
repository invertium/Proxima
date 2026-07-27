// Session telemetry, for offline analysis of a played game.
//
// Ported in spirit from SessionRecorderSubsystem: the gameplay code does not know it
// is being watched. This subscribes to the same state stream the crew broadcast
// already produces, samples it, and derives discrete events by diffing successive
// samples — so it needs no hooks in src/sim at all.
//
// Off by default; enable with ?record=1 and download at game over.

import type { SimEvent, Snapshot } from '../sim/types';

/** 2 Hz. Enough to reconstruct a flight path; small enough to keep in memory. */
const SAMPLE_INTERVAL = 0.5;

interface Sample {
  t: number;
  x: number;
  z: number;
  heading: number;
  speed: number;
  hull: number;
  shield: number;
  alert: string;
  contacts: number;
  mission: number;
  credits: number;
}

export class SessionRecorder {
  private readonly lines: string[] = [];
  private nextSampleAt = 0;
  private previous: Snapshot | null = null;

  constructor(readonly enabled: boolean) {
    if (enabled) this.write({ kind: 'start', at: new Date().toISOString() });
  }

  private write(record: object): void {
    this.lines.push(JSON.stringify(record));
  }

  /** Called for every state message. Sampling and event derivation both happen here. */
  observe(snap: Snapshot, events: SimEvent[]): void {
    if (!this.enabled) return;

    for (const e of events) {
      // Only the discrete beats worth analysing later; beams at 60Hz are noise.
      if (
        e.t === 'kill' ||
        e.t === 'dock' ||
        e.t === 'warp' ||
        e.t === 'detonate' ||
        e.t === 'alert'
      ) {
        this.write({ kind: e.t, t: round(snap.time), ...stripVectors(e) });
      }
    }

    // Mission and phase transitions are derived by diffing, not by a sim hook.
    if (this.previous) {
      if (this.previous.phase !== snap.phase) {
        this.write({
          kind: 'phase',
          t: round(snap.time),
          from: this.previous.phase,
          to: snap.phase,
        });
      }
      const was = this.previous.objective?.name;
      const now = snap.objective?.name;
      if (was !== now) this.write({ kind: 'objective', t: round(snap.time), from: was, to: now });
    }
    this.previous = snap;

    if (snap.time < this.nextSampleAt) return;
    this.nextSampleAt = snap.time + SAMPLE_INTERVAL;

    const sample: Sample = {
      t: round(snap.time),
      x: round(snap.player.pos.x),
      z: round(snap.player.pos.z),
      heading: round(snap.player.heading, 3),
      speed: round(snap.player.speed),
      hull: round(snap.player.hull),
      shield: round(snap.player.shield),
      alert: snap.alert,
      contacts: snap.contacts.length,
      mission: snap.player.credits,
      credits: snap.player.credits,
    };
    this.write({ kind: 'sample', ...sample });
  }

  /** Newline-delimited JSON, the same shape the C++ recorder wrote. */
  toJsonl(): string {
    return this.lines.join('\n');
  }

  /** Offers the log as a download. Called at game over. */
  download(): void {
    if (!this.enabled || this.lines.length === 0) return;

    const blob = new Blob([this.toJsonl()], { type: 'application/x-ndjson' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `proxima-session-${Date.now()}.jsonl`;
    a.click();
    URL.revokeObjectURL(url);
  }
}

const round = (v: number, dp = 0): number => Number(v.toFixed(dp));

/** Flattens the Vec3s in an event so each line stays one flat JSON object. */
const stripVectors = (e: SimEvent): Record<string, unknown> => {
  const out: Record<string, unknown> = {};
  for (const [k, v] of Object.entries(e)) {
    if (k === 't') continue;
    if (v && typeof v === 'object' && 'x' in v) {
      out[`${k}X`] = round((v as { x: number }).x);
      out[`${k}Z`] = round((v as { z: number }).z);
    } else {
      out[k] = v;
    }
  }
  return out;
};
