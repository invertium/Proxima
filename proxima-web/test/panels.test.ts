// @vitest-environment jsdom
//
// The tests that should have existed. The pre-React console rebuilt panel.innerHTML
// ~10x/sec, which destroyed every button; browsers cancel a click whose mousedown
// target was removed before mouseup, so most real presses were dropped. Synthetic
// Playwright clicks press and release in under a millisecond and never spanned a
// repaint, so an 8/8 green E2E suite sat on top of a console no human could operate.
//
// React's reconciler makes node identity much more likely to survive — but "likely" is
// not "checked", and a stray `key={index}`, a conditional wrapper or a remount on a
// changed prop puts it straight back. These assert the invariant directly, per panel,
// in ~40 ms.

import { createElement, type ComponentType } from 'react';
import { act, cleanup, fireEvent, render, screen } from '@testing-library/react';
import { afterEach, beforeEach, describe, expect, it } from 'vitest';

import { TICK_DT } from '../src/sim/data';
import { createWorld, snapshot, step } from '../src/sim/world';
import type { Command, Snapshot } from '../src/sim/types';
import { EngineeringPanel } from '../src/stations/components/panels/EngineeringPanel';
import { HelmPanel, useHelmScopeStore } from '../src/stations/components/panels/HelmPanel';
import { SciencePanel, useScienceScopeStore } from '../src/stations/components/panels/SciencePanel';
import { WeaponsPanel } from '../src/stations/components/panels/WeaponsPanel';
import { gameStore } from '../src/store/game';

const resetStores = (): void => {
  gameStore.setState({ snapshot: null });
  useHelmScopeStore.getState().resetMode();
  useScienceScopeStore.getState().resetMode();
};

const apply = (snap: Snapshot): void => {
  gameStore.getState().applySnapshot(snap);
};

/** A real snapshot stream, advanced so the live values (speed, ranges, charge) move. */
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
  for (let i = 0; i < count; i += 1) {
    step(w, TICK_DT);
    out.push(snapshot(w));
  }
  return out;
};

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

const PANELS: Record<string, ComponentType<{ send: (cmd: Command) => void }>> = {
  helm: HelmPanel,
  weapons: WeaponsPanel,
  engineering: EngineeringPanel,
  science: SciencePanel,
};

const labelled = (): string[] =>
  screen.queryAllByRole('button').map((b) => b.textContent ?? '');

beforeEach(resetStores);
afterEach(() => {
  cleanup();
  resetStores();
});

describe.each(Object.entries(PANELS))('%s panel', (name, Panel) => {
  it('keeps every control attached across hundreds of state updates', () => {
    const frames = snapshots(300, { fight: true });
    act(() => apply(frames[0]!));

    const { container } = render(createElement(Panel, { send: () => undefined }));

    // Capture the controls as they exist after the first paint.
    const controls = [...container.querySelectorAll('button, input, [data-slot="slider-thumb"]')];
    expect(controls.length, `${name} rendered no controls`).toBeGreaterThan(0);

    act(() => {
      for (const s of frames) apply(s);
    });

    for (const control of controls) {
      expect(
        control.isConnected,
        `${name}: a control was detached during updates — a press spanning that update is dropped`,
      ).toBe(true);
    }
  });

  it('does not churn the control set while state ticks', () => {
    const frames = snapshots(120, { fight: true });
    act(() => apply(frames[0]!));

    const { container } = render(createElement(Panel, { send: () => undefined }));
    const before = container.querySelectorAll('button, input').length;

    act(() => {
      for (const s of frames) apply(s);
    });
    expect(container.querySelectorAll('button, input').length).toBe(before);
  });

  it('survives a snapshot with no contacts and one with a full fight', () => {
    act(() => apply(snapshots(1)[0]!));
    render(createElement(Panel, { send: () => undefined }));

    expect(() => {
      act(() => apply(snapshots(1, { fight: true })[0]!));
      act(() => apply(snapshots(1)[0]!));
    }).not.toThrow();
  });

  it('renders before any snapshot has arrived', () => {
    // A console opened before the host is up must say something, not throw.
    expect(() => render(createElement(Panel, { send: () => undefined }))).not.toThrow();
  });
});

