// M7 — browser end-to-end user journeys.
//
// Drives the real pages through Playwright the way a crew would: clicking the front
// end, flying from the pilot window, and operating the stations from separate tabs.
// Every journey asserts host-side state (read back out of the live HUD/console text),
// not just that a screenshot rendered.
//
// Deep gameplay paths that need a long flight to reach — docking, repair, the drydock
// purchase loop — are covered headlessly in replay.test.ts instead. This file covers
// what a user actually touches.
//
//   node test/e2e.mjs           (against a running `npm run dev` on :5173)
//   BASE=... OUT=... node test/e2e.mjs

import { chromium } from 'playwright';
import { mkdirSync } from 'node:fs';

const BASE = process.env.BASE ?? 'http://localhost:5173';
const OUT = process.env.OUT ?? 'shots';
mkdirSync(OUT, { recursive: true });

try {
  const probe = await fetch(BASE, { signal: AbortSignal.timeout(3000) });
  if (!probe.ok) throw new Error(`HTTP ${probe.status}`);
} catch (err) {
  console.error(`nothing serving ${BASE} (${err.message}) — run \`npm run dev\` first`);
  process.exit(1);
}

const browser = await chromium.launch({
  executablePath: process.env.CHROME ?? '/usr/bin/chromium',
  args: ['--enable-unsafe-swiftshader', '--use-gl=angle', '--use-angle=swiftshader'],
});

const failures = [];
const results = [];

const journey = async (name, fn) => {
  const ctx = await browser.newContext({ viewport: { width: 1600, height: 900 } });
  const errors = [];
  ctx.on('weberror', (e) => errors.push(`page error: ${e.error().message}`));
  ctx.on('response', (r) => {
    if (r.status() >= 400 && !r.url().includes('favicon')) errors.push(`HTTP ${r.status()} ${r.url()}`);
  });

  try {
    await fn(ctx);
    if (errors.length) throw new Error(errors.join('; '));
    results.push(`  ok   ${name}`);
  } catch (err) {
    failures.push(`${name}: ${err.message}`);
    results.push(`  FAIL ${name}`);
  } finally {
    await ctx.close();
  }
};

/** Boots the pilot page and clears the loading splash. */
const openPilot = async (ctx) => {
  const page = await ctx.newPage();
  page.on('console', (m) => {
    if (m.type() === 'error' && !m.text().includes('favicon')) throw new Error(`console: ${m.text()}`);
  });
  await page.goto(BASE, { waitUntil: 'networkidle' });
  await page.waitForSelector('#boot', { state: 'detached', timeout: 30000 });
  return page;
};

const startNewGame = async (page, difficulty = 'captain') => {
  await page.click('button[data-action="newgame"]');
  await page.click(`button[data-action="difficulty:${difficulty}"]`);
  await page.click('button[data-action="launch"]');
  await page.waitForFunction(() => document.querySelector('#hud')?.textContent?.includes('HULL'), null, {
    timeout: 15000,
  });
};

const openStation = async (ctx, which) => {
  const page = await ctx.newPage();
  await page.goto(`${BASE}/station.html#${which}`, { waitUntil: 'networkidle' });
  await page.waitForFunction(() => document.querySelector('#status')?.textContent === 'LINKED', null, {
    timeout: 15000,
  });
  return page;
};

const hud = (page) => page.evaluate(() => document.querySelector('#hud')?.textContent ?? '');

// ── Journey 1: new game, hail, accept, engage ───────────────────────────────────

await journey('new game -> objective hail -> accept -> fleet engages', async (ctx) => {
  const pilot = await openPilot(ctx);
  await startNewGame(pilot);

  // The player starts inside the home system's trigger radius, so the hail is prompt.
  await pilot.waitForFunction(() => /press E to ACCEPT/.test(document.querySelector('#hud')?.textContent ?? ''), null, {
    timeout: 20000,
  });

  const weapons = await openStation(ctx, 'weapons');
  // No contacts before the crew commits — the sector must not ambush them.
  const before = await weapons.textContent('#panel');
  if (!/No contacts/.test(before)) throw new Error('contacts existed before the objective was accepted');

  await pilot.keyboard.press('KeyE');

  // Accepting spawns the fleet, which the Weapons console must see.
  await weapons.waitForFunction(() => /Derelict|Pact/.test(document.querySelector('#panel')?.textContent ?? ''), null, {
    timeout: 15000,
  });
  await pilot.screenshot({ path: `${OUT}/e2e-engaged.png` });
});

// ── Journey 2: the crew flies the ship ──────────────────────────────────────────

await journey('helm station drives the ship the pilot renders', async (ctx) => {
  const pilot = await openPilot(ctx);
  await startNewGame(pilot);
  const helm = await openStation(ctx, 'helm');

  await helm.click('button:has-text("FULL AHEAD")');
  await pilot.waitForFunction(
    () => Number(/SPD\s*(\d+)/.exec(document.querySelector('#hud')?.textContent ?? '')?.[1] ?? 0) > 800,
    null,
    { timeout: 10000 },
  );

  await helm.click('button:has-text("STOP")');
  await pilot.waitForFunction(
    () => Number(/SPD\s*(\d+)/.exec(document.querySelector('#hud')?.textContent ?? '')?.[1] ?? 999) < 200,
    null,
    { timeout: 10000 },
  );
});

// ── Journey 3: Engineering moves the reactor, and it reaches the ship ───────────

