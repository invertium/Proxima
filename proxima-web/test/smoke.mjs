// Browser smoke test: boots the host, waits for the sim to actually produce state,
// drives the pilot controls, and captures both the pilot view and a crew console.
//
// This is the visual-verification path that replaces driving the Unreal editor over
// MCP: `node test/smoke.mjs` against a running `npm run dev`.

import { chromium } from 'playwright';
import { mkdirSync } from 'node:fs';

const BASE = process.env.BASE ?? 'http://localhost:5174';
const OUT = process.env.OUT ?? 'shots';
mkdirSync(OUT, { recursive: true });

// Uses the system Chromium by default so CI and a fresh checkout don't have to
// download a browser; set CHROME= to point at another binary.
const browser = await chromium.launch({
  executablePath: process.env.CHROME ?? '/usr/bin/chromium',
  args: ['--enable-unsafe-swiftshader', '--use-gl=angle', '--use-angle=swiftshader'],
});
const ctx = await browser.newContext({ viewport: { width: 1600, height: 900 } });

const fail = [];
ctx.on('weberror', (e) => fail.push(`page error: ${e.error().message}`));
// A missing favicon is noise; a missing asset is a real failure, so report the URL.
ctx.on('response', (r) => {
  if (r.status() >= 400 && !r.url().includes('favicon')) fail.push(`HTTP ${r.status()} ${r.url()}`);
});

const pilot = await ctx.newPage();
pilot.on('console', (m) => {
  if (m.type() === 'error' && !m.text().includes('favicon')) fail.push(`console: ${m.text()}`);
});

await pilot.goto(BASE, { waitUntil: 'networkidle' });

// The splash only clears once every ship model has loaded.
await pilot.waitForSelector('#boot', { state: 'detached', timeout: 30000 });

// Wait for the worker to deliver real state, not just for the DOM to exist.
await pilot.waitForFunction(() => document.querySelector('#hud')?.textContent?.includes('HULL'), null, {
  timeout: 15000,
});

// Fly: full ahead, hard to starboard, then take a shot.
await pilot.keyboard.down('KeyW');
await pilot.waitForTimeout(2500);
await pilot.keyboard.down('KeyD');
await pilot.waitForTimeout(1500);
await pilot.keyboard.up('KeyD');
await pilot.waitForTimeout(2000);

const speed = await pilot.evaluate(() => document.querySelector('#hud')?.textContent ?? '');
if (!/SPD/.test(speed)) fail.push('HUD never reported speed');

await pilot.screenshot({ path: `${OUT}/pilot.png` });

// A crew station in a second tab, linked over BroadcastChannel.
const station = await ctx.newPage();
await station.goto(`${BASE}/station.html#weapons`, { waitUntil: 'networkidle' });
await station.waitForFunction(() => document.querySelector('#status')?.textContent === 'LINKED', null, {
  timeout: 15000,
});
await station.waitForTimeout(1200);
await station.screenshot({ path: `${OUT}/station-weapons.png` });

await station.goto(`${BASE}/station.html#helm`, { waitUntil: 'networkidle' });
await station.waitForFunction(() => document.querySelector('#status')?.textContent === 'LINKED', null, {
  timeout: 15000,
});

// The crew loop that matters: a button on a phone must move the ship the host renders.
await pilot.keyboard.up('KeyW');
await station.click('button:has-text("STOP")');
await pilot.waitForFunction(
  () => /SPD.*?\b0\b/.test(document.querySelector('#hud')?.textContent ?? ''),
  null,
  { timeout: 10000 },
);

await station.click('button:has-text("FULL AHEAD")');
await pilot.waitForFunction(
  () => {
    const m = /SPD\s*(\d+)/.exec(document.querySelector('#hud')?.textContent ?? '');
    return m && Number(m[1]) > 500;
  },
  null,
  { timeout: 10000 },
);

await station.waitForTimeout(800);
await station.screenshot({ path: `${OUT}/station-helm.png` });

await browser.close();

if (fail.length) {
  console.error('SMOKE FAILED:\n' + fail.join('\n'));
  process.exit(1);
}
console.log(`smoke ok — shots in ${OUT}/`);
