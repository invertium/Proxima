import type { MutableRefObject } from 'react';
import { useCallback, useEffect, useRef, useState } from 'react';
import { gameStore, useGameStore } from '@/store/game';
import { sessionPin } from '../net/transport';
import { SectorView } from '../render/scene';
import type { Settings } from '../settings';
import { PIXEL_RATIO_CAP, saveSettings } from '../settings';
import type { SaveGame } from '../sim/save';
import type { Snapshot } from '../sim/types';
import type { MenuProps, MenuScreen, NewGameChoice } from './components/Menu';

import {
  audio,
  clearCampaign,
  cmd,
  crew,
  getLatestSave,
  loadCampaign,
  onServerMessage,
  primeLatestSave,
  recorder,
  send,
  settings,
} from './main';
import { usePilotLoop } from './usePilotLoop';

interface HostRuntime {
  readonly canvasRef: MutableRefObject<HTMLCanvasElement | null>;
  readonly error: Error | null;
  readonly evicted: boolean;
  readonly loading: boolean;
  readonly menuProps: MenuProps;
}

export function useHostRuntime(): HostRuntime {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const viewRef = useRef<SectorView | null>(null);
  const snapshotRef = useRef<Snapshot | null>(null);
  const crewCountRef = useRef(0);
  const menuOpenRef = useRef(true);
  const outcomeShownRef = useRef(false);
  const pinRef = useRef(sessionPin());

  const snapshot = useGameStore((state) => state.snapshot);

  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<Error | null>(null);
  const [menuOpen, setMenuOpen] = useState(true);
  const [menuScreen, setMenuScreen] = useState<MenuScreen>('main');
  const [menuSettings, setMenuSettings] = useState<Settings>({ ...settings });
  const [outcomeShown, setOutcomeShown] = useState(false);
  const [save, setSave] = useState<SaveGame | null>(getLatestSave());
  const [evicted, setEvicted] = useState(false);

  snapshotRef.current = snapshot;
  menuOpenRef.current = menuOpen;
  outcomeShownRef.current = outcomeShown;

  const updatePause = useCallback((): void => {
    send({
      m: 'pause',
      paused: menuOpenRef.current || (document.hidden && crewCountRef.current === 0),
    });
  }, []);

  const startGame = useCallback(
    (choice: NewGameChoice): void => {
      viewRef.current?.reset();
      gameStore.setState({ snapshot: null });
      snapshotRef.current = null;
      menuOpenRef.current = false;
      setMenuOpen(false);
      setMenuScreen('hidden');
      setOutcomeShown(false);
      send({
        m: 'boot',
        options: {
          seed: choice.save?.seed ?? Math.floor(Math.random() * 1e9),
          difficulty: choice.difficulty,
          shipType: choice.shipType,
          mode: choice.mode,
          save: choice.save ?? null,
        },
      });
      updatePause();
    },
    [updatePause],
  );

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || viewRef.current) return;

    const view = new SectorView(canvas);
    view.setQuality(PIXEL_RATIO_CAP[settings.quality]);
    viewRef.current = view;
  }, []);

  useEffect(() => {
    const view = viewRef.current;
    if (!view) return;

    let cancelled = false;
    const boot = async (): Promise<void> => {
      try {
        await view.load();
        const nextSave = await loadCampaign();
        primeLatestSave(nextSave);
        if (!cancelled) {
          setSave(nextSave);
          setLoading(false);
          setMenuOpen(true);
          setMenuScreen('main');
        }
      } catch (error0) {
        const nextError = error0 instanceof Error ? error0 : new Error(String(error0));
        if (!cancelled) {
          setError(nextError);
          setLoading(false);
        }
      }
    };

    void boot();
    return () => {
      cancelled = true;
    };
  }, []);

  useEffect(() => {
    const unsubscribe = onServerMessage((msg) => {
      if (msg.m === 'save') {
        primeLatestSave(msg.save);
        setSave(msg.save);
        return;
      }
      if (msg.m === 'ack') {
        crew.broadcast(msg);
        return;
      }
      if (msg.m !== 'state') return;

      recorder.observe(msg.snapshot, msg.events);
      viewRef.current?.pushSnapshot(msg.snapshot);
      viewRef.current?.ingest(msg.events);
      audio.ingest(msg.events, msg.snapshot.player.hullCritical, 1 / 60);
      crew.broadcast(msg);

      if (msg.snapshot.phase !== 'playing' && !outcomeShownRef.current) {
        setOutcomeShown(true);
        setMenuOpen(true);
        setMenuScreen('outcome');
        recorder.download();
        if (msg.snapshot.phase === 'victory') void clearCampaign();
      }
    });

    return unsubscribe;
  }, []);

  useEffect(() => {
    crew.onMessage((msg) => {
      if (msg.m === 'cmd') {
        cmd(msg.cmd, msg.id);
        return;
      }
      if (msg.m !== 'game') return;

      const save = getLatestSave();
      startGame({
        difficulty: save?.difficulty ?? 'captain',
        shipType: save?.shipType ?? 'interceptor',
        mode: 'campaign',
        save: msg.action === 'restart' ? save : null,
      });

      if (msg.action === 'new') {
        primeLatestSave(null);
        setSave(null);
        void clearCampaign();
      }
    });

    crew.onCrewCount((count) => {
      crewCountRef.current = count;
      updatePause();
    });

    crew.onEvicted(() => {
      send({ m: 'pause', paused: true });
      setEvicted(true);
    });
  }, [startGame, updatePause]);

  useEffect(() => {
    document.addEventListener('visibilitychange', updatePause);
    return () => {
      document.removeEventListener('visibilitychange', updatePause);
    };
  }, [updatePause]);

  // `menuOpen` is not "unnecessary" here, whatever the linter thinks: updatePause reads
  // menuOpenRef.current and is memoised with an empty dep list, so it never changes
  // identity. Drop `menuOpen` as suggested and opening the menu stops pausing the sim.
  // biome-ignore lint/correctness/useExhaustiveDependencies: menuOpen is the only trigger
  useEffect(() => {
    updatePause();
  }, [menuOpen, updatePause]);

  useEffect(() => {
    if (menuOpen && menuScreen === 'hidden' && snapshotRef.current && !outcomeShown) {
      setMenuScreen('paused');
      return;
    }
    if (!menuOpen && menuScreen === 'paused') {
      setMenuScreen('hidden');
    }
  }, [menuOpen, menuScreen, outcomeShown]);

  const handleResume = useCallback((): void => {
    setMenuOpen(false);
    setMenuScreen('hidden');
  }, []);

  const handleSettings = useCallback((next: Settings): void => {
    settings.volume = next.volume;
    settings.quality = next.quality;
    settings.wakeLock = next.wakeLock;
    setMenuSettings({ ...next });
    saveSettings(next);
    audio.setVolume(next.volume);
    viewRef.current?.setQuality(PIXEL_RATIO_CAP[next.quality]);
  }, []);

  usePilotLoop({ menuOpenRef, setMenuOpen, snapshotRef, viewRef });

  return {
    canvasRef,
    error,
    evicted,
    loading,
    menuProps: {
      isOpen: menuOpen,
      screen: menuScreen,
      onStart: startGame,
      onResume: handleResume,
      onSettings: handleSettings,
      settings: menuSettings,
      pin: pinRef.current,
      save,
      outcomeShown,
      outcome: snapshot?.phase === 'victory' ? 'victory' : 'defeat',
    },
  };
}
