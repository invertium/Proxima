// Crew console: Helm, Weapons, Engineering, Science. One page, one canvas radar,
// station chosen by tab.
//
// The pilot HUD and this share the same Snapshot type and the same Command union —
// there is no second protocol and no hand-written endpoint per control.

import { BEAM_ARC_DEG, BEAM_RANGE, MAX_PER_SYSTEM, REACTOR_BUDGET, SHIPS } from '../sim/data';
import { RelayStation } from '../net/transport';
import type { Command, PlayerShipType, ShipSystem, Snapshot, Station } from '../sim/types';

const canvas = document.getElementById('radar') as HTMLCanvasElement;
const ctx = canvas.getContext('2d')!;
const panel = document.getElementById('panel') as HTMLDivElement;
const tabs = document.getElementById('tabs') as HTMLDivElement;
const statusEl = document.getElementById('status') as HTMLDivElement;

const link = new RelayStation();
const send = (cmd: Command): void => link.send({ m: 'cmd', cmd });

let snap: Snapshot | null = null;
let station: Station = (location.hash.slice(1) as Station) || 'helm';

link.onMessage((msg) => {
  if (msg.m === 'state') snap = msg.snapshot;
});

// The relay being up isn't the same as the host being live, so report both: a crew
// member staring at a dead console needs to know which end to go and fix.
link.onStatus((connected) => {
  statusEl.textContent = connected ? 'LINKED' : 'NO LINK — RECONNECTING…';
  statusEl.className = connected ? 'ok' : '';
});

// Deep links and the back button pick the station, not just the tab bar.
window.addEventListener('hashchange', () => {
  station = (location.hash.slice(1) as Station) || 'helm';
  render(true);
});

// ── Radar ───────────────────────────────────────────────────────────────────────

const RADAR_RANGE = 30000;

const drawRadar = (s: Snapshot): void => {
  const dpr = Math.min(devicePixelRatio, 2);
  const size = canvas.clientWidth;
  if (canvas.width !== size * dpr) {
    canvas.width = canvas.height = size * dpr;
  }

  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, size, size);

  const cx = size / 2;
  const cy = size / 2;
  const scale = size / 2 / RADAR_RANGE;

  // Range rings.
  ctx.strokeStyle = '#12283f';
  ctx.lineWidth = 1;
  for (let r = 1; r <= 3; r++) {
    ctx.beginPath();
    ctx.arc(cx, cy, (size / 2) * (r / 3), 0, Math.PI * 2);
    ctx.stroke();
  }

  // Beam firing arc, drawn bow-up (the radar rotates with the ship).
  const half = (BEAM_ARC_DEG / 2) * (Math.PI / 180);
  ctx.fillStyle = 'rgba(56, 189, 248, .10)';
  ctx.beginPath();
  ctx.moveTo(cx, cy);
  ctx.arc(cx, cy, BEAM_RANGE * scale, -Math.PI / 2 - half, -Math.PI / 2 + half);
  ctx.closePath();
  ctx.fill();

  // Contacts, rotated into the ship's frame so "up" is always the bow.
  const project = (p: { x: number; z: number }) => {
    const dx = p.x - s.player.pos.x;
    const dz = p.z - s.player.pos.z;
    const a = -s.player.heading - Math.PI / 2;
    return {
      x: cx + (dx * Math.cos(a) - dz * Math.sin(a)) * scale,
      y: cy + (dx * Math.sin(a) + dz * Math.cos(a)) * scale,
    };
  };

  for (const l of s.landmarks) {
    const p = project(l.pos);
    ctx.fillStyle = '#2e4a6b';
    ctx.beginPath();
    ctx.arc(p.x, p.y, 5, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = '#4a6c92';
    ctx.font = '10px ui-monospace, monospace';
    ctx.fillText(l.name, p.x + 8, p.y + 3);
  }

  for (const c of s.contacts) {
    const p = project(c.pos);
    const selected = c.id === s.player.targetId;
    ctx.fillStyle = selected ? '#fbbf24' : c.hostile ? '#f87171' : '#94a3b8';
    ctx.beginPath();
    ctx.arc(p.x, p.y, selected ? 6 : 4, 0, Math.PI * 2);
    ctx.fill();

    if (selected) {
      ctx.strokeStyle = '#fbbf24';
      ctx.beginPath();
      ctx.arc(p.x, p.y, 11, 0, Math.PI * 2);
      ctx.stroke();
    }
  }

  // Own ship.
  ctx.fillStyle = '#4ade80';
  ctx.beginPath();
  ctx.moveTo(cx, cy - 8);
  ctx.lineTo(cx - 5, cy + 6);
  ctx.lineTo(cx + 5, cy + 6);
  ctx.closePath();
  ctx.fill();
};

// ── Station panels ──────────────────────────────────────────────────────────────

const pct = (v: number, max: number) => `${Math.round((max > 0 ? v / max : 0) * 100)}%`;

