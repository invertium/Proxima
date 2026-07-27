// Bridge audio, synthesised in WebAudio.
//
// The Unreal build's six cues are .uasset files that can't be read from the browser,
// and asset work is deliberately out of scope for this pass. So the bus is real —
// master gain, per-cue mixing, engine hum that tracks throttle — and the sources are
// procedural stand-ins. Swapping in decoded samples later touches only `play()`.

import type { SimEvent } from '../sim/types';

type Cue = 'beam' | 'enemyFire' | 'hit' | 'explosion' | 'alarm' | 'dock';

const MIX: Record<
  Cue,
  { freq: number; decay: number; gain: number; type: OscillatorType; sweep: number }
> = {
  beam: { freq: 880, decay: 0.18, gain: 0.18, type: 'square', sweep: -420 },
  enemyFire: { freq: 320, decay: 0.2, gain: 0.14, type: 'sawtooth', sweep: -140 },
  hit: { freq: 180, decay: 0.14, gain: 0.22, type: 'triangle', sweep: -90 },
  explosion: { freq: 90, decay: 0.7, gain: 0.34, type: 'sawtooth', sweep: -70 },
  alarm: { freq: 660, decay: 0.5, gain: 0.16, type: 'sine', sweep: 220 },
  dock: { freq: 440, decay: 0.4, gain: 0.16, type: 'sine', sweep: 180 },
};

export class BridgeAudio {
  private ctx: AudioContext | null = null;
  private master: GainNode | null = null;
  private hum: { osc: OscillatorNode; gain: GainNode } | null = null;
  private alarmCooldown = 0;

  /**
   * Browsers refuse to start audio without a gesture, so the context is created on
   * the first real interaction rather than at load.
   */
  unlock(): void {
    if (this.ctx) return;

    this.ctx = new AudioContext();
    this.master = this.ctx.createGain();
    this.master.gain.value = 0.5;
    this.master.connect(this.ctx.destination);

    // Engine hum: a low drone whose gain follows the throttle.
    const osc = this.ctx.createOscillator();
    const gain = this.ctx.createGain();
    osc.type = 'sine';
    osc.frequency.value = 58;
    gain.gain.value = 0;
    osc.connect(gain).connect(this.master);
    osc.start();
    this.hum = { osc, gain };
  }

  setVolume(v: number): void {
    if (this.master) this.master.gain.value = Math.max(0, Math.min(1, v));
  }

  /** Engine note tracks speed, so the ship sounds like it's working. */
  setThrottle(fraction: number): void {
    if (!this.hum || !this.ctx) return;
    const f = Math.max(0, Math.min(1, fraction));
    this.hum.gain.gain.setTargetAtTime(f * 0.06, this.ctx.currentTime, 0.2);
    this.hum.osc.frequency.setTargetAtTime(48 + f * 46, this.ctx.currentTime, 0.3);
  }

  private play(cue: Cue): void {
    if (!this.ctx || !this.master) return;

    const { freq, decay, gain, type, sweep } = MIX[cue];
    const now = this.ctx.currentTime;

    const osc = this.ctx.createOscillator();
    const env = this.ctx.createGain();
    osc.type = type;
    osc.frequency.setValueAtTime(freq, now);
    osc.frequency.linearRampToValueAtTime(Math.max(40, freq + sweep), now + decay);

    env.gain.setValueAtTime(gain, now);
    env.gain.exponentialRampToValueAtTime(0.0001, now + decay);

    osc.connect(env).connect(this.master);
    osc.start(now);
    osc.stop(now + decay + 0.02);
  }

  /** One tick's events, plus the hull state that drives the alarm. */
  ingest(events: SimEvent[], hullCritical: boolean, dt: number): void {
    if (!this.ctx) return;

    for (const ev of events) {
      if (ev.t === 'beam') this.play(ev.friendly ? 'beam' : 'enemyFire');
      else if (ev.t === 'kill') this.play('explosion');
      else if (ev.t === 'hit') this.play('hit');
      else if (ev.t === 'dock') this.play('dock');
    }

    // The alarm repeats on a slow cycle while the hull is critical, rather than
    // firing once and leaving the crew unaware.
    this.alarmCooldown -= dt;
    if (hullCritical && this.alarmCooldown <= 0) {
      this.alarmCooldown = 1.6;
      this.play('alarm');
    }
  }
}
