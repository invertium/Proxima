import { Button } from '@/components/ui/button';
import type { Settings } from '@/settings';
import { SHIPS } from '@/sim/data';
import type { SaveGame } from '@/sim/save';
import type { Difficulty, PlayerShipType } from '@/sim/types';

export const DIFFICULTIES: readonly {
  readonly id: Difficulty;
  readonly name: string;
  readonly blurb: string;
}[] = [
  { id: 'ensign', name: 'ENSIGN', blurb: 'Softer hostiles — learn the bridge.' },
  { id: 'captain', name: 'CAPTAIN', blurb: 'The tuned baseline.' },
  { id: 'admiral', name: 'ADMIRAL', blurb: 'Harder-hitting, tougher hulls.' },
];

export const CONTROL_ROWS: readonly [string, string][] = [
  ['W / S', 'throttle'],
  ['A / D', 'steer'],
  ['Q / E', 'strafe (needs thrusters)'],
  ['TAB', 'cycle target'],
  ['SPACE', 'fire beam'],
  ['F', 'fire torpedo'],
  ['V', 'red alert'],
  ['R', 'repair weld'],
  ['G', 'dock / undock'],
  ['J', 'warp'],
  ['ENTER', 'accept orders'],
  ['ESC', 'pause'],
];

export const ACTION_BUTTON_CLASS = 'h-10 w-full uppercase tracking-[0.18em]';
export const OPTION_BUTTON_CLASS =
  'h-auto min-h-14 w-full flex-col items-start gap-1 border-[#1e3a5f] bg-[#0b1528] px-4 py-3 text-left text-slate-100 hover:border-[#2b547f] hover:bg-[#122038]';
export const SELECTED_BUTTON_CLASS = 'border-[#7dd3fc] bg-sky-950/60 text-sky-100';

const starterShips = SHIPS.filter((ship) => ship.cost === 0);

interface MainMenuSectionProps {
  readonly pin: string;
  readonly save: SaveGame | null;
  readonly onContinue: () => void;
  readonly onControls: () => void;
  readonly onNewGame: () => void;
  readonly onSettings: () => void;
  readonly onSkirmish: () => void;
}

export function MainMenuSection({
  pin,
  save,
  onContinue,
  onControls,
  onNewGame,
  onSettings,
  onSkirmish,
}: MainMenuSectionProps) {
  return (
    <>
      <h1 className="text-3xl font-medium tracking-[0.32em] text-[#7dd3fc]">PROXIMA</h1>
      <p className="text-sm uppercase tracking-[0.18em] text-slate-400">
        a co-op starship bridge simulator
      </p>
      {save ? (
        <Button className={ACTION_BUTTON_CLASS} data-testid="menu-continue" onClick={onContinue}>
          CONTINUE — OBJECTIVE {save.missionIndex + 1}
        </Button>
      ) : null}
      <Button className={ACTION_BUTTON_CLASS} data-testid="menu-newgame" onClick={onNewGame}>
        NEW GAME
      </Button>
      <Button className={ACTION_BUTTON_CLASS} data-testid="menu-skirmish" onClick={onSkirmish}>
        SKIRMISH
      </Button>
      <Button className={ACTION_BUTTON_CLASS} variant="outline" onClick={onSettings}>
        SETTINGS
      </Button>
      <Button className={ACTION_BUTTON_CLASS} variant="outline" onClick={onControls}>
        CONTROLS
      </Button>
      <p className="text-sm text-slate-400">
        Crew joins at <code>/station.html</code> on this machine&apos;s LAN address.
      </p>
      {pin ? (
        <p className="text-sm text-slate-400">
          Session PIN <code>{pin}</code> — or hand out <code>/station.html?pin={pin}</code>
        </p>
      ) : null}
    </>
  );
}

interface NewGameSectionProps {
  readonly difficulty: Difficulty;
  readonly shipType: PlayerShipType;
  readonly onBack: () => void;
  readonly onDifficulty: (value: Difficulty) => void;
  readonly onLaunch: () => void;
  readonly onShipType: (value: PlayerShipType) => void;
}

export function NewGameSection({
  difficulty,
  shipType,
  onBack,
  onDifficulty,
  onLaunch,
  onShipType,
}: NewGameSectionProps) {
  return (
    <>
      <h2 className="text-3xl font-medium tracking-[0.32em] text-[#7dd3fc]">NEW GAME</h2>
      <div className="grid gap-2 text-left">
        <p className="text-xs uppercase tracking-[0.28em] text-slate-400">Difficulty</p>
        <div className="grid gap-2 md:grid-cols-3">
          {DIFFICULTIES.map((option) => (
            <Button
              key={option.id}
              className={`${OPTION_BUTTON_CLASS} ${difficulty === option.id ? SELECTED_BUTTON_CLASS : ''}`}
              variant="outline"
              onClick={() => onDifficulty(option.id)}
            >
              <span className="text-sm font-semibold tracking-[0.18em]">{option.name}</span>
              <span className="text-xs text-slate-300">{option.blurb}</span>
            </Button>
          ))}
        </div>
      </div>
      <div className="grid gap-2 text-left">
        <p className="text-xs uppercase tracking-[0.28em] text-slate-400">Starting hull</p>
        <div className="grid gap-2 md:grid-cols-2">
          {starterShips.map((ship) => (
            <Button
              key={ship.type}
              className={`${OPTION_BUTTON_CLASS} ${shipType === ship.type ? SELECTED_BUTTON_CLASS : ''}`}
              variant="outline"
              onClick={() => onShipType(ship.type)}
            >
              <span className="text-sm font-semibold tracking-[0.18em]">
                {ship.name.toUpperCase()}
              </span>
              <span className="text-xs text-slate-300">{ship.blurb}</span>
            </Button>
          ))}
        </div>
      </div>
      <Button className={ACTION_BUTTON_CLASS} data-testid="menu-launch" onClick={onLaunch}>
        LAUNCH
      </Button>
      <Button className={ACTION_BUTTON_CLASS} variant="outline" onClick={onBack}>
        BACK
      </Button>
    </>
  );
}

