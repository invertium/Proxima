// Asset pipeline: art_src/generated_ships/*.glb -> public/assets/ships/*.glb
//
// This is the entire import step. Drop a TRELLIS/Hunyuan GLB into art_src, run
// `npm run assets`, add a row to src/sim/data.ts. No editor, no .uasset, no reimport
// dance, no per-format importer flags.
//
// gltf-transform does the work: weld + join draws down the draw-call count, meshopt
// compresses geometry, and textures go to WebP at 1K — which is what the generator
// produces anyway, so nothing is thrown away.

import { execFileSync } from 'node:child_process';
import { mkdirSync, readdirSync, statSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const src = resolve(root, '..', 'art_src', 'generated_ships');
const out = join(root, 'public', 'assets', 'ships');

mkdirSync(out, { recursive: true });

const mb = (p) => (statSync(p).size / 1024 / 1024).toFixed(1);
const models = readdirSync(src).filter((f) => f.endsWith('.glb'));

if (models.length === 0) {
  console.error(`no .glb found in ${src}`);
  process.exit(1);
}

for (const file of models) {
  const from = join(src, file);
  const to = join(out, file);

  execFileSync(
    'npx',
    [
      '--yes',
      '@gltf-transform/cli',
      'optimize',
      from,
      to,
      '--compress',
      'meshopt',
      '--texture-compress',
      'webp',
      '--texture-size',
      '1024',
      '--simplify-error',
      '0.001',
      '--join',
      'true',
    ],
    { stdio: 'inherit', cwd: root },
  );

  console.log(`${file}: ${mb(from)} MB -> ${mb(to)} MB`);
}

console.log(`\n${models.length} model(s) ready in public/assets/ships/`);