const panels: Record<Station, (s: Snapshot) => string> = {
  helm: (s) => `
    <div class="grid">
      <button data-cmd="throttle:1">FULL AHEAD</button>
      <button data-cmd="throttle:0.5">HALF</button>
      <button data-cmd="throttle:0">STOP</button>
      <button data-cmd="throttle:-0.5">REVERSE</button>
    </div>
    <div class="grid">
      <button data-cmd="turn:-1">◀ PORT</button>
      <button data-cmd="turn:0">RUDDER MID</button>
      <button data-cmd="turn:1">STBD ▶</button>
    </div>
    <div class="grid two">
      <button data-cmd="dock">${s.player.docked ? 'UNDOCK' : 'DOCK'}</button>
      <button data-cmd="warp" ${s.player.warpCharge < 1 ? 'disabled' : ''}>WARP ${Math.round(s.player.warpCharge * 100)}%</button>
    </div>
    <dl>
      <dt>SPEED</dt><dd>${Math.round(s.player.speed)} / ${s.player.maxSpeed}</dd>
      <dt>HULL</dt><dd>${pct(s.player.hull, s.player.maxHull)}</dd>
      <dt>OBJECTIVE</dt><dd>${s.objective ? `${s.objective.name} · ${(s.objective.range / 1000).toFixed(1)} km` : '—'}</dd>
    </dl>`,

  weapons: (s) => {
    const list = s.contacts.length
      ? s.contacts
          .map(
            (c) => `<button class="contact ${c.id === s.player.targetId ? 'sel' : ''}" data-cmd="target:${c.id}">
              <b>${c.name}</b>
              <span>${(c.range / 1000).toFixed(1)} km · hull ${Math.round(c.hull)}</span>
              <em class="${c.inBeamArc ? 'ok' : ''}">${c.inBeamArc ? 'IN ARC' : 'NO SOLUTION'}</em>
            </button>`,
          )
          .join('')
      : '<p class="muted">No contacts.</p>';

    return `
      <div class="contacts">${list}</div>
      <div class="grid two">
        <button data-cmd="fireBeam" ${s.player.beamCharge < 1 ? 'disabled' : ''}>FIRE BEAM ${Math.round(s.player.beamCharge * 100)}%</button>
        <button data-cmd="fireTorpedo" ${s.player.torpedoAmmo <= 0 ? 'disabled' : ''}>TORPEDO (${s.player.torpedoAmmo})</button>
      </div>`;
  },

  engineering: (s) => {
    const used = s.player.power.engines + s.player.power.weapons + s.player.power.shields;
    const rows = (['engines', 'weapons', 'shields'] as ShipSystem[])
      .map(
        (sys) => `<div class="pwr">
          <label>${sys.toUpperCase()}</label>
          <input type="range" min="0" max="${MAX_PER_SYSTEM}" step="0.1" value="${s.player.power[sys]}" data-power="${sys}">
          <b>${s.player.power[sys].toFixed(1)}</b>
        </div>`,
      )
      .join('');

    const drydock = s.player.docked
      ? SHIPS.map(
          (d) => `<button data-cmd="buyShip:${d.type}" ${d.type === s.player.shipType || s.player.credits < d.cost ? 'disabled' : ''}>
            ${d.name} — ${d.cost === 0 ? 'owned' : `${d.cost} cr`}
          </button>`,
        ).join('')
      : '<p class="muted">Dock at a starbase to open the drydock.</p>';

    return `
      ${rows}
      <p class="muted">Reactor ${used.toFixed(1)} / ${REACTOR_BUDGET.toFixed(1)}</p>
      <h3>DRYDOCK · ${s.player.credits} cr</h3>
      <div class="grid two">${drydock}</div>`;
  },

  science: (s) => `
    <dl>
      <dt>SECTOR</dt><dd>${s.objective?.name ?? 'all systems clear'}</dd>
      <dt>CONTACTS</dt><dd>${s.contacts.length}</dd>
      <dt>XP</dt><dd>${s.player.xp}</dd>
    </dl>
    <h3>COMMS</h3>
    <div class="comms">${s.comms.map((c) => `<p><b>${c.sender}</b> ${c.text}</p>`).join('') || '<p class="muted">Channel quiet.</p>'}</div>`,
};

// ── Wiring ──────────────────────────────────────────────────────────────────────

const parseCmd = (raw: string): Command | null => {
  const [name, arg] = raw.split(':');
  switch (name) {
    case 'throttle':
      return { c: 'throttle', v: Number(arg) };
    case 'turn':
      return { c: 'turn', v: Number(arg) };
    case 'target':
      return { c: 'target', id: Number(arg) };
    case 'fireBeam':
      return { c: 'fireBeam' };
    case 'fireTorpedo':
      return { c: 'fireTorpedo' };
    case 'dock':
      return { c: 'dock' };
    case 'warp':
      return { c: 'warp' };
    case 'buyShip':
      return { c: 'buyShip', type: arg as PlayerShipType };
    default:
      return null;
  }
};

panel.addEventListener('click', (e) => {
  const btn = (e.target as HTMLElement).closest('[data-cmd]') as HTMLElement | null;
  if (!btn) return;
  const cmd = parseCmd(btn.dataset.cmd!);
  if (cmd) send(cmd);
});

panel.addEventListener('input', (e) => {
  const slider = e.target as HTMLInputElement;
  if (!slider.dataset.power) return;
  send({ c: 'power', system: slider.dataset.power as ShipSystem, v: Number(slider.value) });
});

tabs.addEventListener('click', (e) => {
  const btn = (e.target as HTMLElement).closest('[data-station]') as HTMLElement | null;
  if (!btn) return;
  station = btn.dataset.station as Station;
  location.hash = station;
  render(true);
});

/**
 * Repaints the panel. Sliders and focus are preserved by only rebuilding the innerHTML
 * when the station changes or the markup actually differs — a station console that
 * stomps its own controls 60 times a second is unusable on a phone.
 */
let lastHtml = '';
const render = (force = false): void => {
  if (!snap) return;

  for (const b of tabs.querySelectorAll('[data-station]')) {
    b.classList.toggle('sel', (b as HTMLElement).dataset.station === station);
  }

  const html = panels[station](snap);
  if (force || html !== lastHtml) {
    const active = document.activeElement as HTMLElement | null;
    if (active?.tagName !== 'INPUT') {
      panel.innerHTML = html;
      lastHtml = html;
    }
  }

  drawRadar(snap);
};

setInterval(render, 100);
link.send({ m: 'join', station, version: 1 });
