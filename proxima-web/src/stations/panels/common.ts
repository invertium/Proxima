// Controls that belong to no single station: the session footer.
//
// The C++ build put the game phase and a restart button on EVERY console. Without it,
// a ship dying leaves four crew phones frozen on a dead snapshot with only the pilot
// able to do anything, which breaks the co-op loop hard.

import { el, setFlag, setHidden, setText } from '../ui/dom';
import { tapButton } from '../ui/controls';
import type { Snapshot } from '../../sim/types';

export interface Footer {
  root: HTMLElement;
  update(s: Snapshot): void;
}

export const createFooter = (onGame: (action: 'restart' | 'new') => void): Footer => {
  const phase = el('div', { class: 'phase' });
  const restart = tapButton('RETRY FROM LAST SAVE', () => onGame('restart'), { class: 'alert' });
  const fresh = tapButton('NEW CAMPAIGN', () => onGame('new'));
  const actions = el('div', { class: 'grid two', children: [restart, fresh] });

  const root = el('div', { class: 'footer', children: [phase, actions] });

  return {
    root,
    update(s: Snapshot) {
      const over = s.phase !== 'playing';

      let text: string;
      if (s.phase === 'victory') text = 'THE VEIL IS SECURE';
      else if (s.phase === 'defeat') text = 'SHIP LOST';
      else if (s.mode === 'skirmish') text = `SKIRMISH — WAVE ${s.skirmishWave}`;
      else if (s.objective?.live) text = 'ENCOUNTER LIVE';
      else if (s.objective?.offered) text = `AWAITING ORDERS — ${s.objective.name}`;
      else if (s.objective) text = `EN ROUTE — ${s.objective.name}`;
      else text = 'SECTOR CLEAR';

      setText(phase, text);
      setFlag(phase, 'bad', s.phase === 'defeat');
      setFlag(phase, 'win', s.phase === 'victory');
      // Restart is only offered once the run is actually over; mid-flight it would
      // just be a way for one crew member to throw the game away.
      setHidden(actions, !over);
    },
  };
};