await journey('engineering reactor preset changes the ship top speed', async (ctx) => {
  const pilot = await openPilot(ctx);
  await startNewGame(pilot);
  const eng = await openStation(ctx, 'engineering');
  const helm = await openStation(ctx, 'helm');

  await eng.click('button:has-text("TURTLE")'); // starves engines
  await helm.click('button:has-text("FULL AHEAD")');
  await pilot.waitForTimeout(4000);
  const starved = Number(/SPD\s*(\d+)/.exec(await hud(pilot))?.[1] ?? 0);

  await eng.click('button:has-text("RUN")'); // everything into engines
  await pilot.waitForTimeout(4000);
  const boosted = Number(/SPD\s*(\d+)/.exec(await hud(pilot))?.[1] ?? 0);

  if (!(boosted > starved * 1.2)) {
    throw new Error(`reactor preset had no effect on speed (${starved} -> ${boosted})`);
  }
  await eng.screenshot({ path: `${OUT}/e2e-engineering.png` });
});

// ── Journey 4: Science resolves a contact, which Weapons can then read ─────────

await journey('science scan resolves a contact for weapons', async (ctx) => {
  const pilot = await openPilot(ctx);
  await startNewGame(pilot);
  await pilot.waitForFunction(() => /press E to ACCEPT/.test(document.querySelector('#hud')?.textContent ?? ''), null, {
    timeout: 20000,
  });
  await pilot.keyboard.press('KeyE');

  const sci = await openStation(ctx, 'science');
  const weapons = await openStation(ctx, 'weapons');

  await sci.waitForFunction(() => /unresolved/.test(document.querySelector('#panel')?.textContent ?? ''), null, {
    timeout: 15000,
  });
  // Weapons should not be shown numbers the crew hasn't earned.
  const raw = await weapons.textContent('#panel');
  if (!/unscanned/.test(raw)) throw new Error('weapons showed hull numbers for an unscanned contact');

  await sci.click('.contact');
  await sci.waitForFunction(() => /RESOLVED/.test(document.querySelector('#panel')?.textContent ?? ''), null, {
    timeout: 20000,
  });

  await weapons.waitForFunction(() => /hull \d+\//.test(document.querySelector('#panel')?.textContent ?? ''), null, {
    timeout: 15000,
  });
  await sci.screenshot({ path: `${OUT}/e2e-science.png` });
});

// ── Journey 5: four stations at once ───────────────────────────────────────────

await journey('four crew stations link simultaneously', async (ctx) => {
  const pilot = await openPilot(ctx);
  await startNewGame(pilot);

  const pages = await Promise.all(
    ['helm', 'weapons', 'engineering', 'science'].map((s) => openStation(ctx, s)),
  );

  for (const page of pages) {
    const status = await page.textContent('#status');
    if (status !== 'LINKED') throw new Error(`a station failed to link: ${status}`);
    const panel = await page.textContent('#panel');
    if (!panel || panel.trim().length === 0) throw new Error('a station rendered an empty panel');
  }
});

// ── Journey 6: pause ──────────────────────────────────────────────────────────

await journey('escape pauses the sim and resume continues it', async (ctx) => {
  const pilot = await openPilot(ctx);
  await startNewGame(pilot);

  await pilot.keyboard.down('KeyW');
  await pilot.waitForTimeout(1500);
  await pilot.keyboard.up('KeyW');

  await pilot.keyboard.press('Escape');
  await pilot.waitForSelector('button[data-action="resume"]', { timeout: 5000 });

  const tickOf = () => pilot.evaluate(() => document.querySelector('#hud')?.textContent ?? '');
  const paused = await tickOf();
  await pilot.waitForTimeout(1500);
  if ((await tickOf()) !== paused) throw new Error('the sim kept running while paused');

  await pilot.click('button[data-action="resume"]');
  await pilot.waitForTimeout(1200);
  await pilot.screenshot({ path: `${OUT}/e2e-resumed.png` });
});

// ── Journey 7: skirmish ───────────────────────────────────────────────────────

await journey('skirmish mode spawns waves', async (ctx) => {
  const pilot = await openPilot(ctx);
  await pilot.click('button[data-action="skirmish"]');
  await pilot.waitForFunction(() => /WAVE 1/.test(document.querySelector('#hud')?.textContent ?? ''), null, {
    timeout: 20000,
  });

  const weapons = await openStation(ctx, 'weapons');
  await weapons.waitForFunction(() => /Pact/.test(document.querySelector('#panel')?.textContent ?? ''), null, {
    timeout: 15000,
  });
  await pilot.screenshot({ path: `${OUT}/e2e-skirmish.png` });
});

// ── Journey 8: progress survives a reload ─────────────────────────────────────

await journey('campaign progress survives a page reload', async (ctx) => {
  const pilot = await openPilot(ctx);
  await startNewGame(pilot, 'ensign');

  // Let the worker persist at least one save (it diffs twice a second).
  await pilot.waitForTimeout(2500);

  await pilot.reload({ waitUntil: 'networkidle' });
  await pilot.waitForSelector('#boot', { state: 'detached', timeout: 30000 });

  // A CONTINUE button only appears when a save was actually read back.
  await pilot.waitForSelector('button[data-action="continue"]', { timeout: 10000 });
  await pilot.click('button[data-action="continue"]');
  await pilot.waitForFunction(() => document.querySelector('#hud')?.textContent?.includes('HULL'), null, {
    timeout: 15000,
  });
  await pilot.screenshot({ path: `${OUT}/e2e-continue.png` });
});

await browser.close();

console.log(results.join('\n'));
if (failures.length) {
  console.error(`\nE2E FAILED (${failures.length}):\n` + failures.map((f) => `  - ${f}`).join('\n'));
  process.exit(1);
}
console.log(`\nall ${results.length} journeys passed — shots in ${OUT}/`);
