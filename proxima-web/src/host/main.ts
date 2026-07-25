// Host / pilot application. Owns the renderer, the pilot's input, and the worker that
// runs the authoritative simulation.

import { BridgeAudio } from '../render/audio';
import { SectorView } from '../render/scene';
import { RelayHost } from '../net/transport';
import { Menu, type NewGameChoice } from './menu';
import { clearCampaign, loadCampaign, saveCampaign } from './storage';
import type { ServerMessage, WorkerMessage } from '../net/protocol';
import type { SaveGame } from '../sim/save';
import type { Command, Snapshot } from '../sim/types';

const canvas = document.getElementById('view') as HTMLCanvasElement;
const hud = document.getElementById('hud') as HTMLDivElement;
const commsEl = document.getElementById('comms') as HTMLDivElement;
const bootEl = document.getElementById('boot') as HTMLDivElement;
const menuEl = document.getElementById('menu') as HTMLDivElement;

const view = new SectorView(canvas);
const audio = new BridgeAudio();
const worker = new Worker(new URL('./sim.worker.ts', import.meta.url), { type: 'module' });
const crew = new RelayHost();

let snap: Snapshot | null = null;

const send = (msg: WorkerMessage): void => worker.postMessage(msg);
const cmd = (c: Command): void => send({ m: 'cmd', cmd: c });

let latestSave: SaveGame | null = null;

const menu = new Menu(
  menuEl,
  (choice: NewGameChoice) => startGame(choice),
  () => updatePause(),
);

worker.onmessage = (ev: MessageEvent<ServerMessage>) => {
  const msg = ev.data;

  if (msg.m === 'save') {
    latestSave = msg.save;
    void saveCampaign(msg.save);
    return;
  }
  if (msg.m !== 'state') return;

  snap = msg.snapshot;
  view.ingest(msg.events);
  audio.ingest(msg.events, msg.snapshot.player.hullCritical, 1 / 60);
  crew.broadcast(msg);

  // The sim owns the outcome; the menu just reflects it.
  if (msg.snapshot.phase !== 'playing') {
    menu.setSave(latestSave);
    menu.showOutcome(msg.snapshot.phase === 'victory' ? 'victory' : 'defeat');
    // A finished campaign shouldn't resume into a dead ship on the next launch.
    if (msg.snapshot.phase === 'victory') void clearCampaign();
  }
};

const startGame = (choice: NewGameChoice): void => {
  view.reset();
  snap = null;
  send({
    m: 'boot',
    options: {
      seed: choice.save?.seed ?? Math.floor(Math.random() * 1e9),
      difficulty: choice.difficulty,
      shipType: choice.shipType,
      mode: choice.mode,
      save: choice.save ?? null,
    },
  });
  updatePause();
};

// Crew stations are untrusted: they may only submit commands, which the sim validates
// (range, arc, charge, reactor headroom) exactly as it does the pilot's.
crew.onMessage((msg) => {
  if (msg.m === 'cmd') cmd(msg.cmd);
});

/** Stations currently connected. Drives whether backgrounding this tab may pause. */
let crewCount = 0;
crew.onCrewCount((n) => {
  crewCount = n;
});

crew.onEvicted(() => {
  // Another pilot tab claimed the host role. Stop simulating and say so plainly,
  // rather than fighting it for the relay.
  send({ m: 'pause', paused: true });
  document.body.insertAdjacentHTML(
    'beforeend',
    '<div id="spectator">ANOTHER PILOT WINDOW TOOK OVER THIS SESSION — close this tab, or reload it to take control back.</div>',
  );
});

// ── Pilot input ─────────────────────────────────────────────────────────────────
//
// Held keys are resolved into a throttle/turn intent each frame rather than firing a
// command per keydown, so the command rate stays fixed regardless of key repeat.

const held = new Set<string>();

window.addEventListener('keydown', (e) => {
  // Browsers won't start an AudioContext without a gesture.
  audio.unlock();
  if (e.repeat) return;
  held.add(e.code);

  if (e.code === 'Space') cmd({ c: 'fireBeam' });
  if (e.code === 'KeyF') cmd({ c: 'fireTorpedo' });
  if (e.code === 'KeyG') cmd({ c: 'dock' });
  if (e.code === 'KeyJ') cmd({ c: 'warp' });
  if (e.code === 'KeyE') cmd({ c: 'acceptObjective' });
  if (e.code === 'KeyR') cmd({ c: 'weld' });
  if (e.code === 'Tab') {
    e.preventDefault();
    cycleTarget();
  }
  if (e.code === 'Escape' && snap) {
    menu.togglePause();
    updatePause();
  }
});

window.addEventListener('keyup', (e) => held.delete(e.code));
window.addEventListener('blur', () => held.clear());

/**
 * A backgrounded tab gets throttled, so pausing avoids a lurch on return — but only
 * when nobody is depending on this tab. With crew connected, hiding the host window is
 * normal (it's the main screen, or the pilot alt-tabbed) and freezing the sim would
 * silently kill every station. Rendering stops on its own: rAF doesn't run when hidden.
 */