interface PausedMenuSectionProps {
  readonly onControls: () => void;
  readonly onMainMenu: () => void;
  readonly onResume: () => void;
  readonly onSettings: () => void;
}

export function PausedMenuSection({
  onControls,
  onMainMenu,
  onResume,
  onSettings,
}: PausedMenuSectionProps) {
  return (
    <>
      <h2 className="text-3xl font-medium tracking-[0.32em] text-[#7dd3fc]">PAUSED</h2>
      <Button className={ACTION_BUTTON_CLASS} onClick={onResume}>
        RESUME
      </Button>
      <Button className={ACTION_BUTTON_CLASS} variant="outline" onClick={onSettings}>
        SETTINGS
      </Button>
      <Button className={ACTION_BUTTON_CLASS} variant="outline" onClick={onControls}>
        CONTROLS
      </Button>
      <Button
        className={ACTION_BUTTON_CLASS}
        data-testid="menu-main"
        variant="outline"
        onClick={onMainMenu}
      >
        MAIN MENU
      </Button>
      <p className="text-sm text-slate-400">Progress is saved automatically.</p>
    </>
  );
}

interface OutcomeMenuSectionProps {
  readonly outcome: 'victory' | 'defeat';
  readonly onMainMenu: () => void;
  readonly onRetry: () => void;
}

export function OutcomeMenuSection({ outcome, onMainMenu, onRetry }: OutcomeMenuSectionProps) {
  return (
    <>
      <h2
        className={`text-3xl font-medium tracking-[0.32em] ${outcome === 'victory' ? 'text-[#4ade80]' : 'text-[#f87171]'}`}
      >
        {outcome === 'victory' ? 'THE VEIL IS SECURE' : 'SHIP LOST'}
      </h2>
      <p className="text-sm text-slate-300">
        {outcome === 'victory'
          ? 'The Crimson Pact is finished in the Veil. Well flown, Captain.'
          : 'All hands lost. The sector falls quiet.'}
      </p>
      {outcome === 'defeat' ? (
        <Button className={ACTION_BUTTON_CLASS} data-testid="menu-retry" onClick={onRetry}>
          RETRY FROM LAST SAVE
        </Button>
      ) : null}
      <Button
        className={ACTION_BUTTON_CLASS}
        data-testid="menu-main"
        variant="outline"
        onClick={onMainMenu}
      >
        MAIN MENU
      </Button>
    </>
  );
}

interface SettingsMenuSectionProps {
  readonly settings: Settings;
  readonly onBack: () => void;
  readonly onQuality: () => void;
  readonly onVolume: () => void;
  readonly onWakeLock: () => void;
}

export function SettingsMenuSection({
  settings,
  onBack,
  onQuality,
  onVolume,
  onWakeLock,
}: SettingsMenuSectionProps) {
  return (
    <>
      <h2 className="text-3xl font-medium tracking-[0.32em] text-[#7dd3fc]">SETTINGS</h2>
      <Button className={OPTION_BUTTON_CLASS} variant="outline" onClick={onVolume}>
        <span className="text-sm font-semibold tracking-[0.18em]">MASTER VOLUME</span>
        <span className="text-xs text-slate-300">{Math.round(settings.volume * 100)}%</span>
      </Button>
      <Button className={OPTION_BUTTON_CLASS} variant="outline" onClick={onQuality}>
        <span className="text-sm font-semibold tracking-[0.18em]">RENDER QUALITY</span>
        <span className="text-xs text-slate-300">{settings.quality.toUpperCase()}</span>
      </Button>
      <Button className={OPTION_BUTTON_CLASS} variant="outline" onClick={onWakeLock}>
        <span className="text-sm font-semibold tracking-[0.18em]">KEEP SCREEN AWAKE</span>
        <span className="text-xs text-slate-300">{settings.wakeLock ? 'ON' : 'OFF'}</span>
      </Button>
      <Button className={ACTION_BUTTON_CLASS} variant="outline" onClick={onBack}>
        BACK
      </Button>
      <p className="text-sm text-slate-400">
        Crew consoles keep their own screen awake using this setting.
      </p>
    </>
  );
}

interface ControlsMenuSectionProps {
  readonly onBack: () => void;
}

export function ControlsMenuSection({ onBack }: ControlsMenuSectionProps) {
  return (
    <>
      <h2 className="text-3xl font-medium tracking-[0.32em] text-[#7dd3fc]">CONTROLS</h2>
      <div className="grid grid-cols-[auto_1fr] gap-x-4 gap-y-2 text-left text-sm text-slate-200">
        {CONTROL_ROWS.map(([keys, action]) => (
          <div key={keys} className="contents">
            <div className="font-semibold tracking-[0.18em] text-sky-200">{keys}</div>
            <div className="text-slate-300">{action}</div>
          </div>
        ))}
      </div>
      <Button className={ACTION_BUTTON_CLASS} variant="outline" onClick={onBack}>
        BACK
      </Button>
    </>
  );
}
