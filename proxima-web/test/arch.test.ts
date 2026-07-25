// Architecture guards.
//
// Two rules this project depends on that were previously only stated in prose. Both
// have already been broken once: the sim-purity rule was documented in the README and
// never checked, and the "controls are never rebuilt" rule did not exist at all, which
// is how a console shipped that no human could press a button on.

import { describe, expect, it } from 'vitest';
import { readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';

/**
 * Comments are stripped before matching: these files document the rules they follow,
 * so `// never call Math.random()` would otherwise trip the check that enforces it.
 */
const stripComments = (source: string): string =>
  source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/(^|[^:])\/\/.*$/gm, '$1');

const read = (dir: string): { path: string; source: string }[] =>
  readdirSync(dir, { withFileTypes: true }).flatMap((entry) => {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) return read(path);
    return entry.name.endsWith('.ts')
      ? [{ path, source: stripComments(readFileSync(path, 'utf8')) }]
      : [];
  });

describe('sim purity', () => {
  it('src/sim imports nothing from three, the DOM, the renderer or the network', () => {
    const banned = /from '(three|\.\.\/render|\.\.\/net|\.\.\/host|\.\.\/stations)/;

    for (const { path, source } of read('src/sim')) {
      expect(banned.test(source), `${path} imports outside the simulation`).toBe(false);
    }
  });

  it('src/sim never reaches for browser globals', () => {
    // performance.now / requestAnimationFrame in here would make the sim untestable
    // headlessly and non-deterministic in a replay.
    const banned = /\b(document|window|localStorage|indexedDB|requestAnimationFrame)\b/;

    for (const { path, source } of read('src/sim')) {
      expect(banned.test(source), `${path} uses a browser global`).toBe(false);
    }
  });

  it('src/sim never calls Math.random — every roll must come from the seeded rng', () => {
    for (const { path, source } of read('src/sim')) {
      expect(/Math\.random\(/.test(source), `${path} breaks replay determinism`).toBe(false);
    }
  });
});

describe('ship models', () => {
  it('every model the catalogue can show has an explicit bow orientation', async () => {
    // A bounding box cannot tell nose from tail, so each GLB needs a recorded yaw.
    // The real regression here is a NEW model landing with no entry and silently
    // flying backwards — which is exactly what happened to three of the four hulls.
    const { MODEL_YAW, allModels } = await import('../src/sim/data');

    for (const model of allModels()) {
      expect(MODEL_YAW[model], `no MODEL_YAW entry for "${model}"`).toBeTypeOf('number');
    }
  });
});

describe('console controls are built once', () => {
  it('no station panel assigns innerHTML', () => {
    // Rebuilding markup is what destroys a button mid-press. Panels may only write
    // text, attributes and classes — see src/stations/ui/dom.ts.
    for (const { path, source } of read('src/stations')) {
      expect(/\.innerHTML\s*=/.test(source), `${path} rebuilds markup instead of mutating it`).toBe(
        false,
      );
    }
  });
});
