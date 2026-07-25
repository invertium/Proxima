// Interactive controls for the crew consoles.
//
// Pointer Events only — never touch* plus mouse* pairs, which double-fire on hybrid
// devices. Everything here is built to survive a real hand: a finger that slides off a
// button, a phone that locks mid-turn, two thumbs on two controls at once.

import { el } from './dom';

/** Every control currently held, so a blur or a locked screen can release them all. */
const heldReleases = new Map<number, () => void>();

const releaseAll = (): void => {
  for (const release of heldReleases.values()) release();
  heldReleases.clear();
};

window.addEventListener('blur', releaseAll);
window.addEventListener('pagehide', releaseAll);
document.addEventListener('visibilitychange', () => {
  if (document.hidden) releaseAll();
});

/**
 * A button that acts while held and stops on release — the rudder, the strafe
 * thrusters. Tap-to-set-and-tap-again is the wrong model for steering: you cannot
 * safely leave the helm hard over because someone's thumb slipped.
 *
 * Pointer capture is load-bearing. Without it, sliding a finger off PORT never
 * delivers pointerup to that element and the rudder stays over.
 */
export const holdButton = (
  label: string,
  onPress: () => void,
  onRelease: () => void,
  opts: { class?: string } = {},
): HTMLButtonElement => {
  const button = el('button', { text: label, class: `hold ${opts.class ?? ''}`.trim() });

  const release = (id?: number): void => {
    if (id !== undefined) heldReleases.delete(id);
    button.classList.remove('pressed');
    onRelease();
  };

  button.addEventListener('pointerdown', (e) => {
    if (button.disabled) return;
    e.preventDefault();
    button.setPointerCapture(e.pointerId);
    button.classList.add('pressed');
    heldReleases.set(e.pointerId, () => release());
    onPress();
  });

  for (const type of ['pointerup', 'pointercancel', 'lostpointercapture'] as const) {
    button.addEventListener(type, (e) => {
      if (!heldReleases.has(e.pointerId)) return;
      release(e.pointerId);
    });
  }

  return button;
};

/** A plain tap button. Identity is stable; only its label and disabled state change. */
export const tapButton = (label: string, onTap: () => void, opts: { class?: string } = {}): HTMLButtonElement => {
  const button = el('button', { text: label, class: opts.class ?? '' });
  button.addEventListener('click', () => {
    if (!button.disabled) onTap();
  });
  return button;
};

export interface Slider {
  root: HTMLElement;
  input: HTMLInputElement;
  /** True while a finger or mouse is on it — the caller must not write value then. */
  isDragging(): boolean;
  /** Writes the value only when idle, so a snapshot can't fight the operator's drag. */
  reflect(value: number): void;
}

/**
 * A continuous slider that sends at most once per frame while dragging, always sends a
 * trailing value so the resting position lands, and never has its value overwritten
 * mid-drag by an arriving snapshot.
 */
export const slider = (
  opts: { min: number; max: number; step: number; detent?: number },
  onChange: (value: number) => void,
): Slider => {
  const input = el('input', {
    attrs: {
      type: 'range',
      min: String(opts.min),
      max: String(opts.max),
      step: String(opts.step),
    },
  });

  let dragging = false;
  let pending: number | null = null;
  let frame = 0;

  const flush = (): void => {
    frame = 0;
    if (pending === null) return;
    onChange(pending);
    pending = null;
  };

  const queue = (value: number): void => {
    pending = value;
    if (!frame) frame = requestAnimationFrame(flush);
  };

  input.addEventListener('pointerdown', () => {
    dragging = true;
  });
  for (const type of ['pointerup', 'pointercancel'] as const) {
    input.addEventListener(type, () => {
      dragging = false;
      flush(); // the resting position must land even if the drag ended between frames
    });
  }

  input.addEventListener('input', () => {
    let value = Number(input.value);
    // Snap-to-zero: without a detent you cannot reliably neutralise a throttle.
    if (opts.detent && Math.abs(value) <= opts.detent) {
      value = 0;
      input.value = '0';
    }
    queue(value);
  });

  return {
    root: input,
    input,
    isDragging: () => dragging,
    reflect(value) {
      if (dragging) return;
      if (Math.abs(Number(input.value) - value) < opts.step) return;
      input.value = String(value);
    },
  };
};

export interface Sweep {
  root: HTMLElement;
  /** Advances the marker. `now` is the shared sim clock, so every console agrees. */
  tick(simTime: number): void;
  setEnabled(enabled: boolean): void;
  flash(credited: boolean): void;
}

/**
 * The repair sweep: a marker runs a triangle wave and the operator releases inside the
 * green band. Run locally so it feels instant, and driven off the SIM clock rather than
 * a local one, so every console's marker sits in the same place — a crew can call
 * "now" and mean it.
 */
export const sweepGauge = (
  opts: { period: number; greenMin: number; greenMax: number },
  onRelease: (phase: number) => void,
): Sweep => {
  const marker = el('i', { class: 'marker' });
  const green = el('i', { class: 'green' });
  green.style.left = `${opts.greenMin * 100}%`;
  green.style.width = `${(opts.greenMax - opts.greenMin) * 100}%`;

  const track = el('div', { class: 'sweep', children: [green, marker] });
  const button = el('button', { text: 'WELD', class: 'hold weld' });
  const root = el('div', { children: [track, button] });

  let phase = 0;
  let enabled = true;

  // Triangle rather than sawtooth: the marker sweeps back, so the green band is
  // approached from both sides and the timing reads naturally.
  const phaseAt = (t: number): number => {
    const x = (t % opts.period) / opts.period;
    return x < 0.5 ? x * 2 : 2 - x * 2;
  };

  button.addEventListener('pointerdown', (e) => {
    if (button.disabled) return;
    e.preventDefault();
    button.setPointerCapture(e.pointerId);
    button.classList.add('pressed');
  });
  for (const type of ['pointerup', 'pointercancel', 'lostpointercapture'] as const) {
    button.addEventListener(type, () => {
      if (!button.classList.contains('pressed')) return;
      button.classList.remove('pressed');
      onRelease(phase);
    });
  }

  return {
    root,
    tick(simTime) {
      if (!enabled) return;
      phase = phaseAt(simTime);
      marker.style.left = `${phase * 100}%`;
    },
    setEnabled(value) {
      enabled = value;
      button.disabled = !value;
    },
    flash(credited) {
      track.classList.remove('hit', 'miss');
      // Force a reflow so a repeat of the same class still animates.
      void track.offsetWidth;
      track.classList.add(credited ? 'hit' : 'miss');
    },
  };
};

export interface Toast {
  root: HTMLElement;
  show(text: string, kind?: 'info' | 'error'): void;
  tick(dt: number): void;
}

/** A short-lived status line. Announced politely so screen readers pick it up. */
export const toastStrip = (): Toast => {
  const root = el('div', { class: 'toast', attrs: { role: 'status', 'aria-live': 'polite' } });
  root.hidden = true;
  let left = 0;

  return {
    root,
    show(text, kind = 'info') {
      root.textContent = text;
      root.classList.toggle('bad', kind === 'error');
      root.hidden = false;
      left = 2.5;
    },
    tick(dt) {
      if (left <= 0) return;
      left -= dt;
      if (left <= 0) root.hidden = true;
    },
  };
};
