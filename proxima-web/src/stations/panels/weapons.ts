// Weapons: pick a target, read the solution, shoot.

import { el, setDisabled, setFlag, setHidden, setText, syncList } from '../ui/dom';
import { tapButton } from '../ui/controls';
import { km, type Panel, type Send } from './panel';
import type { Snapshot } from '../../sim/types';

type Contact = Snapshot['contacts'][number];

export const createWeaponsPanel = (send: Send): Panel => {
  const list = el('div', { class: 'contacts' });
  const empty = el('p', { class: 'muted', text: 'No contacts.' });
  const rows = new Map<string, HTMLElement>();

  const beam = tapButton('FIRE BEAM', () => send({ c: 'fireBeam' }), { class: 'fire' });
  const torpedo = tapButton('TORPEDO', () => send({ c: 'fireTorpedo' }), { class: 'fire' });

  const dmgOut = el('dd');
  const arcOut = el('dd');
  const turretRow = el('div', { class: 'dl-row' });
  const turretOut = el('dd');
  const damagedNote = el('p', { class: 'muted bad', text: 'WEAPONS DAMAGED — beam recharges at half rate.' });

  turretRow.append(el('dt', { text: 'TURRET' }), turretOut);

  const root = el('div', {
    children: [
      empty,
      list,
      el('div', { class: 'grid two', children: [beam, torpedo] }),
      damagedNote,
      el('dl', {
        children: [el('dt', { text: 'BEAM DMG' }), dmgOut, el('dt', { text: 'BEAM ARC' }), arcOut],
      }),
      turretRow,
    ],
  });

  const createRow = (c: Contact): HTMLElement => {
    const name = el('b');
    const detail = el('span');
    const solution = el('em');
    const row = el('button', { class: 'contact', children: [name, detail, solution] });
    // The listener is attached once, to a node that outlives every repaint.
    row.addEventListener('click', () => send({ c: 'target', id: Number(row.dataset['id']) }));
    return row;
  };

  const updateRow = (row: HTMLElement, c: Contact, s: Snapshot): void => {
    row.dataset['id'] = String(c.id);
    setFlag(row, 'sel', c.id === s.player.targetId);

    const [name, detail, solution] = row.children as unknown as HTMLElement[];
    setText(name!, `${c.name} · ${c.className}`);
    setText(
      detail!,
      c.scanned
        ? `${km(c.range)} · hull ${Math.round(c.hull)}/${Math.round(c.maxHull)} · shield ${Math.round(c.shield)}`
        : `${km(c.range)} · unscanned — Science can resolve it`,
    );
    setText(
      solution!,
      c.inBeamArc ? 'BEAM SOLUTION' : c.inTorpedoArc ? 'TORPEDO ARC ONLY' : 'NO SOLUTION',
    );
    setFlag(solution!, 'ok', c.inBeamArc);
  };

  return {
    root,
    update(s: Snapshot) {
      const p = s.player;
      setHidden(empty, s.contacts.length > 0);
      syncList(list, s.contacts, (c) => String(c.id), createRow, (node, c) => updateRow(node, c, s), rows);

      const target = s.contacts.find((c) => c.id === p.targetId);
      setText(beam, `FIRE BEAM ${Math.round(p.beamCharge * 100)}%`);
      setDisabled(beam, p.beamCharge < 1 || !target?.inBeamArc);
      setText(
        torpedo,
        p.torpedoReload > 0 ? `RELOADING ${p.torpedoReload.toFixed(1)}s` : `TORPEDO (${p.torpedoAmmo})`,
      );
      setDisabled(torpedo, p.torpedoAmmo <= 0 || p.torpedoReload > 0 || !target?.inTorpedoArc);

      setText(dmgOut, String(Math.round(p.stats.beamDamage)));
      setText(arcOut, `${Math.round(p.stats.beamArcDeg)}°`);
      setHidden(turretRow, p.stats.turretDamage <= 0);
      setText(turretOut, `${p.stats.turretDamage} dmg auto`);
      setHidden(damagedNote, !p.damaged.weapons);
    },
  };
};
