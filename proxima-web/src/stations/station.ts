// Crew console entry point: transport, tab routing, the render loop, link state.
//
// Panels are created once and never destroyed — switching tabs hides one and shows
// another, so scroll position, slider drags and focus all survive. See ui/dom.ts for
// why that matters more than it sounds.

import { RelayStation } from '../net/transport';
import { Scope } from './ui/scope';
import { createHelmPanel } from './panels/helm';
import { createWeaponsPanel } from './panels/weapons';
import { createEngineeringPanel } from './panels/engineering';
import { createSciencePanel } from './panels/science';
import { createFooter } from './panels/common';
import type { Panel } from './panels/panel';
import type { Command, Snapshot, Station } from '../sim/types';

const canvas = document.getElementById('radar') as HTMLCanvasElement;
const panelHost = document.getElementById('panel') as HTMLDivElement;
const tabs = document.getElementById('tabs') as HTMLDivElement;
const statusEl = document.getElementById('status') as HTMLDivElement;
const noticeEl = document.getElementById('notice') as HTMLDivElement;
const alertEl = document.getElementById('alert') as HTMLButtonElement;

const link = new RelayStation();
const send = (cmd: Command): void => link.send({ m: 'cmd', cmd });
const scope = new Scope(canvas);
const footer = createFooter((action) => link.send({ m: 'game', action }));
document.querySelector('.wrap')!.appendChild(footer.root);

let snap: Snapshot | null = null;
let lastSnapAt = 0;
let connected = false;
let dirty = false;
let showMap = false;
let station: Station = (location.hash.slice(1) as Station) || 'helm';

const toggleMap = (): void => {
  showMap = !showMap;
  dirty = true;
};

// Built lazily on first activation, then kept forever.
const factories: Record<Station, () => Panel> = {
  helm: () => createHelmPanel(send, toggleMap),
  weapons: () => createWeaponsPanel(send),
  engineering: () => createEngineeringPanel(send),
  science: () => createSciencePanel(send, toggleMap),
};
const panels = new Map<Station, Panel>();

const panelFor = (which: Station): Panel => {
  let panel = panels.get(which);
  if (!panel) {
    panel = factories[which]();
    panels.set(which, panel);
    panelHost.appendChild(panel.root);
  }
  return panel;
};

const activate = (which: Station): void => {
  station = which;
  panelFor(which);
  for (const [name, panel] of panels) panel.root.hidden = name !== which;
  for (const b of tabs.querySelectorAll('[data-station]')) {
    b.classList.toggle('sel', (b as HTMLElement).dataset['station'] === which);
  }
  dirty = true;
};

link.onMessage((msg) => {
  if (msg.m !== 'state') return;
  snap = msg.snapshot;
  lastSnapAt = performance.now();
  dirty = true;
});

link.onStatus((isConnected) => {
  connected = isConnected;
  dirty = true;
});

/**
 * Three states, not two. "The relay accepted my socket" is not the same as "there is a
 * ship to fly" — a crew member staring at a dead console needs to know which end to go
 * and fix, and the old console showed a confident green LINKED next to an empty panel.
 */
const updateLinkState = (): void => {
  const fresh = snap !== null && performance.now() - lastSnapAt < 2000;

  if (!connected) {
    statusEl.textContent = 'NO LINK — RECONNECTING…';
    statusEl.className = '';
    noticeEl.textContent = 'Cannot reach the ship. Check you are on the same network as the host.';
    noticeEl.hidden = false;
  } else if (!fresh) {
    statusEl.textContent = 'LINKED — WAITING FOR SHIP';
    statusEl.className = 'warn';
    noticeEl.textContent =
      'Connected, but nothing is flying yet. On the host machine, open the game and press LAUNCH.';
    noticeEl.hidden = false;
  } else {
    statusEl.textContent = 'LINKED';
    statusEl.className = 'ok';
    noticeEl.hidden = true;
  }

  panelHost.hidden = !fresh;
  canvas.hidden = !fresh;
};

alertEl.addEventListener('click', () => send({ c: 'alert', state: 'toggle' }));

tabs.addEventListener('click', (e) => {
  const btn = (e.target as HTMLElement).closest('[data-station]') as HTMLElement | null;
  if (!btn) return;
  const which = btn.dataset['station'] as Station;
  location.hash = which;
  activate(which);
});

window.addEventListener('hashchange', () => {
  activate((location.hash.slice(1) as Station) || 'helm');
});

/**
 * One update per frame, not one per snapshot. Snapshots arrive at up to 60 Hz and must
 * coalesce, or a phone spends its whole budget on state it will never display.
 */
const frame = (): void => {
  requestAnimationFrame(frame);
  updateLinkState();

  if (!snap || !dirty) return;
  dirty = false;

  // Alert doctrine is ship-wide: show it on every console and tint the whole page,
  // so the state is readable from across a room.
  const red = snap.alert === 'red';
  alertEl.hidden = false;
  alertEl.textContent = red ? 'RED ALERT — STAND DOWN' : 'SOUND RED ALERT';
  alertEl.classList.toggle('red', red);
  document.body.classList.toggle('red', red);

  panelFor(station).update(snap);
  footer.update(snap);
  scope.draw(snap, showMap && (station === 'helm' || station === 'science') ? 'map' : 'tactical');
};

activate(station);
frame();
link.send({ m: 'join', station, version: 1 });
