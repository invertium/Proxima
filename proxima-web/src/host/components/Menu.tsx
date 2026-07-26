import { useEffect, useState } from 'react';

import type { SaveGame } from '@/sim/save';
import type { Settings } from '@/settings';
import type { Difficulty, GameMode, PlayerShipType } from '@/sim/types';

import {
  ControlsMenuSection,
  MainMenuSection,
  NewGameSection,
  OutcomeMenuSection,
  PausedMenuSection,
  SettingsMenuSection,
} from './MenuSections';

export interface NewGameChoice {
  readonly difficulty: Difficulty;
  readonly shipType: PlayerShipType;
  readonly mode: GameMode;
  readonly save?: SaveGame | null;
}

export type MenuScreen = 'hidden' | 'main' | 'paused' | 'outcome';

export interface MenuProps {
  readonly isOpen: boolean;
  readonly screen: MenuScreen;
  readonly onStart: (choice: NewGameChoice) => void;
  readonly onResume: () => void;
  readonly onSettings: (next: Settings) => void;
  readonly settings: Settings;
  readonly pin: string;
  readonly save: SaveGame | null;
  readonly outcomeShown: boolean;
  readonly outcome: 'victory' | 'defeat';
}

type VisibleScreen = 'main' | 'newgame' | 'paused' | 'outcome' | 'settings' | 'controls';

export function Menu({
  isOpen,
  screen,
  onStart,
  onResume,
  onSettings,
  settings,
  pin,
  save,
  outcomeShown,
  outcome,
}: MenuProps) {
  const [currentScreen, setCurrentScreen] = useState<VisibleScreen>('main');
  const [backScreen, setBackScreen] = useState<'main' | 'paused'>('main');
  const [difficulty, setDifficulty] = useState<Difficulty>('captain');
  const [shipType, setShipType] = useState<PlayerShipType>('interceptor');
  const [draftSettings, setDraftSettings] = useState<Settings>({ ...settings });

  useEffect(() => {
    if (screen === 'hidden') return;
    if (screen === 'outcome' && outcomeShown) {
      setCurrentScreen('outcome');
      return;
    }
    setCurrentScreen(screen);
    if (screen === 'main' || screen === 'paused') {
      setBackScreen(screen);
    }
  }, [outcomeShown, screen]);

  useEffect(() => {
    setDraftSettings({ ...settings });
  }, [settings]);

  if (!isOpen) {
    return null;
  }

  const cycleVolume = (): void => {
    const nextValue = Math.round(((draftSettings.volume + 0.25 > 1 ? 0 : draftSettings.volume + 0.25) * 100)) / 100;
    const next = { ...draftSettings, volume: nextValue };
    setDraftSettings(next);
    onSettings(next);
  };

  const cycleQuality = (): void => {
    const order: readonly Settings['quality'][] = ['low', 'medium', 'high'];
    const index = order.indexOf(draftSettings.quality);
    const next = { ...draftSettings, quality: order[(index + 1) % order.length] ?? 'high' };
    setDraftSettings(next);
    onSettings(next);
  };

  const toggleWakeLock = (): void => {
    const next = { ...draftSettings, wakeLock: !draftSettings.wakeLock };
    setDraftSettings(next);
    onSettings(next);
  };

  const launchCampaign = (): void => {
    onStart({ difficulty, shipType, mode: 'campaign' });
  };

  const continueCampaign = (): void => {
    if (!save) return;
    onStart({
      difficulty: save.difficulty,
      shipType: save.shipType,
      mode: 'campaign',
      save,
    });
  };

  const launchSkirmish = (): void => {
    onStart({ difficulty, shipType, mode: 'skirmish' });
  };

  const retryFromSave = (): void => {
    onStart({
      difficulty: save?.difficulty ?? difficulty,
      shipType: save?.shipType ?? shipType,
      mode: 'campaign',
      save,
    });
  };

  const openSubmenu = (next: 'settings' | 'controls'): void => {
    if (currentScreen === 'main' || currentScreen === 'paused') {
      setBackScreen(currentScreen);
    }
    setCurrentScreen(next);
  };

  const panelClass =
    currentScreen === 'newgame' || currentScreen === 'controls'
      ? 'grid gap-3 min-w-[520px] max-w-[92vw] p-7 bg-[#070e1c] border border-[#1e3a5f] rounded-xl text-center'
      : 'grid gap-3 min-w-[340px] max-w-[92vw] p-7 bg-[#070e1c] border border-[#1e3a5f] rounded-xl text-center';

  return (
    <div className="fixed inset-0 z-20 grid place-content-center bg-black/90 px-4">
      <div className={panelClass}>
        {currentScreen === 'main' ? (
          <MainMenuSection
            pin={pin}
            save={save}
            onContinue={continueCampaign}
            onControls={() => openSubmenu('controls')}
            onNewGame={() => setCurrentScreen('newgame')}
            onSettings={() => openSubmenu('settings')}
            onSkirmish={launchSkirmish}
          />
        ) : null}

        {currentScreen === 'newgame' ? (
          <NewGameSection
            difficulty={difficulty}
            shipType={shipType}
            onBack={() => setCurrentScreen('main')}
            onDifficulty={setDifficulty}
            onLaunch={launchCampaign}
            onShipType={setShipType}
          />
        ) : null}

        {currentScreen === 'paused' ? (
          <PausedMenuSection
            onControls={() => openSubmenu('controls')}
            onMainMenu={() => setCurrentScreen('main')}
            onResume={onResume}
            onSettings={() => openSubmenu('settings')}
          />
        ) : null}

        {currentScreen === 'outcome' ? (
          <OutcomeMenuSection outcome={outcome} onMainMenu={() => setCurrentScreen('main')} onRetry={retryFromSave} />
        ) : null}

        {currentScreen === 'settings' ? (
          <SettingsMenuSection
            settings={draftSettings}
            onBack={() => setCurrentScreen(backScreen)}
            onQuality={cycleQuality}
            onVolume={cycleVolume}
            onWakeLock={toggleWakeLock}
          />
        ) : null}

        {currentScreen === 'controls' ? (
          <ControlsMenuSection onBack={() => setCurrentScreen(backScreen)} />
        ) : null}
      </div>
    </div>
  );
}
