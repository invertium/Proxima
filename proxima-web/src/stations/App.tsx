import { useEffect, useState } from 'react';
import { useGameStore } from '@/store/game';
import { PROTOCOL_VERSION } from '../net/protocol';
import type { RelayStation } from '../net/transport';
import type { Command, Station } from '../sim/types';
import { type StationConnectionState, StationShell } from './components/StationShell';

interface StationAppProps {
  readonly relay: RelayStation;
  readonly send: (cmd: Command, id?: number) => void;
}

const stationFromHash = (hash: string): Station => {
  const station = hash.slice(1);

  switch (station) {
    case 'helm':
    case 'weapons':
    case 'engineering':
    case 'science':
      return station;
    default:
      return 'helm';
  }
};

const resolveConnectionState = (
  connected: boolean,
  pinRejected: boolean,
  snapshotExists: boolean,
): StationConnectionState => {
  if (pinRejected) return 'pin-required';
  if (!connected) return 'connecting';
  if (!snapshotExists) return 'waiting';
  return 'linked';
};

export function StationApp({ relay, send }: StationAppProps) {
  const snapshotExists = useGameStore((state) => state.snapshot !== null);
  const [connected, setConnected] = useState(false);
  const [pinRejected, setPinRejected] = useState(false);

  useEffect(() => {
    relay.onStatus((nextConnected) => {
      setConnected(nextConnected);
    });

    relay.onRejected(() => {
      sessionStorage.removeItem('proxima.pin');
      setPinRejected(true);
      setConnected(false);
    });

    const sendJoin = (): void => {
      relay.send({
        m: 'join',
        station: stationFromHash(location.hash),
        version: PROTOCOL_VERSION,
      });
    };

    sendJoin();
    window.addEventListener('hashchange', sendJoin);

    return () => {
      window.removeEventListener('hashchange', sendJoin);
    };
  }, [relay]);

  useEffect(() => {
    if (snapshotExists) setPinRejected(false);
  }, [snapshotExists]);

  const connectionState = resolveConnectionState(connected, pinRejected, snapshotExists);

  return <StationShell connectionState={connectionState} send={send} />;
}
