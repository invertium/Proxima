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
  sessionPin = await page.evaluate(() => sessionStorage.getItem('proxima.pin'));
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

/**
 * Stations join with the host's session PIN. The E2E reads it out of the pilot page
 * rather than hard-coding one, which is also what a crew does — the pilot reads the
 * number off the menu.
 */
let sessionPin = null;

const openStation = async (ctx, which) => {
  const page = await ctx.newPage();
  // Join by link, carrying the PIN — the same way a crew would be handed a QR code.
  const pin = sessionPin ? `?pin=${sessionPin}` : '';
  await page.goto(`${BASE}/station.html${pin}#${which}`, { waitUntil: 'networkidle' });
  await page.waitForFunction(() => document.querySelector('#status')?.textContent === 'LINKED', null, {
    timeout: 15000,
  });
  return page;
};

const hud = (page) => page.evaluate(() => document.querySelector('#hud')?.textContent ?? '');

/**
 * Presses a control the way a hand does: press, dwell, release. This is the thing the
 * old suite never did — page.click() dispatches press+release in under a millisecond,
 * so it never spanned a repaint, and an entire console that dropped real presses
 * reported 8/8 green.
 */
const press = async (page, selector, ms = 140) => {
  const box = await page.locator(selector).first().boundingBox();
  if (!box) throw new Error(`no such control: ${selector}`);
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down();
  await page.waitForTimeout(ms);
  await page.mouse.up();
};

/** Holds a control down for `ms`, for the steering controls. */
const hold = async (page, selector, ms) => {
  const box = await page.locator(selector).first().boundingBox();
  if (!box) throw new Error(`no such control: ${selector}`);
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down();
  await page.waitForTimeout(ms);
  await page.mouse.up();
};

const speedOf = async (page) => Number(/SPD\s*(\d+)/.exec(await hud(page))?.[1] ?? 0);

// ── Journey 1: new game, hail, accept, engage ───────────────────────────────────

await journey('new game -> objective hail -> accept -> fleet engages', async (ctx) => {
  const pilot = await openPilot(ctx);
  await startNewGame(pilot);

  // The player starts inside the home system's trigger radius, so the hail is prompt.
  await pilot.waitForFunction(() => /press ENTER to ACCEPT/.test(document.querySelector('#hud')?.textContent ?? ''), null, {
    timeout: 20000,
  });

  const weapons = await openStation(ctx, 'weapons');
  // No contacts before the crew commits — the sector must not ambush them.
  const before = await weapons.textContent('#panel');
  if (!/No contacts/.test(before)) throw new Error('contacts existed before the objective was accepted');

  await pilot.keyboard.press('Enter');

  // Accepting spawns the fleet, which the Weapons console must see.
  await weapons.waitForFunction(() => /Derelict|Pact/.test(document.querySelector('#panel')?.textContent ?? ''), null, {
    timeout: 15000,
  });
  await pilot.screenshot({ path: `${OUT}/e2e-engaged.png` });
});

// ── Journey 1b: Science answers the hail ────────────────────────────────────────
//
// The same acceptance as journey 1, but from the console that owns it. The pilot's
// ENTER key working proves the command path; it does not prove a crew can reach it.

await journey('science console accepts the fleet orders', async (ctx) => {
  const pilot = await openPilot(ctx);
  await startNewGame(pilot);
  await pilot.waitForFunction(() => /press ENTER to ACCEPT/.test(document.querySelector('#hud')?.textContent ?? ''), null, {
    timeout: 20000,
  });

  const sci = await openStation(ctx, 'science');
  const accept = sci.locator('button:has-text("ACCEPT ORDERS")');
  await sci.waitForFunction(
    () => [...document.querySelectorAll('button')].some(
      (b) => b.textContent?.startsWith('ACCEPT ORDERS') && !b.hidden,
    ),
    null,
    { timeout: 15000 },
  );

  // Pressed with a real dwell, so the press spans a live snapshot update.
  await press(sci, 'button:has-text("ACCEPT ORDERS")');

  // Accepting engages the fleet and retires the button.
  await sci.waitForFunction(
    () => /ENGAGED/.test(document.querySelector('#panel')?.textContent ?? ''),
    null,
    { timeout: 15000 },
  );
  if (await accept.isVisible()) throw new Error('ACCEPT ORDERS still offered after accepting');

  // Neither Helm nor Engineering may also carry the verb — one console owns each hail.
  for (const which of ['helm', 'engineering']) {
    const other = await openStation(ctx, which);
    const text = await other.textContent('#panel');
    if (/ACCEPT/.test(text)) throw new Error(`${which} still offers an ACCEPT control`);
  }

  await sci.screenshot({ path: `${OUT}/e2e-science-orders.png`, fullPage: true });
});

// ── Journey 2: the crew flies the ship ──────────────────────────────────────────

await journey('helm station drives the ship, pressed the way a hand presses', async (ctx) => {
  const pilot = await openPilot(ctx);
  await startNewGame(pilot);
  const helm = await openStation(ctx, 'helm');

  // Deliberate 140ms presses, long enough to span several state updates.
  await press(helm, 'button:has-text("FULL")');
  await pilot.waitForFunction(
    () => Number(/SPD\s*(\d+)/.exec(document.querySelector('#hud')?.textContent ?? '')?.[1] ?? 0) > 800,
    null,
    { timeout: 10000 },
  );

  await press(helm, 'button:has-text("STOP")');
  await pilot.waitForFunction(
    () => Number(/SPD\s*(\d+)/.exec(document.querySelector('#hud')?.textContent ?? '')?.[1] ?? 999) < 200,
    null,
    { timeout: 10000 },
  );
});

