// @vitest-environment jsdom

import { createElement } from 'react';
import { act, cleanup, render, screen } from '@testing-library/react';
import { afterEach, beforeEach, describe, expect, it } from 'vitest';

import { TICK_DT } from '../src/sim/data';
import { createWorld, snapshot, step } from '../src/sim/world';
import { EngineeringPanel } from '../src/stations/components/panels/EngineeringPanel';
import { HelmPanel, useHelmScopeStore } from '../src/stations/components/panels/HelmPanel';
import { SciencePanel, useScienceScopeStore } from '../src/stations/components/panels/SciencePanel';
import { gameStore } from '../src/store/game';

const resetStores = (): void => {
  gameStore.setState({ snapshot: null });
  useHelmScopeStore.getState().resetMode();
};

const applyWorldSnapshot = (world: ReturnType<typeof createWorld>): void => {
  gameStore.getState().applySnapshot(snapshot(world));
};

const applyPanelSnapshot = (): void => {
  const world = createWorld({ seed: 12 });
  const snap = snapshot(world);

  gameStore.getState().applySnapshot({
    ...snap,
    player: { ...snap.player, docked: true },
    objective: {
      name: 'Answer the Hail',
      pos: snap.player.pos,
      range: 1200,
      offered: true,
      live: false,
    },
    offer: {
      text: 'Survey the derelict and report back.',
      reward: 250,
    },
    contract: null,
  });
};

const requiredElement = (parent: ParentNode, selector: string): HTMLElement => {
  const element = parent.querySelector(selector);
  if (!(element instanceof HTMLElement)) {
    throw new Error(`missing element for selector: ${selector}`);
  }
  return element;
};

describe('Panel DOM stability', () => {
  beforeEach(() => {
    resetStores();
    useScienceScopeStore.getState().resetMode();
  });

  afterEach(() => {
    cleanup();
    resetStores();
    useScienceScopeStore.getState().resetMode();
  });

  it('keeps helm controls mounted across 300 snapshot updates', () => {
    const world = createWorld({ seed: 8 });
    world.missionIndex = 99;
    world.pending.push({ cmd: { c: 'throttle', v: 1 } });

    act(() => {
      applyWorldSnapshot(world);
    });

    const { container } = render(createElement(HelmPanel, { send: () => undefined }));

    const dockButton = screen.getByRole('button', { name: 'DOCK' });
    const fullButton = screen.getByRole('button', { name: 'FULL' });
    const throttleSlider = requiredElement(container, '[data-slot="slider"]');

    act(() => {
      for (let i = 0; i < 300; i += 1) {
        step(world, TICK_DT);
        applyWorldSnapshot(world);
      }
    });

    expect(screen.getByRole('button', { name: 'DOCK' })).toBe(dockButton);
    expect(screen.getByRole('button', { name: 'FULL' })).toBe(fullButton);
    expect(requiredElement(container, '[data-slot="slider"]')).toBe(throttleSlider);
  });

  it('shows orders and contract controls only on science', () => {
    act(() => {
      applyPanelSnapshot();
    });

    const helm = render(createElement(HelmPanel, { send: () => undefined }));
    expect(screen.queryByRole('button', { name: /accept orders/i })).toBeNull();
    expect(screen.queryByText('CONTRACT')).toBeNull();
    helm.unmount();

    const engineering = render(createElement(EngineeringPanel, { send: () => undefined }));
    expect(screen.queryByText('CONTRACT BOARD')).toBeNull();
    expect(screen.queryByRole('button', { name: /accept contract/i })).toBeNull();
    engineering.unmount();

    render(createElement(SciencePanel, { send: () => undefined }));
    expect(screen.getByRole('button', { name: /accept orders — answer the hail/i })).toBeTruthy();
    expect(screen.getByText('CONTRACT BOARD')).toBeTruthy();
    expect(screen.getByRole('button', { name: 'ACCEPT CONTRACT' })).toBeTruthy();
    expect(screen.getByText(/ORDERS PENDING/)).toBeTruthy();
  });

});
