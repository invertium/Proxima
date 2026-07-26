// Science: the ship's comms officer. Resolves contacts, reads the sector, keeps the
// channel — and answers every hail that arrives on it.
//
// Both acceptances live here because both are transmissions, not actions on the ship:
// fleet orders are a reply to CMDR VOSS, and a station contract is a reply to the
// starbase. The C++ Science page owned ACCEPT ORDERS (StationServerSubsystem.cpp:511);
// this port had drifted it onto Helm. The contract board is a deliberate departure —
// C++ kept it on Engineering next to the drydock wallet, but that split the two
// "answer a hail" verbs across two consoles for no reason a crew could feel.

import { SCAN_DURATION } from '../../sim/data';
import { el, setDisabled, setFlag, setHidden, setText, setWidth, syncList } from '../ui/dom';
import { tapButton } from '../ui/controls';
import { km, type Panel, type Send } from './panel';
import type { Snapshot } from '../../sim/types';

type Contact = Snapshot['contacts'][number];

export const createSciencePanel = (send: Send, onToggleMap: () => void): Panel => {
  const mapToggle = tapButton('SECTOR MAP', onToggleMap);
  const cancel = tapButton('CANCEL SCAN', () => send({ c: 'scan', id: null }));

  // ── Orders ────────────────────────────────────────────────────────────────────
  // Top of the panel, as on the C++ console: when the fleet hails, the crew has to
  // find this in a hurry.
  const objectiveOut = el('dd');
  const accept = tapButton('ACCEPT ORDERS', () => send({ c: 'acceptObjective' }), {
    class: 'alert wide',
  });
  accept.hidden = true;

  // ── Contract board ────────────────────────────────────────────────────────────
  const boardText = el('p', { class: 'muted' });
  const acceptContract = tapButton('ACCEPT CONTRACT', () => send({ c: 'acceptContract' }), {
    class: 'alert',
  });

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
      el('h3', { text: 'ORDERS' }),
      el('dl', { children: [el('dt', { text: 'OBJECTIVE' }), objectiveOut] }),
      accept,
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
      el('h3', { text: 'CONTRACT BOARD' }),
      boardText,
      acceptContract,
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

      // Orders. `offered` means the ship has arrived and the fleet is waiting on a reply.
      setText(
        objectiveOut,
        s.objective
          ? `${s.objective.name} · ${km(s.objective.range)}${
              s.objective.offered ? ' — ORDERS PENDING' : s.objective.live ? ' — ENGAGED' : ''
            }`
          : '—',
      );
      setFlag(objectiveOut, 'ok', !!s.objective?.offered);
      setHidden(accept, !s.objective?.offered);
      setText(accept, s.objective ? `ACCEPT ORDERS — ${s.objective.name}` : 'ACCEPT ORDERS');

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

      // The board only posts while docked, and only one contract runs at a time.
      if (s.contract) {
        setText(boardText, `ACTIVE — ${s.contract.text}`);
        setHidden(acceptContract, true);
      } else if (s.offer) {
        setText(boardText, `ON OFFER — ${s.offer.text}`);
        setHidden(acceptContract, false);
      } else {
        setText(boardText, p.docked ? 'No postings.' : 'Dock at a starbase to see the board.');
        setHidden(acceptContract, true);
      }
    },
  };
};
