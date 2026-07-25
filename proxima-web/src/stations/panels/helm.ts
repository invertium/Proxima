// Helm: fly the ship, dock, warp, read the course.

import { DOCK_RANGE, REVERSE_THROTTLE_MIN } from '../../sim/data';
import { el, setDisabled, setFlag, setHidden, setText } from '../ui/dom';
import { holdButton, slider, tapButton } from '../ui/controls';
import { km, pct, type Panel, type Send } from './panel';
import type { Snapshot } from '../../sim/types';

export const createHelmPanel = (send: Send, onToggleMap: () => void): Panel => {
  // ── Orders ────────────────────────────────────────────────────────────────────
  const accept = tapButton('ACCEPT ORDERS', () => send({ c: 'acceptObjective' }), { class: 'alert wide' });
  accept.hidden = true;

  // ── Throttle ──────────────────────────────────────────────────────────────────
  // A continuous lever for trimming, plus discrete taps for a panic stop. Both are
  // wanted: you cannot reliably drag to zero in a fight, and you cannot ease onto a
  // docking approach with four buttons.
  const throttle = slider(
    { min: REVERSE_THROTTLE_MIN * 100, max: 100, step: 1, detent: 3 },
    (v) => send({ c: 'throttle', v: v / 100 }),
  );
  const throttleLabel = el('b', { text: '0%' });

  const setThrottle = (fraction: number) => {
    throttle.input.value = String(Math.round(fraction * 100));
    send({ c: 'throttle', v: fraction });
  };

  const throttleTaps = el('div', {
    class: 'grid',
    children: [
      tapButton('FULL', () => setThrottle(1)),
      tapButton('HALF', () => setThrottle(0.5)),
      tapButton('STOP', () => setThrottle(0)),
      tapButton('REV', () => setThrottle(REVERSE_THROTTLE_MIN)),
    ],
  });

  // ── Steering ──────────────────────────────────────────────────────────────────
  // Held, not toggled. Leaving the rudder hard over because a thumb slipped is the
  // difference between a bridge sim and a frustration.
  const steering = el('div', {
    class: 'grid two',
    children: [
      holdButton('◀ PORT', () => send({ c: 'turn', v: -1 }), () => send({ c: 'turn', v: 0 })),
      holdButton('STBD ▶', () => send({ c: 'turn', v: 1 }), () => send({ c: 'turn', v: 0 })),
    ],
  });

  const strafeRow = el('div', {
    class: 'grid two',
    children: [
      holdButton('◀ SLIDE', () => send({ c: 'strafe', v: -1 }), () => send({ c: 'strafe', v: 0 })),
      holdButton('SLIDE ▶', () => send({ c: 'strafe', v: 1 }), () => send({ c: 'strafe', v: 0 })),
    ],
  });
  const strafeNote = el('p', {
    class: 'muted',
    text: 'Manoeuvring thrusters not installed — buy them at the drydock.',
  });

  // ── Navigation ────────────────────────────────────────────────────────────────
  const dock = tapButton('DOCK', () => send({ c: 'dock' }));
  const warp = tapButton('WARP', () => send({ c: 'warp' }));
  const course = tapButton('LAY IN COURSE', () => send({ c: 'layInCourse' }));
  const mapToggle = tapButton('SECTOR MAP', onToggleMap);

  const speedOut = el('dd');
  const hullOut = el('dd');
  const objectiveOut = el('dd');
  const baseOut = el('dd');
  const contractRow = el('div', { class: 'dl-row' });
  const contractOut = el('dd');

  const readouts = el('dl', {
    children: [
      el('dt', { text: 'SPEED' }),
      speedOut,
      el('dt', { text: 'HULL' }),
      hullOut,
      el('dt', { text: 'STARBASE' }),
      baseOut,
      el('dt', { text: 'OBJECTIVE' }),
      objectiveOut,
    ],
  });
  contractRow.append(el('dt', { text: 'CONTRACT' }), contractOut);

  const root = el('div', {
    children: [
      accept,
      el('div', { class: 'lever', children: [el('label', { text: 'THROTTLE' }), throttle.root, throttleLabel] }),
      throttleTaps,
      steering,
      strafeRow,
      strafeNote,
      el('div', { class: 'grid two', children: [dock, warp] }),
      el('div', { class: 'grid two', children: [course, mapToggle] }),
      readouts,
      contractRow,
    ],
  });

  return {
    root,
    update(s: Snapshot) {
      const p = s.player;

      setHidden(accept, !s.objective?.offered);
      setText(accept, s.objective ? `ACCEPT ORDERS — ${s.objective.name}` : 'ACCEPT ORDERS');

      throttle.reflect(Math.round((p.speed / Math.max(1, p.maxSpeed)) * 100));
      setText(throttleLabel, `${Math.round((p.speed / Math.max(1, p.maxSpeed)) * 100)}%`);

      setHidden(strafeRow, p.stats.strafeSpeed <= 0);
      setHidden(strafeNote, p.stats.strafeSpeed > 0);

      setText(dock, p.docked ? 'UNDOCK' : 'DOCK');
      setText(warp, `WARP ${Math.round(p.warpCharge * 100)}%`);
      setDisabled(warp, p.warpCharge < 1 || p.docked);
      setDisabled(course, p.warpCharge < 1 || !s.objective || p.docked);

      setText(speedOut, `${Math.round(p.speed)} / ${Math.round(p.maxSpeed)}`);
      setText(hullOut, pct(p.hull, p.maxHull));
      setFlag(hullOut, 'bad', p.hullCritical);

      // The dock-readiness readout the C++ helm had: without it, DOCK is a button you
      // press hopefully.
      const base = s.landmarks.find((l) => l.kind === 'station');
      if (base) {
        const range = Math.hypot(base.pos.x - p.pos.x, base.pos.z - p.pos.z);
        const inRange = range <= DOCK_RANGE;
        setText(baseOut, p.docked ? 'DOCKED' : `${km(range)}${inRange ? ' — IN RANGE' : ''}`);
        setFlag(baseOut, 'ok', p.docked || inRange);
        setDisabled(dock, !p.docked && !inRange);
      }

      setText(objectiveOut, s.objective ? `${s.objective.name} · ${km(s.objective.range)}` : '—');
      setHidden(contractRow, !s.contract);
      if (s.contract) setText(contractOut, s.contract.text);
    },
  };
};
