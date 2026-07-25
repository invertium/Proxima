// Engineering: reactor, damage control, the drydock, and the contract board.

import {
  MAX_PER_SYSTEM,
  SHIPS,
  UPGRADES,
  WELD_GREEN_MAX,
  WELD_GREEN_MIN,
  WELD_SWEEP_PERIOD,
  upgradeCost,
  upgradeRankReq,
} from '../../sim/data';
import { el, setDisabled, setFlag, setHidden, setText, syncList } from '../ui/dom';
import { slider, sweepGauge, tapButton } from '../ui/controls';
import { type Panel, type Send } from './panel';
import type { ShipSystem, Snapshot } from '../../sim/types';

const SYSTEMS: ShipSystem[] = ['engines', 'weapons', 'shields'];
const DAMAGE_SYSTEMS = ['engine', 'weapons', 'sensors'] as const;

/** One tap instead of dragging three sliders mid-fight. */
const PRESETS: Record<string, [number, number, number]> = {
  combat: [0.4, 1.3, 1.3],
  travel: [1.6, 0.7, 0.7],
  balanced: [1, 1, 1],
};

export const createEngineeringPanel = (send: Send): Panel => {
  // ── Reactor ───────────────────────────────────────────────────────────────────
  const powerRows = SYSTEMS.map((sys) => {
    const value = el('b');
    const control = slider({ min: 0, max: MAX_PER_SYSTEM, step: 0.1 }, (v) =>
      send({ c: 'power', system: sys, v }),
    );
    const row = el('div', {
      class: 'pwr',
      children: [el('label', { text: sys.toUpperCase() }), control.root, value],
    });
    return { sys, control, value, row };
  });

  const reactorOut = el('p', { class: 'muted' });

  const applyPreset = (name: keyof typeof PRESETS): void => {
    // Zero everything first: the reactor caps the total, so raising a row before
    // dropping the others just gets clamped against power still allocated.
    const [e, w, sh] = PRESETS[name]!;
    for (const sys of SYSTEMS) send({ c: 'power', system: sys, v: 0 });
    send({ c: 'power', system: 'engines', v: e });
    send({ c: 'power', system: 'weapons', v: w });
    send({ c: 'power', system: 'shields', v: sh });
  };

  const presets = el('div', {
    class: 'grid',
    children: [
      tapButton('COMBAT', () => applyPreset('combat')),
      tapButton('TRAVEL', () => applyPreset('travel')),
      tapButton('BALANCED', () => applyPreset('balanced')),
    ],
  });

  // ── Damage control ────────────────────────────────────────────────────────────
  const chips = DAMAGE_SYSTEMS.map((d) => el('span', { class: 'chip', text: d.toUpperCase() }));
  const weldLabel = el('p', { class: 'muted' });
  const sweep = sweepGauge(
    { period: WELD_SWEEP_PERIOD, greenMin: WELD_GREEN_MIN, greenMax: WELD_GREEN_MAX },
    (phase) => send({ c: 'weld', phase }),
  );

  // ── Contract board ────────────────────────────────────────────────────────────
  const boardText = el('p', { class: 'muted' });
  const acceptContract = tapButton('ACCEPT CONTRACT', () => send({ c: 'acceptContract' }), {
    class: 'alert',
  });
  const board = el('div', { children: [boardText, acceptContract] });

  // ── Drydock ───────────────────────────────────────────────────────────────────
  const dockedNote = el('p', { class: 'muted', text: 'Dock at a starbase to open the drydock.' });
  const upgradeList = el('div', { class: 'contacts' });
  const upgradeRows = new Map<string, HTMLElement>();
  const hullList = el('div', { class: 'grid two' });
  const hullRows = new Map<string, HTMLElement>();
  const drydockHead = el('h3');

  const root = el('div', {
    children: [
      ...powerRows.map((r) => r.row),
      reactorOut,
      presets,
      el('h3', { text: 'DAMAGE CONTROL' }),
      el('div', { class: 'chips', children: chips }),
      sweep.root,
      weldLabel,
      el('h3', { text: 'CONTRACT BOARD' }),
      board,
      drydockHead,
      dockedNote,
      upgradeList,
      hullList,
    ],
  });

  const createUpgradeRow = (u: (typeof UPGRADES)[number]): HTMLElement => {
    const row = el('button', {
      children: [el('b', { text: u.name }), el('span'), el('em')],
    });
    row.addEventListener('click', () => send({ c: 'buyUpgrade', id: u.id }));
    return row;
  };

  const createHullRow = (d: (typeof SHIPS)[number]): HTMLElement => {
    const row = el('button', { children: [el('b', { text: d.name }), el('span')] });
    row.addEventListener('click', () => send({ c: 'buyShip', type: d.type }));
    return row;
  };

  return {
    root,
    update(s: Snapshot) {
      const p = s.player;

      for (const { sys, control, value } of powerRows) {
        control.reflect(p.power[sys]);
        setText(value, p.power[sys].toFixed(1));
        // A starved system is genuinely dead now, so say so rather than letting the
        // crew wonder why nothing works.
        setFlag(value, 'bad', p.power[sys] <= 0.01);
      }
      const used = SYSTEMS.reduce((sum, sys) => sum + p.power[sys], 0);
      setText(reactorOut, `Reactor ${used.toFixed(1)} / ${p.stats.reactorBudget.toFixed(1)}`);

      DAMAGE_SYSTEMS.forEach((d, i) => {
        const chip = chips[i]!;
        setFlag(chip, 'bad', p.damaged[d]);
        setFlag(chip, 'ok', !p.damaged[d]);
      });
      // The sweep runs off the sim clock so every console's marker agrees.
      sweep.tick(s.time);
      setText(
        weldLabel,
        p.repairTarget
          ? `Repairing ${p.repairTarget.toUpperCase()} — ${p.repairWelds}/3 welds. Release in the green.`
          : 'Release in the green to patch hull.',
      );

      // Contracts: the offer only exists while docked, and only one runs at a time.
      if (s.contract) {
        setText(boardText, s.contract.text);
        setHidden(acceptContract, true);
      } else if (s.offer) {
        setText(boardText, s.offer.text);
        setHidden(acceptContract, false);
      } else {
        setText(boardText, p.docked ? 'No postings.' : 'Dock at a starbase to see the board.');
        setHidden(acceptContract, true);
      }

      setText(drydockHead, `DRYDOCK · ${p.credits} cr · rank ${p.rank}`);
      setHidden(dockedNote, p.docked);
      setHidden(upgradeList, !p.docked);
      setHidden(hullList, !p.docked);

      if (!p.docked) return;

      syncList(
        upgradeList,
        UPGRADES,
        (u) => u.id,
        createUpgradeRow,
        (node, u) => {
          const tier = p.upgrades[u.id] ?? 0;
          const maxed = tier >= u.maxTier;
          const cost = upgradeCost(u, tier);
          const rankOk = p.rank >= upgradeRankReq(tier);
          const [, detail, tierOut] = node.children as unknown as HTMLElement[];
          setText(
            detail!,
            maxed ? 'MAX' : `${cost} cr · rank ${upgradeRankReq(tier)} · +${u.magnitudePerTier}${u.unit}`,
          );
          setText(tierOut!, `tier ${tier}/${u.maxTier}`);
          setDisabled(node as HTMLButtonElement, maxed || !rankOk || p.credits < cost);
        },
        upgradeRows,
      );

      syncList(
        hullList,
        SHIPS,
        (d) => d.type,
        createHullRow,
        (node, d) => {
          const owned = p.ownedShips.includes(d.type);
          const active = d.type === p.shipType;
          const [, detail] = node.children as unknown as HTMLElement[];
          setText(detail!, active ? 'ACTIVE' : owned ? 'owned' : `${d.cost} cr · rank ${d.rankReq}`);
          setDisabled(
            node as HTMLButtonElement,
            active || (!owned && (p.credits < d.cost || p.rank < d.rankReq)),
          );
        },
        hullRows,
      );
    },
  };
};
