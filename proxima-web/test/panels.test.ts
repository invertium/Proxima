// @vitest-environment jsdom
//
// The test that should have existed. The old console rebuilt panel.innerHTML ~10x/sec,
// which destroyed every button; browsers cancel a click whose mousedown target was
// removed before mouseup, so most real presses were dropped. Synthetic Playwright
// clicks press and release in under a millisecond and never spanned a repaint, so an
// 8/8 green E2E suite sat on top of a console no human could operate.
//
// These assert the invariant directly and in ~40ms: a control's DOM node survives an
// arbitrary number of state updates.

import { beforeEach, describe, expect, it, vi } from 'vitest';
import { createHelmPanel } from '../src/stations/panels/helm';
import { createWeaponsPanel } from '../src/stations/panels/weapons';
import { createEngineeringPanel } from '../src/stations/panels/engineering';
import { createSciencePanel } from '../src/stations/panels/science';
import { createWorld, snapshot, step } from '../src/sim/world';
import { TICK_DT } from '../src/sim/data';
import type { Command, Snapshot } from '../src/sim/types';

/** A real snapshot, advanced so the live values (speed, ranges, charge) actually move. */
const snapshots = (count: number, opts: { fight?: boolean } = {}): Snapshot[] => {
  const w = createWorld({ seed: 4 });
  if (opts.fight) {
    w.player.pos = { ...w.landmarks[0]!.pos };
    step(w, TICK_DT);
    w.pending.push({ cmd: { c: 'acceptObjective' } });
    step(w, TICK_DT);
  }
  w.pending.push({ cmd: { c: 'throttle', v: 1 } });

  const out: Snapshot[] = [];
  for (let i = 0; i < count; i++) {
    step(w, TICK_DT);
    out.push(snapshot(w));
  }
  return out;
};

const PANELS = {
  helm: (send: (c: Command) => void) => createHelmPanel(send, () => {}),
  weapons: (send: (c: Command) => void) => createWeaponsPanel(send),
  engineering: (send: (c: Command) => void) => createEngineeringPanel(send),
  science: (send: (c: Command) => void) => createSciencePanel(send, () => {}),
};

describe.each(Object.entries(PANELS))('%s panel', (name, create) => {
  beforeEach(() => {
    document.body.innerHTML = '';
  });

  it('keeps every control attached across hundreds of state updates', () => {
    const panel = create(() => {});
    document.body.appendChild(panel.root);

    const frames = snapshots(300, { fight: true });
    panel.update(frames[0]!);

    // Capture the controls as they exist after the first paint.
    const controls = [...panel.root.querySelectorAll('button, input')];
    expect(controls.length, `${name} rendered no controls`).toBeGreaterThan(0);

    for (const s of frames) panel.update(s);

    for (const control of controls) {
      expect(
        control.isConnected,
        `${name}: a control was detached during updates — a press spanning that update is dropped`,
      ).toBe(true);
    }
  });

  it('does not churn the control set while state ticks', () => {
    const panel = create(() => {});
    document.body.appendChild(panel.root);

    const frames = snapshots(120, { fight: true });
    panel.update(frames[0]!);
    const before = panel.root.querySelectorAll('button, input').length;

    for (const s of frames) panel.update(s);
    expect(panel.root.querySelectorAll('button, input').length).toBe(before);
  });

  it('survives a snapshot with no contacts and one with a full fight', () => {
    const panel = create(() => {});
    document.body.appendChild(panel.root);

    expect(() => {
      panel.update(snapshots(1)[0]!);
      panel.update(snapshots(1, { fight: true })[0]!);
      panel.update(snapshots(1)[0]!);
    }).not.toThrow();
  });
});

