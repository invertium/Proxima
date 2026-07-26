// Screenshots one procedural model off the dev server's model bench, for holding up
// against art_src/refs/. Every fidelity defect in these models so far was found by
// looking at a render next to the reference, never by a passing test.
//
//   npm run dev
//   node tools/model-shot.mjs starbase front /tmp/sb.png
//
// model: starbase | haven | rocky | ice | sun     view: front | three-quarter | top

import { chromium } from 'playwright';

const BASE = process.env.BASE ?? 'http://localhost:5173';
const model = process.argv[2] ?? 'starbase';
const view = process.argv[3] ?? 'front';
const out = process.argv[4] ?? `/tmp/${model}-${view}.png`;

const browser = await chromium.launch({
  executablePath: process.env.CHROME ?? '/usr/bin/chromium',
  args: ['--enable-unsafe-swiftshader', '--use-gl=angle', '--use-angle=swiftshader'],
});
const page = await browser.newContext({ viewport: { width: 1500, height: 1000 } }).then((c) => c.newPage());
page.on('pageerror', (e) => console.error('PAGE ERROR:', e.message));
page.on('console', (m) => m.type() === 'error' && console.error('CONSOLE:', m.text()));
await page.goto(`${BASE}/model.html?model=${model}&view=${view}`, { waitUntil: 'networkidle' });
await page.waitForTimeout(1500);
await page.screenshot({ path: out });
console.log('wrote', out);
await browser.close();
