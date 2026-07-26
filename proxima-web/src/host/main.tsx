import React from 'react';
import ReactDOM from 'react-dom/client';
import '../index.css';

import { BridgeAudio } from '../render/audio';
import { RelayHost } from '../net/transport';
import { gameStore } from '@/store/game';

import { clearCampaign, loadCampaign, saveCampaign } from './storage';
import { PIXEL_RATIO_CAP, keepAwake, loadSettings, saveSettings } from '../settings';
import { SessionRecorder } from './recorder';
import { App } from './App';

import type { ServerMessage, WorkerMessage } from '../net/protocol';
import type { SaveGame } from '../sim/save';
import type { Command } from '../sim/types';

export const worker = new Worker(new URL('./sim.worker.ts', import.meta.url), { type: 'module' });
export const audio = new BridgeAudio();
export const crew = new RelayHost();
export const settings = loadSettings();
export const recorder = new SessionRecorder(new URLSearchParams(location.search).has('record'));

audio.setVolume(settings.volume);
keepAwake(() => settings.wakeLock);

let latestSave: SaveGame | null = null;

const listeners = new Set<(msg: ServerMessage) => void>();

export const send = (msg: WorkerMessage): void => worker.postMessage(msg);

export const cmd = (command: Command, id?: number): void => {
  send({ m: 'cmd', cmd: command, id });
};

export const onServerMessage = (listener: (msg: ServerMessage) => void): (() => void) => {
  listeners.add(listener);
  return () => {
    listeners.delete(listener);
  };
};

export const getLatestSave = (): SaveGame | null => latestSave;

export const primeLatestSave = (save: SaveGame | null): void => {
  latestSave = save;
};

worker.onmessage = (ev: MessageEvent<ServerMessage>) => {
  const msg = ev.data;

  if (msg.m === 'save') {
    latestSave = msg.save;
    void saveCampaign(msg.save);
  }

  if (msg.m === 'state') {
    gameStore.getState().applySnapshot(msg.snapshot);
  }

  for (const listener of listeners) {
    listener(msg);
  }
};

const rootElement = document.getElementById('root');

if (!(rootElement instanceof HTMLDivElement)) {
  throw new Error('Missing #root mount point');
}

ReactDOM.createRoot(rootElement).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
);

export { clearCampaign, loadCampaign, PIXEL_RATIO_CAP, saveSettings };
