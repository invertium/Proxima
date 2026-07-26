// Model bench — see model.html.
//
// Renders a single procedural model with the game's exact lighting so a render can be
// held up against art_src/refs/. This exists because every fidelity defect in these
// models so far (black planets, an opaque corona ring, a near-black station hull) was
// found by LOOKING at the output next to the reference, and never by a passing test.
//
// Query string: ?model=starbase|haven|rocky|ice|sun  &view=front|three-quarter|top
// Not a build entry; dev-server only.

import {
  AmbientLight,
  Color,
  DirectionalLight,
  PerspectiveCamera,
  Scene,
  WebGLRenderer,
} from 'three';
import { createStarbase } from './starbase';
import { PLANET_PALETTES, createBody } from './planet';

const params = new URLSearchParams(location.search);
const which = params.get('model') ?? 'starbase';
const view = params.get('view') ?? 'front';

const renderer = new WebGLRenderer({ antialias: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
document.body.appendChild(renderer.domElement);

const scene = new Scene();
scene.background = new Color(0x05070f);
scene.add(new AmbientLight(0x404a66, 1.4));
const key = new DirectionalLight(0xfff0dd, 2.2);
key.position.set(1, 0.6, 0.4);
scene.add(key);

const camera = new PerspectiveCamera(38, innerWidth / innerHeight, 0.01, 100);

let update: (dt: number) => void;

if (which === 'starbase') {
  const station = createStarbase();
  scene.add(station.root);
  update = station.update;
} else {
  const body = createBody({
    kind: which === 'sun' ? 'sun' : 'planet',
    radius: 1,
    color: which === 'sun' ? 0xff8a3c : 0x6fa8d8,
    palette: PLANET_PALETTES[which] ?? PLANET_PALETTES['rocky']!,
    seed: 7,
  });
  scene.add(body.root);
  update = body.update;
}

// The reference sheet is a flat front elevation, so `front` is the view that can actually
// be compared against it; the others are for checking the model holds up in the round.
const FRAMING: Record<string, [number, number, number]> = {
  front: [0, 0, 3.1],
  'three-quarter': [2.0, 1.1, 2.0],
  top: [0.01, 3.0, 0],
};
camera.position.set(...(FRAMING[view] ?? FRAMING['front']!));
camera.lookAt(0, 0, 0);

addEventListener('resize', () => {
  camera.aspect = innerWidth / innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight);
});

// Exposed so a script can read renderer.info — the draw-call count for these models is a
// claim worth being able to check rather than assert in a comment.
(window as unknown as { __bench: unknown }).__bench = { renderer, scene, camera };

let last = performance.now();
const frame = (now: number): void => {
  const dt = Math.min((now - last) / 1000, 0.05);
  last = now;
  update(dt);
  renderer.render(scene, camera);
  requestAnimationFrame(frame);
};
requestAnimationFrame(frame);
