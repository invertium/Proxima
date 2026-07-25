// Front-end flow: main menu, new-game setup, pause, and the outcome screens.
//
// Replaces MainMenuWidget / PauseMenuWidget / SettingsMenuWidget / OutcomeMenuWidget /
// EndScreenWidget — five UMG classes and their C++ backing — with one overlay driven
// by the same Snapshot the HUD reads.

import { SHIPS } from '../sim/data';
import type { SaveGame } from '../sim/save';
import type { Difficulty, GameMode, PlayerShipType } from '../sim/types';

export interface NewGameChoice {
  difficulty: Difficulty;
  shipType: PlayerShipType;
  mode: GameMode;
  save?: SaveGame | null;
}

type Screen = 'main' | 'newgame' | 'paused' | 'outcome' | 'none';

const DIFFICULTIES: { id: Difficulty; name: string; blurb: string }[] = [
  { id: 'ensign', name: 'ENSIGN', blurb: 'Softer hostiles — learn the bridge.' },
  { id: 'captain', name: 'CAPTAIN', blurb: 'The tuned baseline.' },
  { id: 'admiral', name: 'ADMIRAL', blurb: 'Harder-hitting, tougher hulls.' },
];

export class Menu {
  private screen: Screen = 'main';
  private difficulty: Difficulty = 'captain';
  private shipType: PlayerShipType = 'interceptor';
  private save: SaveGame | null = null;
  private outcome: 'victory' | 'defeat' = 'defeat';

  constructor(
    private readonly root: HTMLElement,
    private readonly onStart: (choice: NewGameChoice) => void,
    private readonly onResume: () => void,
  ) {
    root.addEventListener('click', (e) => this.handleClick(e));
  }

  /** The menu owns whether the sim should be running. */
  get isOpen(): boolean {
    return this.screen !== 'none';
  }

  setSave(save: SaveGame | null): void {
    this.save = save;
    if (this.screen === 'main') this.render();
  }

  showMain(): void {
    this.screen = 'main';
    this.render();
  }

  showOutcome(kind: 'victory' | 'defeat'): void {
    if (this.screen === 'outcome') return;
    this.outcome = kind;
    this.screen = 'outcome';
    this.render();
  }

  togglePause(): void {
    if (this.screen === 'none') {
      this.screen = 'paused';
      this.render();
    } else if (this.screen === 'paused') {
      this.close();
    }
  }

  close(): void {
    this.screen = 'none';
    this.root.hidden = true;
    this.onResume();
  }

  private handleClick(e: Event): void {
    const el = (e.target as HTMLElement).closest('[data-action]') as HTMLElement | null;
    if (!el) return;

    const [action, value] = (el.dataset.action ?? '').split(':');
    switch (action) {
      case 'newgame':
        this.screen = 'newgame';
        break;
      case 'difficulty':
        this.difficulty = value as Difficulty;
        break;
      case 'hull':
        this.shipType = value as PlayerShipType;
        break;
      case 'launch':
        this.screen = 'none';
        this.root.hidden = true;
        this.onStart({ difficulty: this.difficulty, shipType: this.shipType, mode: 'campaign' });
        return;
      case 'continue':
        this.screen = 'none';
        this.root.hidden = true;
        this.onStart({
          difficulty: this.save?.difficulty ?? this.difficulty,
          shipType: this.save?.shipType ?? this.shipType,
          mode: 'campaign',
          save: this.save,
        });
        return;
      case 'skirmish':
        this.screen = 'none';
        this.root.hidden = true;
        this.onStart({ difficulty: this.difficulty, shipType: this.shipType, mode: 'skirmish' });
        return;
      case 'resume':
        this.close();
        return;
      case 'menu':
        this.screen = 'main';
        break;
      case 'retry':
        // Retry replays the campaign from the last save, which is exactly where the
        // C++ build put the player: the run is lost, the progression isn't.
        this.screen = 'none';
        this.root.hidden = true;
        this.onStart({
          difficulty: this.save?.difficulty ?? this.difficulty,
          shipType: this.save?.shipType ?? this.shipType,
          mode: 'campaign',
          save: this.save,
        });
        return;
    }
    this.render();
  }

  private render(): void {
    this.root.hidden = false;
    this.root.innerHTML = this.markup();
  }

  private markup(): string {
    if (this.screen === 'main') {
      return `
        <div class="panel">
          <h1>PROXIMA</h1>
          <p class="sub">a co-op starship bridge simulator</p>
          ${this.save ? `<button data-action="continue" class="primary">CONTINUE — objective ${this.save.missionIndex + 1}</button>` : ''}
          <button data-action="newgame">NEW GAME</button>
          <button data-action="skirmish">SKIRMISH</button>
          <p class="hint">Crew joins at <code>/station.html</code> on this machine's LAN address.</p>
        </div>`;
    }

    if (this.screen === 'newgame') {
      const diffs = DIFFICULTIES.map(
        (d) => `<button data-action="difficulty:${d.id}" class="${d.id === this.difficulty ? 'sel' : ''}">
          <b>${d.name}</b><span>${d.blurb}</span>
        </button>`,
      ).join('');

      // Only starter hulls are available at the start; the rest are bought at the drydock.
      const hulls = SHIPS.filter((s) => s.cost === 0)
        .map(
          (s) => `<button data-action="hull:${s.type}" class="${s.type === this.shipType ? 'sel' : ''}">
            <b>${s.name.toUpperCase()}</b><span>${s.blurb}</span>
          </button>`,
        )
        .join('');

      return `
        <div class="panel wide">
          <h2>NEW GAME</h2>
          <h3>DIFFICULTY</h3>
          <div class="choices">${diffs}</div>
          <h3>STARTING HULL</h3>
          <div class="choices">${hulls}</div>
          <button data-action="launch" class="primary">LAUNCH</button>
          <button data-action="menu">BACK</button>
        </div>`;
    }

    if (this.screen === 'paused') {
      return `
        <div class="panel">
          <h2>PAUSED</h2>
          <button data-action="resume" class="primary">RESUME</button>
          <button data-action="menu">MAIN MENU</button>
          <p class="hint">Progress is saved automatically.</p>
        </div>`;
    }

    if (this.screen === 'outcome') {
      const won = this.outcome === 'victory';
      return `
        <div class="panel">
          <h1 class="${won ? 'win' : 'lose'}">${won ? 'THE VEIL IS SECURE' : 'SHIP LOST'}</h1>
          <p class="sub">${won ? 'The Crimson Pact is finished in the Veil. Well flown, Captain.' : 'All hands lost. The sector falls quiet.'}</p>
          ${won ? '' : '<button data-action="retry" class="primary">RETRY FROM LAST SAVE</button>'}
          <button data-action="menu">MAIN MENU</button>
        </div>`;
    }

    return '';
  }
}