describe('Science owns communications', () => {
  const buttons = (panel: { root: HTMLElement }): string[] =>
    [...panel.root.querySelectorAll('button')].map((b) => b.textContent ?? '');

  /** A snapshot with the fleet hailing and a board posting up, without answering either. */
  const hailed = (): Snapshot => {
    const w = createWorld({ seed: 4 });
    const base = w.landmarks.find((l) => l.dockable)!;
    w.player.pos = { ...base.pos };
    w.player.speed = 0;
    w.pending.push({ cmd: { c: 'dock' } });
    step(w, TICK_DT);
    // Stand on the objective so the director hails — but never accept.
    w.player.pos = { ...w.landmarks[0]!.pos };
    step(w, TICK_DT);
    return snapshot(w);
  };

  it('offers both acceptances on Science and neither anywhere else', () => {
    const s = hailed();
    expect(s.objective?.offered, 'fixture did not produce a pending hail').toBe(true);
    expect(s.offer, 'fixture did not produce a board posting').not.toBeNull();

    const sci = createSciencePanel(() => {}, () => {});
    const helm = createHelmPanel(() => {}, () => {});
    const eng = createEngineeringPanel(() => {});
    for (const p of [sci, helm, eng]) {
      document.body.appendChild(p.root);
      p.update(s);
    }

    expect(buttons(sci).some((t) => t.startsWith('ACCEPT ORDERS'))).toBe(true);
    expect(buttons(sci)).toContain('ACCEPT CONTRACT');
    // A verb on two consoles is a verb no one owns.
    expect(buttons(helm).some((t) => t.includes('ACCEPT'))).toBe(false);
    expect(buttons(eng).some((t) => t.includes('ACCEPT'))).toBe(false);
  });

  it('sends both commands from Science', () => {
    const sent: Command[] = [];
    const sci = createSciencePanel((c) => sent.push(c), () => {});
    document.body.appendChild(sci.root);
    sci.update(hailed());

    for (const b of sci.root.querySelectorAll('button')) {
      if (b.textContent?.startsWith('ACCEPT')) b.click();
    }
    expect(sent.map((c) => c.c)).toEqual(
      expect.arrayContaining(['acceptObjective', 'acceptContract']),
    );
  });
});

describe('press survives a concurrent update', () => {
  it('a click still fires when state updates between pointerdown and click', () => {
    const sent: Command[] = [];
    const panel = createHelmPanel((c) => sent.push(c), () => {});
    document.body.appendChild(panel.root);

    const frames = snapshots(40);
    panel.update(frames[0]!);

    const stop = [...panel.root.querySelectorAll('button')].find((b) => b.textContent === 'STOP')!;
    expect(stop).toBeDefined();

    // The exact sequence the old console broke on.
    stop.dispatchEvent(new MouseEvent('mousedown', { bubbles: true }));
    for (const s of frames) panel.update(s);
    stop.dispatchEvent(new MouseEvent('mouseup', { bubbles: true }));
    stop.click();

    expect(sent.some((c) => c.c === 'throttle' && c.v === 0)).toBe(true);
  });
});

describe('hold controls', () => {
  it('release fires even when the pointer is lost rather than lifted', () => {
    const sent: Command[] = [];
    const panel = createHelmPanel((c) => sent.push(c), () => {});
    document.body.appendChild(panel.root);
    panel.update(snapshots(1)[0]!);

    const port = [...panel.root.querySelectorAll('button')].find((b) => b.textContent?.includes('PORT'))!;
    // jsdom has no pointer capture; stub it so the handler path is exercised.
    port.setPointerCapture = vi.fn();

    port.dispatchEvent(new Event('pointerdown', { bubbles: true }) as PointerEvent);
    expect(sent.filter((c) => c.c === 'turn' && c.v === -1).length).toBe(1);

    // A finger sliding off the button loses capture instead of firing pointerup. The
    // rudder must still centre, or the ship spins forever.
    port.dispatchEvent(new Event('lostpointercapture', { bubbles: true }) as PointerEvent);
    expect(sent.filter((c) => c.c === 'turn' && c.v === 0).length).toBe(1);
  });
});
