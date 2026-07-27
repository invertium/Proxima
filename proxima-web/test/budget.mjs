// M7 — performance and size budgets.
//
// Two things that degrade silently unless something watches them: the bundle a phone
// has to download, and the frame time under a full fight. Both fail the build rather
// than being noticed months later.
//
//   bun run build && bun run budget
//
// Starts its own `vite preview` if nothing is already serving BASE, so it can be the
// last link in `bun run verify` without a second server to remember.
//
// The React/Bun migration deleted this file rather than renegotiating it, which is the
// one move a budget cannot survive: the ceilings existed precisely so a stack change
// would have to argue for the bytes it costs. It is back, with the argument made
// explicit in BUDGETS below.

import { chromium } from 'playwright';
import { spawn } from 'node:child_process';
import { gzipSync } from 'node:zlib';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { setTimeout as sleep } from 'node:timers/promises';

const PORT = Number(process.env.PORT ?? 8099);
const BASE = process.env.BASE ?? `http://localhost:${PORT}`;
const DIST = 'dist';

const serving = async () => {
  try {
    return (await fetch(BASE, { signal: AbortSignal.timeout(1500) })).ok;
  } catch {
    return false;
  }
};

/**
 * Gzipped ceilings, in KB, set just above current so a regression trips them.
 *
 * These are NOT the pre-React numbers (180 host / 22 station). React 19 + Base UI +
 * the icon set cost roughly 85 KB gzipped that the hand-rolled DOM did not, and the
 * shared vendor chunk is paid by both pages. That trade bought back ~20 ms a frame on
 * the pilot view — see FRAME_BUDGET_MS — and a component model the consoles can grow
 * into. It is a deliberate purchase, recorded here so the next one has to be argued
 * for too rather than merely happening.
 *
 * The station figure is still the one that matters: it is what a phone on the LAN
 * downloads before a crew member can press anything.
 */
const BUDGETS = { host: 270, station: 115 };

/**
 * Median frame time under a full fight, in ms. 16.7 is 60fps; software rendering in CI
 * is much slower, so this is a "did something catastrophic happen" line, not a target.
 * Tightened 60 -> 50 because this branch measures ~37 ms where main measured ~57: main
 * rebuilt the whole HUD's markup every frame. Locking the ceiling below main's actual
 * is what stops that regression coming back unnoticed.
 */
const FRAME_BUDGET_MS = 50;

const failures = [];

// ── Bundle size ────────────────────────────────────────────────────────────────
//
// Measured per PAGE by reading what the built HTML actually references — script src,
// modulepreload, stylesheet — rather than guessing from filename prefixes. Chunk names
// are a build-tool detail that changes under you; the entry HTML is the contract with
// the browser. It also means CSS counts, which under Tailwind is no longer a rounding
// error (~10 KB gzipped, and it was going uncounted).

const assetsOf = (html) => {
  const source = readFileSync(join(DIST, html), 'utf8');
  const refs = new Set();
  for (const [, href] of source.matchAll(/(?:src|href)="(\/assets\/[^"]+\.(?:js|css))"/g)) {
    refs.add(href.replace(/^\//, ''));
  }
  return [...refs];
};

const sizes = {};
for (const [name, html] of Object.entries({ host: 'index.html', station: 'station.html' })) {
  const files = assetsOf(html);
  if (files.length === 0) throw new Error(`${html} references no built assets — did the build run?`);
  sizes[name] = files.reduce((kb, file) => kb + gzipSync(readFileSync(join(DIST, file))).length / 1024, 0);
}

for (const [name, budget] of Object.entries(BUDGETS)) {
  const kb = sizes[name];
  const verdict = kb <= budget ? 'ok  ' : 'FAIL';
  console.log(`  ${verdict} ${name} page ${kb.toFixed(1)} KB gzipped (budget ${budget})`);
  if (kb > budget) failures.push(`${name} page ${kb.toFixed(1)} KB exceeds ${budget} KB`);
}

// ── Frame time under load ──────────────────────────────────────────────────────

let preview = null;
if (!(await serving())) {
  preview = spawn('npx', ['vite', 'preview', '--port', String(PORT), '--strictPort'], {
    stdio: 'ignore',
  });
  for (let i = 0; i < 40 && !(await serving()); i += 1) await sleep(250);
  if (!(await serving())) {
    preview.kill();
    console.error(`could not start a preview server on ${BASE} — run \`bun run build\` first`);
    process.exit(1);
  }
}

const browser = await chromium.launch({
  executablePath: process.env.CHROME ?? '/usr/bin/chromium',
  args: ['--enable-unsafe-swiftshader', '--use-gl=angle', '--use-angle=swiftshader'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 900 } });
await page.goto(BASE, { waitUntil: 'networkidle' });
await page.waitForSelector('#boot', { state: 'detached', timeout: 30000 });

// Skirmish reaches a full fight fastest, which is the load worth measuring.
await page.click('[data-testid="menu-skirmish"]');
await page.waitForFunction(() => /WAVE 1/.test(document.body?.textContent ?? ''), null, {
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
console.log(
  `  ${frameVerdict} frame time median ${median.toFixed(1)} ms, p95 ${p95.toFixed(1)} ms (budget ${FRAME_BUDGET_MS})`,
);
if (median > FRAME_BUDGET_MS) failures.push(`median frame ${median.toFixed(1)} ms exceeds ${FRAME_BUDGET_MS} ms`);

await browser.close();
preview?.kill();

if (failures.length) {
  console.error('\nBUDGET FAILED:\n' + failures.map((f) => `  - ${f}`).join('\n'));
  process.exit(1);
}
console.log('\nbudgets ok');
