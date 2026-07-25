// Science: resolve contacts, read the sector, keep the comms log.

import { SCAN_DURATION } from '../../sim/data';
import { el, setDisabled, setFlag, setHidden, setText, setWidth, syncList } from '../ui/dom';
import { tapButton } from '../ui/controls';
import { km, type Panel, type Send } from './panel';
import type { Snapshot } from '../../sim/types';

type Contact = Snapshot['contacts'][number];

export const createSciencePanel = (send: Send, onToggleMap: () => void): Panel => {
  const mapToggle = tapButton('SECTOR MAP', onToggleMap);
  const cancel = tapButton('CANCEL SCAN', () => send({ c: 'scan', id: null }));

  const progress = el('i');
  const progressBar = el('div', { class: 'bar', children: [progress] });
  const progressNote = el('p', {
    class: 'muted',
    text: `Scanning — hold the lock for ${SCAN_DURATION}s.`,
  });
  const scanning = el('div', { children: [progressBar, progressNote] });

  const list = el('div', { class: 'contacts' });
  const empty = el('p', { class: 'muted', text: 'No contacts.' });
  const rows = new Map<string, HTMLElement>();

  const sensorOut = el('dd');
  const xpOut = el('dd');
  const eventRow = el('div', { class: 'dl-row' });
  const eventOut = el('dd');
  eventRow.append(el('dt', { text: 'EVENT' }), eventOut);

  const comms = el('div', { class: 'comms' });
  const commsRows = new Map<string, HTMLElement>();
  const commsEmpty = el('p', { class: 'muted', text: 'Channel quiet.' });

  const root = el('div', {
    children: [
      el('div', { class: 'grid two', children: [mapToggle, cancel] }),
      scanning,
      el('h3', { text: 'CONTACTS' }),
      empty,
      list,
      el('dl', {
        children: [el('dt', { text: 'SENSORS' }), sensorOut, el('dt', { text: 'XP / RANK' }), xpOut],
      }),
      eventRow,
      el('h3', { text: 'COMMS' }),
      commsEmpty,
      comms,
    ],
  });

  const createRow = (c: Contact): HTMLElement => {
    const row = el('button', {
      class: 'contact',
      children: [el('b'), el('span'), el('em')],
    });
    row.addEventListener('click', () => send({ c: 'scan', id: Number(row.dataset['id']) }));
    return row;
  };

  return {
    root,
    update(s: Snapshot) {
      const p = s.player;
      const isScanning = p.scanning && p.scanTargetId !== null;

      setText(mapToggle, 'SECTOR MAP');
      setDisabled(cancel, !isScanning);
      setHidden(scanning, !isScanning);
      setWidth(progress, p.scanProgress);

      setHidden(empty, s.contacts.length > 0);
      syncList(
        list,
        s.contacts,
        (c) => String(c.id),
        createRow,
        (node, c) => {
          node.dataset['id'] = String(c.id);
          setFlag(node, 'sel', c.id === p.scanTargetId);
          const [name, detail, state] = node.children as unknown as HTMLElement[];
          setText(name!, `${c.name} · ${c.className}`);
          setText(
            detail!,
            `${km(c.range)} · ${c.range <= p.stats.scanRange ? 'in sensor range' : 'out of range'}`,
          );
          setText(state!, c.scanned ? 'RESOLVED' : 'unresolved');
          setFlag(state!, 'ok', c.scanned);
          setDisabled(node as HTMLButtonElement, c.range > p.stats.scanRange && !c.scanned);
        },
        rows,
      );

      setText(sensorOut, `${km(p.stats.scanRange)}${p.damaged.sensors ? ' (DAMAGED)' : ''}`);
      setFlag(sensorOut, 'bad', p.damaged.sensors);
      setText(xpOut, `${p.xp} · rank ${p.rank}`);

      setHidden(eventRow, !s.event);
      if (s.event) setText(eventOut, `${s.event.kind.toUpperCase()} · ${Math.ceil(s.event.timeLeft)}s`);

      setHidden(commsEmpty, s.comms.length > 0);
      syncList(
        comms,
        s.comms,
        (c) => `${c.at}:${c.sender}`,
        () => el('p', { children: [el('b'), el('span')] }),
        (node, c) => {
          const [sender, text] = node.children as unknown as HTMLElement[];
          setText(sender!, c.sender);
          setText(text!, ` ${c.text}`);
        },
        commsRows,
      );
    },
  };
};
