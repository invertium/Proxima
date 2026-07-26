import * as React from 'react';
import * as ReactDOM from 'react-dom/client';
import '../index.css';
import { gameStore } from '@/store/game';
import { RelayStation, sessionPin } from '../net/transport';
import type { Command } from '../sim/types';
import { StationApp } from './App';

const relay = new RelayStation();

export const send = (cmd: Command, id?: number): void => {
  if (id === undefined) {
    relay.sendCommand({ m: 'cmd', cmd });
    return;
  }

  relay.send({ m: 'cmd', cmd, id });
};

export const hostSessionPin = sessionPin;

relay.onSnapshot((snap) => {
  gameStore.getState().applySnapshot(snap);
});

const root = document.getElementById('root');
if (!(root instanceof HTMLDivElement)) {
  throw new Error('Station root container not found');
}

ReactDOM.createRoot(root).render(
  <React.StrictMode>
    <StationApp relay={relay} send={send} />
  </React.StrictMode>,
);
