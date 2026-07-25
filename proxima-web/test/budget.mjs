// M7 — performance and size budgets.
//
// Two things that degrade silently unless something watches them: the bundle a phone
// has to download, and the frame time under a full fight. Both fail the build rather
// than being noticed months later.
//
//   npm run build && node test/budget.mjs

import { chromium } from 'playwright';
import { gzipSync } from 'node:zlib';
import { readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';

const BASE = process.env.BASE ?? 'http://localhost:8099';

/**
 * Gzipped ceilings, in KB, set just above current so a regression trips them. The
 * station figure is the one that matters — it's what a phone downloads — and it counts
 * the shared sim/data chunks it pulls in, not just its own.
 */
const BUDGETS = { host: 200, station: 20 };
/** Median frame time under a full fight, in ms. 16.7 is 60fps; software rendering in CI is slower. */
const FRAME_BUDGET_MS = 60;

const failures = [];

// ── Bundle size ────────────────────────────────────────────────────────────────

const assets = join('dist', 'assets');
const sizes = { host: 0, station: 0 };

for (const file of readdirSync(assets)) {
  if (!file.endsWith('.js')) continue;
  const kb = gzipSync(readFileSync(join(assets, file))).length / 1024;
  // The station page pulls only its own chunk plus the shared transport; the host
  // additionally pulls Three.js.
  if (file.startsWith('host')) sizes.host += kb;
  else if (file.startsWith('station')) sizes.station += kb;
  else {
    sizes.host += kb;
    sizes.station += kb;
  }
}

for (const [name, budget] of Object.entries(BUDGETS)) {
  const kb = sizes[name];
  const verdict = kb <= budget ? 'ok  ' : 'FAIL';
  console.log(`  ${verdict} ${name} bundle ${kb.toFixed(1)} KB gzipped (budget ${budget})`);
  if (kb > budget) failures.push(`${name} bundle ${kb.toFixed(1)} KB exceeds ${budget} KB`);
}

// ── Frame time under load ──────────────────────────────────────────────────────

const browser = await chromium.launch({
  executablePath: process.env.CHROME ?? '/usr/bin/chromium',
  args: ['--enable-unsafe-swiftshader', '--use-gl=angle', '--use-angle=swiftshader'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 900 } });
await page.goto(BASE, { waitUntil: 'networkidle' });
await page.waitForSelector('#boot', { state: 'detached', timeout: 30000 });

// Skirmish reaches a full fight fastest, which is the load worth measuring.
await page.click('button[data-action="skirmish"]');
await page.waitForFunction(() => /WAVE 1/.test(document.querySelector('#hud')?.textContent ?? ''), null, {
  timeout: 20000,
});
await page.waitForTimeout(4000); // let hostiles close and start shooting

const frames = await page.evaluate(
  () =>
    new Promise((resolve) => {
      const samples = [];
      let last = performance.now();
      const tick = () => {
        const now = performance.now();
        samples.push(now - last);
        last = now;
        if (samples.length < 120) requestAnimationFrame(tick);
        else resolve(samples);
      };
      requestAnimationFrame(tick);
    }),
);

frames.sort((a, b) => a - b);
const median = frames[Math.floor(frames.length / 2)];
const p95 = frames[Math.floor(frames.length * 0.95)];

const frameVerdict = median <= FRAME_BUDGET_MS ? 'ok  ' : 'FAIL';
console.log(`  ${frameVerdict} frame time median ${median.toFixed(1)} ms, p95 ${p95.toFixed(1)} ms (budget ${FRAME_BUDGET_MS})`);
if (median > FRAME_BUDGET_MS) failures.push(`median frame ${median.toFixed(1)} ms exceeds ${FRAME_BUDGET_MS} ms`);

await browser.close();

if (failures.length) {
  console.error('\nBUDGET FAILED:\n' + failures.map((f) => `  - ${f}`).join('\n'));
  process.exit(1);
}
console.log('\nbudgets ok');