const updatePause = (): void => {
  send({ m: 'pause', paused: menu.isOpen || (document.hidden && crewCount === 0) });
};
document.addEventListener('visibilitychange', updatePause);

const cycleTarget = (): void => {
  if (!snap || snap.contacts.length === 0) return;
  const ids = snap.contacts.map((c) => c.id);
  const at = ids.indexOf(snap.player.targetId ?? -1);
  cmd({ c: 'target', id: ids[(at + 1) % ids.length]! });
};

let lastThrottle = 0;
let lastTurn = 0;

const pumpInput = (): void => {
  const throttle = (held.has('KeyW') ? 1 : 0) - (held.has('KeyS') ? 1 : 0);
  const turn = (held.has('KeyD') ? 1 : 0) - (held.has('KeyA') ? 1 : 0);

  // Only send on change — a held key is already state on the sim side.
  if (throttle !== lastThrottle) {
    cmd({ c: 'throttle', v: throttle });
    lastThrottle = throttle;
  }
  if (turn !== lastTurn) {
    cmd({ c: 'turn', v: turn });
    lastTurn = turn;
  }
};

// ── HUD ─────────────────────────────────────────────────────────────────────────

const bar = (label: string, v: number, max: number, colour: string): string => {
  const pct = max > 0 ? Math.max(0, Math.min(1, v / max)) * 100 : 0;
  return `<div class="row"><span>${label}</span><div class="bar"><i style="width:${pct}%;background:${colour}"></i></div><b>${Math.round(v)}</b></div>`;
};

const drawHud = (s: Snapshot): void => {
  const p = s.player;
  const target = s.contacts.find((c) => c.id === p.targetId);

  hud.innerHTML = [
    bar('HULL', p.hull, p.maxHull, '#4ade80'),
    bar('SHIELD', p.shield, p.maxShield, '#38bdf8'),
    bar('BEAM', p.beamCharge, 1, '#f59e0b'),
    bar('WARP', p.warpCharge, 1, '#a78bfa'),
    `<div class="row"><span>SPD</span><b>${Math.round(p.speed)}</b><span>HDG</span><b>${Math.round(((p.heading * 180) / Math.PI + 360) % 360)}</b><span>TORP</span><b>${p.torpedoAmmo}</b><span>CR</span><b>${p.credits}</b></div>`,
    s.mode === 'skirmish' ? `<div class="obj">SKIRMISH — WAVE ${s.skirmishWave}</div>` : '',
    s.objective && s.mode === 'campaign'
      ? `<div class="obj">OBJECTIVE: ${s.objective.name} — ${(s.objective.range / 1000).toFixed(1)} km${s.objective.offered ? ' — press E to ACCEPT' : ''}</div>`
      : '',
    s.event ? `<div class="obj">EVENT: ${s.event.kind.toUpperCase()} — ${Math.ceil(s.event.timeLeft)}s</div>` : '',
    s.contract ? `<div class="tgt">${s.contract.text}</div>` : '',
    p.repairTarget ? `<div class="tgt">DAMAGE: ${p.repairTarget.toUpperCase()} offline — R to weld (${p.repairWelds}/3)</div>` : '',
    target
      ? `<div class="tgt ${target.inBeamArc ? 'ok' : ''}">TARGET: ${target.name} — hull ${Math.round(target.hull)} — ${(target.range / 1000).toFixed(1)} km ${target.inBeamArc ? '[IN ARC]' : '[NO SOLUTION]'}</div>`
      : '<div class="tgt">NO TARGET — press TAB</div>',
    p.docked ? '<div class="obj">DOCKED — repaired and resupplied</div>' : '',

  ].join('');

  commsEl.innerHTML = s.comms
    .slice(-5)
    .map((c) => `<p><b>${c.sender}</b> ${c.text}</p>`)
    .join('');
};

// ── Frame loop ──────────────────────────────────────────────────────────────────

let lastFrame = performance.now();

const frame = (): void => {
  requestAnimationFrame(frame);

  const now = performance.now();
  const dt = Math.min((now - lastFrame) / 1000, 0.1);
  lastFrame = now;

  if (!menu.isOpen) pumpInput();
  if (snap) {
    view.update(snap, dt);
    audio.setThrottle(Math.abs(snap.player.speed) / Math.max(1, snap.player.maxSpeed));
    drawHud(snap);
  }
};

const boot = async (): Promise<void> => {
  try {
    await view.load();
  } catch (err0) {
    const err = err0;
    // Asset failures are the most likely thing to go wrong on a fresh checkout
    // (forgot `npm run assets`), so say so on the page instead of hanging on the
    // splash — that silence is expensive to debug from a screenshot.
    bootEl.innerHTML = `<h1>PROXIMA</h1><p style="color:#f87171">asset load failed — did you run <code>npm run assets</code>?</p><pre style="color:#7d9dc4;font-size:11px">${String(err)}</pre>`;
    throw err;
  }
  bootEl.remove();

  // Offer CONTINUE only if there is something to continue.
  latestSave = await loadCampaign();
  menu.setSave(latestSave);
  menu.showMain();

  frame();
};

void boot();