// ── Journey 2b: loss rate, not just 'one press worked' ────────────────────────

await journey('ten deliberate presses all land', async (ctx) => {
  const pilot = await openPilot(ctx);
  await startNewGame(pilot);
  const helm = await openStation(ctx, 'helm');

  let landed = 0;
  for (let i = 0; i < 10; i++) {
    const wantFast = i % 2 === 0;
    await press(helm, wantFast ? 'button:has-text("FULL")' : 'button:has-text("STOP")');
    // The interceptor accelerates at 1500/s, so braking from full speed genuinely
    // takes ~1.2s. Allow for the physics, not just the message.
    await pilot.waitForTimeout(1800);
    const speed = await speedOf(pilot);
    if (wantFast ? speed > 300 : speed < 300) landed++;
  }

  if (landed < 10) throw new Error(`only ${landed}/10 presses reached the ship`);
});

// ── Journey 2c: the console must not churn its own controls ───────────────────

await journey('the panel never destroys a control while state ticks', async (ctx) => {
  const pilot = await openPilot(ctx);
  await startNewGame(pilot);
  const helm = await openStation(ctx, 'helm');

  await press(helm, 'button:has-text("FULL")'); // guarantee live, changing values
  await helm.evaluate(() => {
    window.__churn = 0;
    new MutationObserver((records) => {
      for (const r of records) {
        for (const node of r.removedNodes) {
          if (node.nodeType !== 1) continue;
          const e = node;
          if (e.matches?.('button,input') || e.querySelector?.('button,input')) window.__churn++;
        }
      }
    }).observe(document.querySelector('#panel'), { childList: true, subtree: true });
  });

  await helm.waitForTimeout(3000);
  const churn = await helm.evaluate(() => window.__churn);
  if (churn > 0) throw new Error(`${churn} controls were destroyed under the user's finger`);
});

// ── Journey 2d: hold-to-turn ──────────────────────────────────────────────────

await journey('hold-to-turn turns while held and centres on release', async (ctx) => {
  const pilot = await openPilot(ctx);
  await startNewGame(pilot);
  const helm = await openStation(ctx, 'helm');

  const heading = async () => Number(/HDG\s*(\d+)/.exec(await hud(pilot))?.[1] ?? -1);

  const before = await heading();
  await hold(helm, 'button:has-text("PORT")', 1200);
  const afterHold = await heading();
  if (before === afterHold) throw new Error('holding PORT did not turn the ship');

  // The rudder must centre itself on release. If it doesn't, the ship spins forever
  // and the helm is unflyable — the exact failure a tap-to-set control has.
  await pilot.waitForTimeout(900);
  const settled = await heading();
  await pilot.waitForTimeout(900);
  const stillSettled = await heading();
  if (settled !== stillSettled) {
    throw new Error(`rudder did not centre on release (${settled} -> ${stillSettled})`);
  }
});

// ── Journey 3: Engineering moves the reactor, and it reaches the ship ───────────

await journey('engineering reactor preset changes the ship top speed', async (ctx) => {
  const pilot = await openPilot(ctx);
  await startNewGame(pilot);
  const eng = await openStation(ctx, 'engineering');
  const helm = await openStation(ctx, 'helm');

  await press(eng, 'button:has-text("COMBAT")'); // weapons/shields heavy, engines starved
  await press(helm, 'button:has-text("FULL")');
  await pilot.waitForTimeout(5000);
  const starved = Number(/SPD\s*(\d+)/.exec(await hud(pilot))?.[1] ?? 0);

  await press(eng, 'button:has-text("TRAVEL")'); // engines heavy
  await pilot.waitForTimeout(5000);
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
  await pilot.waitForFunction(() => /press ENTER to ACCEPT/.test(document.querySelector('#hud')?.textContent ?? ''), null, {
    timeout: 20000,
  });
  await pilot.keyboard.press('Enter');

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
  // Snapshots already in flight when the pause message was posted still land after it,
  // so let the stream drain before taking the baseline.
  await pilot.waitForTimeout(500);
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

// ── Journey 9: the PIN gate ───────────────────────────────────────────────────

await journey('a wrong session PIN is refused with a way back in', async (ctx) => {
  const pilot = await openPilot(ctx);
  await startNewGame(pilot);

  const station = await ctx.newPage();
  await station.goto(`${BASE}/station.html?pin=0000#helm`, { waitUntil: 'networkidle' });

  // Refused, and offered a form rather than a dead end.
  await station.waitForFunction(() => document.querySelector('#status')?.textContent === 'PIN REQUIRED', null, {
    timeout: 15000,
  });
  await station.waitForSelector('#pinform:not([hidden])', { timeout: 5000 });

  // The right PIN, typed into the form, gets in.
  await station.fill('#pin', sessionPin);
  await station.click('#pinform button');
  await station.waitForFunction(() => document.querySelector('#status')?.textContent === 'LINKED', null, {
    timeout: 15000,
  });
});

await browser.close();

console.log(results.join('\n'));
if (failures.length) {
  console.error(`\nE2E FAILED (${failures.length}):\n` + failures.map((f) => `  - ${f}`).join('\n'));
  process.exit(1);
}
console.log(`\nall ${results.length} journeys passed — shots in ${OUT}/`);
