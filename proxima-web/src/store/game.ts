import { create } from 'zustand';
import { subscribeWithSelector } from 'zustand/middleware';
import type { Snapshot } from '../sim/types';

interface GameStore {
  snapshot: Snapshot | null;
  applySnapshot: (snap: Snapshot) => void;
}

export const gameStore = create<GameStore>()(
  subscribeWithSelector((set) => ({
    snapshot: null,
    applySnapshot: (snap) => set({ snapshot: snap }),
  })),
);

export const useGameStore = gameStore;