describe('Science owns communications', () => {
  it('offers both acceptances on Science and neither anywhere else', () => {
    const s = hailed();
    expect(s.objective?.offered, 'fixture did not produce a pending hail').toBe(true);
    expect(s.offer, 'fixture did not produce a board posting').not.toBeNull();
    act(() => apply(s));

    const helm = render(createElement(HelmPanel, { send: () => undefined }));
    // A verb on two consoles is a verb no one owns.
    expect(labelled().some((t) => t.includes('ACCEPT'))).toBe(false);
    helm.unmount();

    const eng = render(createElement(EngineeringPanel, { send: () => undefined }));
    expect(labelled().some((t) => t.includes('ACCEPT'))).toBe(false);
    expect(screen.queryByText('CONTRACT BOARD')).toBeNull();
    eng.unmount();

    render(createElement(SciencePanel, { send: () => undefined }));
    expect(labelled().some((t) => t.startsWith('ACCEPT ORDERS'))).toBe(true);
    expect(labelled()).toContain('ACCEPT CONTRACT');
    expect(screen.getByText('CONTRACT BOARD')).toBeTruthy();
    expect(screen.getByText(/ORDERS PENDING/)).toBeTruthy();
  });

  it('sends both commands from Science', () => {
    const sent: Command[] = [];
    act(() => apply(hailed()));
    render(createElement(SciencePanel, { send: (c: Command) => sent.push(c) }));

    for (const b of screen.queryAllByRole('button')) {
      if (b.textContent?.startsWith('ACCEPT')) fireEvent.click(b);
    }

    expect(sent.map((c) => c.c)).toEqual(
      expect.arrayContaining(['acceptObjective', 'acceptContract']),
    );
  });
});

describe('press survives a concurrent update', () => {
  it('a click still fires when state updates between pointerdown and click', () => {
    const sent: Command[] = [];
    const frames = snapshots(40);
    act(() => apply(frames[0]!));
    render(createElement(HelmPanel, { send: (c: Command) => sent.push(c) }));

    const stop = screen.getByRole('button', { name: 'STOP' });

    // The exact sequence the pre-React console broke on.
    fireEvent.mouseDown(stop);
    act(() => {
      for (const s of frames) apply(s);
    });
    fireEvent.mouseUp(stop);
    fireEvent.click(stop);

    expect(stop.isConnected, 'the button under the finger was replaced mid-press').toBe(true);
    expect(sent.some((c) => c.c === 'throttle' && c.v === 0)).toBe(true);
  });
});

describe('hold controls', () => {
  it('centres the rudder when the pointer leaves rather than lifts', () => {
    // A finger sliding off the button never delivers pointerup to it. If only pointerup
    // stopped the hold, the ship would spin forever.
    const sent: Command[] = [];
    act(() => apply(snapshots(1)[0]!));
    render(createElement(HelmPanel, { send: (c: Command) => sent.push(c) }));

    const port = screen.getByRole('button', { name: 'PORT' });
    fireEvent.pointerDown(port);
    expect(sent.filter((c) => c.c === 'turn' && c.v === -1).length).toBe(1);

    fireEvent.pointerLeave(port);
    expect(sent.filter((c) => c.c === 'turn' && c.v === 0).length).toBe(1);
  });

  it('centres the rudder when the tab goes to the background', () => {
    // The console can lose the pointer for reasons the element never sees: the phone
    // locking, the browser taking the gesture, the tab being backgrounded. No event
    // reaches the button, so without a window-level release the rudder stays hard over.
    const sent: Command[] = [];
    act(() => apply(snapshots(1)[0]!));
    render(createElement(HelmPanel, { send: (c: Command) => sent.push(c) }));

    fireEvent.pointerDown(screen.getByRole('button', { name: 'STBD' }));
    expect(sent.filter((c) => c.c === 'turn' && c.v === 1).length).toBe(1);

    fireEvent(window, new Event('blur'));
    expect(sent.filter((c) => c.c === 'turn' && c.v === 0).length).toBe(1);
  });
});

describe('the throttle lever tells the truth', () => {
  const helmSnapshot = (throttle: number): Snapshot => {
    const snap = snapshots(1)[0]!;
    return { ...snap, player: { ...snap.player, throttle } };
  };

  const lever = (parent: ParentNode): HTMLInputElement => {
    const input = parent.querySelector('input[type="range"]');
    if (!(input instanceof HTMLInputElement)) throw new Error('no throttle lever rendered');
    return input;
  };

  it('reaches astern rather than bottoming out at all stop', () => {
    // `min={0}` made REVERSE_THROTTLE_MIN unreachable by drag, and the readout insisted
    // 0% while the ship made sternway. Both halves of that are checked here.
    act(() => apply(helmSnapshot(-0.35)));
    const { container } = render(createElement(HelmPanel, { send: () => undefined }));

    expect(lever(container).min).toBe('-0.35');
    expect(lever(container).getAttribute('aria-valuenow')).toBe('-0.35');
    expect(screen.getByTestId('throttle-readout').textContent).toBe('-35%');
  });

  it('reads full ahead as full ahead', () => {
    act(() => apply(helmSnapshot(1)));
    const { container } = render(createElement(HelmPanel, { send: () => undefined }));

    expect(lever(container).getAttribute('aria-valuenow')).toBe('1');
    expect(screen.getByTestId('throttle-readout').textContent).toBe('100%');
  });

  it('REV commands astern, not all stop', () => {
    const sent: Command[] = [];
    act(() => apply(helmSnapshot(0)));
    render(createElement(HelmPanel, { send: (c: Command) => sent.push(c) }));

    fireEvent.click(screen.getByRole('button', { name: 'REV' }));
    expect(sent).toEqual([{ c: 'throttle', v: -0.35 }]);
  });
});
