// Crew console: Helm, Weapons, Engineering, Science. One page, station chosen by tab.
//
// The pilot HUD and this share the same Snapshot type and the same Command union —
// there is no second protocol and no hand-written endpoint per control, which is what
// the 1940-line StationServerSubsystem.cpp existed to do.

import {
  BEAM_RANGE,
  MAX_PER_SYSTEM,
  SCAN_DURATION,
  SHIPS,
  UPGRADES,
  upgradeCost,
  upgradeRankReq,
} from '../sim/data';
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
/** Helm and Science can swap the scope for the sector map. */
let showMap = false;

link.onMessage((msg) => {
  if (msg.m === 'state') snap = msg.snapshot;
});

// The relay being up isn't the same as the host being live, so report both: a crew
// member staring at a dead console needs to know which end to go and fix.
link.onStatus((connected) => {
  statusEl.textContent = connected ? 'LINKED' : 'NO LINK — RECONNECTING…';
  statusEl.className = connected ? 'ok' : '';
});

window.addEventListener('hashchange', () => {
  station = (location.hash.slice(1) as Station) || 'helm';
  render(true);
});

// ── Scope ───────────────────────────────────────────────────────────────────────

const RADAR_RANGE = 30000;

/** Tactical scope: bow-up, contacts and firing arcs. */
const drawRadar = (s: Snapshot, size: number, cx: number, cy: number): void => {
  const scale = size / 2 / (s.player.stats.scanRange > 0 ? RADAR_RANGE : RADAR_RANGE);

  ctx.strokeStyle = '#12283f';
  ctx.lineWidth = 1;
  for (let r = 1; r <= 3; r++) {
    ctx.beginPath();
    ctx.arc(cx, cy, (size / 2) * (r / 3), 0, Math.PI * 2);
    ctx.stroke();
  }

  // Both firing arcs, so Weapons can see which solution it has. The torpedo arc is
  // wider than the beam arc, which is the whole reason to show them separately.
  const wedge = (arcDeg: number, radius: number, fill: string) => {
    const half = (arcDeg / 2) * (Math.PI / 180);
    ctx.fillStyle = fill;
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.arc(cx, cy, radius * scale, -Math.PI / 2 - half, -Math.PI / 2 + half);
    ctx.closePath();
    ctx.fill();
  };
  wedge(110, BEAM_RANGE * 0.8, 'rgba(251, 191, 36, .07)'); // torpedo
  wedge(s.player.stats.beamArcDeg, BEAM_RANGE, 'rgba(56, 189, 248, .11)');

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
    ctx.fillStyle = l.kind === 'station' ? '#4ade80' : '#2e4a6b';
    ctx.beginPath();
    ctx.arc(p.x, p.y, 5, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = '#4a6c92';
    ctx.font = '10px ui-monospace, monospace';
    ctx.fillText(l.name, p.x + 8, p.y + 3);
  }

  // Sector event marker — amber, distinct from hostiles.
  if (s.event) {
    const p = project(s.event.pos);
    ctx.strokeStyle = '#fbbf24';
    ctx.beginPath();
    ctx.arc(p.x, p.y, 9, 0, Math.PI * 2);
    ctx.stroke();
    ctx.fillStyle = '#fbbf24';
    ctx.font = '10px ui-monospace, monospace';
    ctx.fillText(s.event.kind.toUpperCase(), p.x + 12, p.y + 3);
  }

  for (const t of s.torpedoes) {
    const p = project(t.pos);
    ctx.fillStyle = t.friendly ? '#fde68a' : '#fb7185';
    ctx.fillRect(p.x - 2, p.y - 2, 4, 4);
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
    if (c.scanned) {
      ctx.strokeStyle = '#34d399';
      ctx.strokeRect(p.x - 8, p.y - 8, 16, 16);
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

/** Navigation map: the whole sector, north-up, with the objective highlighted. */
const drawSectorMap = (s: Snapshot, size: number): void => {
  const pad = 26;
  const span = 220000;
  const toScreen = (p: { x: number; z: number }) => ({
    x: pad + ((p.x + span / 2) / span) * (size - pad * 2),
    y: pad + ((p.z + span / 2) / span) * (size - pad * 2),
  });

  ctx.strokeStyle = '#12283f';
  ctx.strokeRect(pad, pad, size - pad * 2, size - pad * 2);
  ctx.fillStyle = '#4a6c92';
  ctx.font = '10px ui-monospace, monospace';
  ctx.fillText('SECTOR — THE VEIL', pad, pad - 8);

  for (const l of s.landmarks) {
    const p = toScreen(l.pos);
    const isObjective = s.objective?.name === l.name;
    ctx.fillStyle = l.kind === 'sun' ? '#fbbf24' : l.kind === 'station' ? '#4ade80' : '#38bdf8';
    ctx.beginPath();
    ctx.arc(p.x, p.y, l.kind === 'sun' ? 9 : 6, 0, Math.PI * 2);
    ctx.fill();

    if (isObjective) {
      ctx.strokeStyle = '#fbbf24';
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.arc(p.x, p.y, 14, 0, Math.PI * 2);
      ctx.stroke();
      ctx.lineWidth = 1;
    }
    ctx.fillStyle = isObjective ? '#fde68a' : '#7d9dc4';
    ctx.fillText(l.name, p.x + 11, p.y + 3);
  }

  // Player marker, pointing along the heading.
  const me = toScreen(s.player.pos);
  ctx.save();
  ctx.translate(me.x, me.y);
  ctx.rotate(s.player.heading + Math.PI / 2);
  ctx.fillStyle = '#4ade80';
  ctx.beginPath();
  ctx.moveTo(0, -8);
  ctx.lineTo(-5, 6);
  ctx.lineTo(5, 6);
  ctx.closePath();
  ctx.fill();
  ctx.restore();
};

const drawScope = (s: Snapshot): void => {
  const dpr = Math.min(devicePixelRatio, 2);
  const size = canvas.clientWidth;
  if (canvas.width !== size * dpr) canvas.width = canvas.height = size * dpr;

  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, size, size);

  const wantMap = showMap && (station === 'helm' || station === 'science');
  if (wantMap) drawSectorMap(s, size);
  else drawRadar(s, size, size / 2, size / 2);
};

// ── Station panels ──────────────────────────────────────────────────────────────

const pct = (v: number, max: number) => `${Math.round((max > 0 ? v / max : 0) * 100)}%`;
const km = (v: number) => `${(v / 1000).toFixed(1)} km`;

const panels: Record<Station, (s: Snapshot) => string> = {
  helm: (s) => `
    ${
      s.objective?.offered
        ? `<button data-cmd="acceptObjective" class="alert">ACCEPT ORDERS — ${s.objective.name}</button>`
        : ''
    }
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
    <div class="grid">
      <button data-cmd="strafe:-1">◀ SLIDE</button>
      <button data-cmd="strafe:0">CENTRE</button>
      <button data-cmd="strafe:1">SLIDE ▶</button>
    </div>
    ${s.player.stats.strafeSpeed === 0 ? '<p class="muted">Manoeuvring thrusters not installed — buy them at the drydock.</p>' : ''}
    <div class="grid two">
      <button data-cmd="dock">${s.player.docked ? 'UNDOCK' : 'DOCK'}</button>
      <button data-cmd="warp" ${s.player.warpCharge < 1 ? 'disabled' : ''}>WARP ${Math.round(s.player.warpCharge * 100)}%</button>
    </div>
    <div class="grid two">
      <button data-cmd="layInCourse" ${s.player.warpCharge < 1 || !s.objective ? 'disabled' : ''}>LAY IN COURSE</button>
      <button data-view="map">${showMap ? 'TACTICAL SCOPE' : 'SECTOR MAP'}</button>
    </div>
    <dl>
      <dt>SPEED</dt><dd>${Math.round(s.player.speed)} / ${Math.round(s.player.maxSpeed)}</dd>
      <dt>HULL</dt><dd class="${s.player.hullCritical ? 'bad' : ''}">${pct(s.player.hull, s.player.maxHull)}</dd>
      <dt>OBJECTIVE</dt><dd>${s.objective ? `${s.objective.name} · ${km(s.objective.range)}` : '—'}</dd>
      ${s.contract ? `<dt>CONTRACT</dt><dd>${s.contract.text}</dd>` : ''}
      ${s.mode === 'skirmish' ? `<dt>WAVE</dt><dd>${s.skirmishWave}</dd>` : ''}
    </dl>`,

  weapons: (s) => {
    const list = s.contacts.length
      ? s.contacts
          .map(
            (c) => `<button class="contact ${c.id === s.player.targetId ? 'sel' : ''}" data-cmd="target:${c.id}">
              <b>${c.name}</b>
              <span>${km(c.range)} · ${c.scanned ? `hull ${Math.round(c.hull)}/${Math.round(c.maxHull)} · shield ${Math.round(c.shield)}` : 'unscanned — Science can resolve it'}</span>
              <em class="${c.inBeamArc ? 'ok' : ''}">${c.inBeamArc ? 'BEAM SOLUTION' : c.inTorpedoArc ? 'TORPEDO ARC ONLY' : 'NO SOLUTION'}</em>
            </button>`,
          )
          .join('')
      : '<p class="muted">No contacts.</p>';

    const target = s.contacts.find((c) => c.id === s.player.targetId);
    return `
      <div class="contacts">${list}</div>
      <div class="grid two">
        <button data-cmd="fireBeam" ${s.player.beamCharge < 1 || !target?.inBeamArc ? 'disabled' : ''}>FIRE BEAM ${Math.round(s.player.beamCharge * 100)}%</button>
        <button data-cmd="fireTorpedo" ${s.player.torpedoAmmo <= 0 || !target?.inTorpedoArc ? 'disabled' : ''}>TORPEDO (${s.player.torpedoAmmo})</button>
      </div>
      <dl>
        <dt>BEAM DMG</dt><dd>${Math.round(s.player.stats.beamDamage)}</dd>
        <dt>BEAM ARC</dt><dd>${Math.round(s.player.stats.beamArcDeg)}°</dd>
        ${s.player.stats.turretDamage > 0 ? `<dt>TURRET</dt><dd>${s.player.stats.turretDamage} dmg auto</dd>` : ''}
        ${s.player.damaged.weapons ? '<dt class="bad">WEAPONS</dt><dd class="bad">DAMAGED — half recharge</dd>' : ''}
      </dl>`;
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

    const damage = (['engine', 'weapons', 'sensors'] as const)
      .map(
        (d) => `<span class="chip ${s.player.damaged[d] ? 'bad' : 'ok'}">${d.toUpperCase()}</span>`,
      )
      .join('');

    const upgrades = s.player.docked
      ? UPGRADES.map((u) => {
          const tier = s.player.upgrades[u.id] ?? 0;
          const maxed = tier >= u.maxTier;
          const cost = upgradeCost(u, tier);
          const rankOk = s.player.rank >= upgradeRankReq(tier);
          const afford = s.player.credits >= cost;
          return `<button data-cmd="buyUpgrade:${u.id}" ${maxed || !rankOk || !afford ? 'disabled' : ''}>
            <b>${u.name}</b>
            <span>${maxed ? 'MAX' : `${cost} cr · rank ${upgradeRankReq(tier)} · +${u.magnitudePerTier}${u.unit}`}</span>
            <em>tier ${tier}/${u.maxTier}</em>
          </button>`;
        }).join('')
      : '<p class="muted">Dock at a starbase to open the drydock.</p>';

    const hulls = s.player.docked
      ? SHIPS.map((d) => {
          const owned = s.player.ownedShips.includes(d.type);
          const active = d.type === s.player.shipType;
          const ok = owned || (s.player.credits >= d.cost && s.player.rank >= d.rankReq);
          return `<button data-cmd="buyShip:${d.type}" ${active || !ok ? 'disabled' : ''}>
            <b>${d.name}</b><span>${active ? 'ACTIVE' : owned ? 'owned' : `${d.cost} cr · rank ${d.rankReq}`}</span>
          </button>`;
        }).join('')
      : '';

    return `
      ${rows}
      <p class="muted">Reactor ${used.toFixed(1)} / ${s.player.stats.reactorBudget.toFixed(1)}</p>
      <div class="grid">
        <button data-cmd="preset:engines">RUN</button>
        <button data-cmd="preset:weapons">ATTACK</button>
        <button data-cmd="preset:shields">TURTLE</button>
        <button data-cmd="preset:balanced">BALANCED</button>
      </div>

      <h3>DAMAGE CONTROL</h3>
      <div class="chips">${damage}</div>
      <button data-cmd="weld" class="${s.player.repairTarget ? 'alert' : ''}">
        ${s.player.repairTarget ? `WELD ${s.player.repairTarget.toUpperCase()} (${s.player.repairWelds}/3)` : 'PATCH HULL'}
      </button>

      <h3>DRYDOCK · ${s.player.credits} cr · rank ${s.player.rank}</h3>
      <div class="contacts">${upgrades}</div>
      ${hulls ? `<h3>HULLS</h3><div class="grid two">${hulls}</div>` : ''}`;
  },

  science: (s) => {
    const scanning = s.player.scanning && s.player.scanTargetId !== null;
    const list = s.contacts.length
      ? s.contacts
          .map(
            (c) => `<button class="contact ${c.id === s.player.scanTargetId ? 'sel' : ''}" data-cmd="scan:${c.id}">
              <b>${c.name}</b>
              <span>${km(c.range)} · ${c.range <= s.player.stats.scanRange ? 'in sensor range' : 'out of range'}</span>
              <em class="${c.scanned ? 'ok' : ''}">${c.scanned ? 'RESOLVED' : 'unresolved'}</em>
            </button>`,
          )
          .join('')
      : '<p class="muted">No contacts.</p>';

    return `
      <div class="grid two">
        <button data-view="map">${showMap ? 'TACTICAL SCOPE' : 'SECTOR MAP'}</button>
        <button data-cmd="scan:none" ${scanning ? '' : 'disabled'}>CANCEL SCAN</button>
      </div>
      ${scanning ? `<div class="bar"><i style="width:${Math.round(s.player.scanProgress * 100)}%"></i></div><p class="muted">Scanning — hold the lock for ${SCAN_DURATION}s.</p>` : ''}
      <h3>CONTACTS</h3>
      <div class="contacts">${list}</div>
      <dl>
        <dt>SENSOR RANGE</dt><dd class="${s.player.damaged.sensors ? 'bad' : ''}">${km(s.player.stats.scanRange)}${s.player.damaged.sensors ? ' (DAMAGED)' : ''}</dd>
        <dt>XP / RANK</dt><dd>${s.player.xp} · rank ${s.player.rank}</dd>
        ${s.event ? `<dt>EVENT</dt><dd>${s.event.kind.toUpperCase()} · ${Math.ceil(s.event.timeLeft)}s</dd>` : ''}
      </dl>
      <h3>COMMS</h3>
      <div class="comms">${s.comms.map((c) => `<p><b>${c.sender}</b> ${c.text}</p>`).join('') || '<p class="muted">Channel quiet.</p>'}</div>`;
  },
};

// ── Wiring ──────────────────────────────────────────────────────────────────────

/** Reactor presets: one tap instead of dragging three sliders mid-fight. */
const PRESETS: Record<string, [number, number, number]> = {
  engines: [2, 0.5, 0.5],
  weapons: [0.5, 2, 0.5],
  shields: [0.5, 0.5, 2],
  balanced: [1, 1, 1],
};

const parseCmd = (raw: string): Command[] => {
  const [name, arg] = raw.split(':');
  switch (name) {
    case 'throttle':
      return [{ c: 'throttle', v: Number(arg) }];
    case 'turn':
      return [{ c: 'turn', v: Number(arg) }];
    case 'strafe':
      return [{ c: 'strafe', v: Number(arg) }];
    case 'target':
      return [{ c: 'target', id: Number(arg) }];
    case 'scan':
      return [{ c: 'scan', id: arg === 'none' ? null : Number(arg) }];
    case 'fireBeam':
      return [{ c: 'fireBeam' }];
    case 'fireTorpedo':
      return [{ c: 'fireTorpedo' }];
    case 'dock':
      return [{ c: 'dock' }];
    case 'warp':
      return [{ c: 'warp' }];
    case 'layInCourse':
      return [{ c: 'layInCourse' }];
    case 'weld':
      return [{ c: 'weld' }];
    case 'acceptObjective':
      return [{ c: 'acceptObjective' }];
    case 'acceptContract':
      return [{ c: 'acceptContract' }];
    case 'buyShip':
      return [{ c: 'buyShip', type: arg as PlayerShipType }];
    case 'buyUpgrade':
      return [{ c: 'buyUpgrade', id: arg! }];
    case 'preset': {
      // Order matters: the reactor caps the total, so drop the others before raising
      // one, or the raise gets clamped by whatever is still allocated.
      const [e, w, sh] = PRESETS[arg ?? 'balanced'] ?? PRESETS['balanced']!;
      return [
        { c: 'power', system: 'engines', v: 0 },
        { c: 'power', system: 'weapons', v: 0 },
        { c: 'power', system: 'shields', v: 0 },
        { c: 'power', system: 'engines', v: e! },
        { c: 'power', system: 'weapons', v: w! },
        { c: 'power', system: 'shields', v: sh! },
      ];
    }
    default:
      return [];
  }
};

panel.addEventListener('click', (e) => {
  const btn = (e.target as HTMLElement).closest('[data-cmd],[data-view]') as HTMLElement | null;
  if (!btn) return;

  if (btn.dataset.view === 'map') {
    showMap = !showMap;
    render(true);
    return;
  }
  for (const cmd of parseCmd(btn.dataset.cmd!)) send(cmd);
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
 * Repaints the panel. Only rebuilds when the markup actually changed, and never while
 * a slider has focus — a console that stomps its own controls is unusable on a phone.
 */
let lastHtml = '';
const render = (force = false): void => {
  if (!snap) return;

  for (const b of tabs.querySelectorAll('[data-station]')) {
    b.classList.toggle('sel', (b as HTMLElement).dataset.station === station);
  }

  const html = panels[station](snap);
  if ((force || html !== lastHtml) && (document.activeElement as HTMLElement)?.tagName !== 'INPUT') {
    panel.innerHTML = html;
    lastHtml = html;
  }

  drawScope(snap);
};

setInterval(render, 100);
link.send({ m: 'join', station, version: 1 });
