// Haven Starbase — procedural, built in code from art_src/refs/station-haven.png.
//
// Reconstruction-by-code (img2threejs method): no mesh file, no texture file. Every
// form here is a primitive, and the only "texture" is instanced emissive geometry for
// the window rows. That keeps it a ~40 KB source file instead of a multi-megabyte GLB,
// and it means the station can be recoloured or re-proportioned by editing numbers.
//
// Identity-defining systems from the reference, in the order a viewer reads them:
//   1. a long horizontal spine of stacked hull modules
//   2. a large habitation torus ringing the middle, on four spokes
//   3. blunt docking clusters capping both ends
//   4. two flat solar wings on the vertical axis
//   5. rows of small amber windows, and red beacons at the extremities
//
// Built at roughly 2 units across so the caller can scale it by the landmark radius.
// Stylised, not exact: a single orthographic view cannot show the far side, so the
// model is bilaterally symmetric by construction.

import {
  BoxGeometry,
  CylinderGeometry,
  Group,
  InstancedMesh,
  Matrix4,
  Mesh,
  MeshBasicMaterial,
  MeshStandardMaterial,
  Object3D,
  SphereGeometry,
  TorusGeometry,
} from 'three';
import { makeRng } from '../../sim/math';

const HULL = 0xb8bcc4;
const HULL_DARK = 0x6f757e;
const PANEL = 0x1b2740;
const AMBER = 0xffb648;
const BEACON = 0xff3b30;

/** Shared across every starbase instance, so the whole station is a handful of draws. */
const materials = {
  // A station is a lit structure, not a rock. With one key light most faces here point
  // away from it and the hull read as near-black; a little self-illumination puts it
  // back to the grey-white of the reference without flattening the shading.
  hull: new MeshStandardMaterial({
    color: HULL,
    roughness: 0.62,
    metalness: 0.18,
    emissive: HULL,
    emissiveIntensity: 0.38,
  }),
  hullDark: new MeshStandardMaterial({
    color: HULL_DARK,
    roughness: 0.7,
    metalness: 0.25,
    emissive: HULL_DARK,
    emissiveIntensity: 0.3,
  }),
  panel: new MeshStandardMaterial({ color: PANEL, roughness: 0.35, metalness: 0.55 }),
  // Windows and beacons are unlit: they read as light sources, not lit surfaces.
  window: new MeshBasicMaterial({ color: AMBER }),
  beacon: new MeshBasicMaterial({ color: BEACON }),
};

/** Reused unit primitives — geometry is created once for the whole module. */
const geo = {
  module: new BoxGeometry(1, 1, 1),
  drum: new CylinderGeometry(1, 1, 1, 16),
  ring: new TorusGeometry(1, 0.075, 12, 64),
  window: new BoxGeometry(1, 1, 1),
  beacon: new SphereGeometry(1, 6, 5),
};

const box = (
  parent: Object3D,
  material: MeshStandardMaterial,
  size: [number, number, number],
  at: [number, number, number],
): Mesh => {
  const mesh = new Mesh(geo.module, material);
  mesh.scale.set(...size);
  mesh.position.set(...at);
  parent.add(mesh);
  return mesh;
};

/** A drum section of the spine, lying along X. */
const drum = (parent: Object3D, radius: number, length: number, x: number): Mesh => {
  const mesh = new Mesh(geo.drum, materials.hull);
  mesh.scale.set(radius, length, radius);
  mesh.rotation.z = Math.PI / 2;
  mesh.position.x = x;
  parent.add(mesh);
  return mesh;
};

/**
 * One docking cluster: a stubby core with four radial pods. The reference shows these
 * as the busiest silhouette on the station, so they carry most of the small detail.
 */
const dockingCluster = (parent: Object3D, x: number, facing: number): Group => {
  const cluster = new Group();
  cluster.position.x = x;

  box(cluster, materials.hullDark, [0.13, 0.17, 0.17], [0, 0, 0]);
  box(cluster, materials.hull, [0.05, 0.26, 0.26], [facing * 0.08, 0, 0]);

  // Four pods on the vertical/lateral cross, matching the reference's cluster shape.
  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * Math.PI * 2 + Math.PI / 4;
    const pod = new Group();
    pod.position.set(facing * 0.04, Math.cos(angle) * 0.13, Math.sin(angle) * 0.13);
    box(pod, materials.hull, [0.16, 0.07, 0.07], [0, 0, 0]);
    box(pod, materials.hullDark, [0.04, 0.09, 0.09], [facing * 0.09, 0, 0]);
    cluster.add(pod);
  }

  // Mast and beacon at the very tip — the outermost thing on the silhouette.
  box(cluster, materials.hullDark, [0.02, 0.12, 0.02], [facing * 0.11, 0.1, 0]);
  return cluster;
};

/** Flat solar wing on the vertical axis, split into cells like the reference. */
const solarWing = (parent: Object3D, sign: number): Group => {
  const wing = new Group();

  // Mast out to the panel.
  box(wing, materials.hullDark, [0.035, 0.34, 0.035], [0, sign * 0.62, 0]);

  const panel = new Group();
  panel.position.y = sign * 1.28;
  for (let c = 0; c < 2; c++) {
    for (let r = 0; r < 4; r++) {
      box(
        panel,
        materials.panel,
        [0.155, 0.15, 0.012],
        [(c - 0.5) * 0.17, (r - 1.5) * 0.16, 0],
      );
    }
  }
  wing.add(panel);
  parent.add(wing);
  return wing;
};

export interface Starbase {
  root: Group;
  /** The habitation ring turns slowly; call each frame. */
  update(dt: number): void;
}

export const createStarbase = (): Starbase => {
  const root = new Group();
  const rng = makeRng(4242); // deterministic: the same station every session

  // ── 1. Spine ────────────────────────────────────────────────────────────────
  const spine = new Group();
  drum(spine, 0.075, 1.5, 0);
  for (const x of [-0.62, -0.42, 0.42, 0.62]) {
    box(spine, materials.hull, [0.16, 0.13, 0.13], [x, 0, 0]);
  }
  for (const x of [-0.52, -0.32, 0.32, 0.52]) {
    box(spine, materials.hullDark, [0.03, 0.15, 0.15], [x, 0, 0]);
  }
  root.add(spine);

  // ── 2. Habitation ring on four spokes ───────────────────────────────────────
  const ring = new Group();
  const torus = new Mesh(geo.ring, materials.hull);
  torus.scale.setScalar(0.42);
  torus.rotation.y = Math.PI / 2; // stand the ring up around the spine's axis
  ring.add(torus);

  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * Math.PI * 2;
    const spoke = new Mesh(geo.module, materials.hullDark);
    spoke.scale.set(0.05, 0.42, 0.05);
    spoke.position.set(0, Math.cos(angle) * 0.21, Math.sin(angle) * 0.21);
    spoke.rotation.x = -angle;
    ring.add(spoke);
  }
  root.add(ring);

  // Central hub where the spokes meet.
  box(root, materials.hull, [0.17, 0.17, 0.17], [0, 0, 0]);
  box(root, materials.panel, [0.185, 0.09, 0.09], [0, 0, 0]);

  // ── 3. Docking clusters ─────────────────────────────────────────────────────
  root.add(dockingCluster(root, -0.86, -1));
  root.add(dockingCluster(root, 0.86, 1));

  // ── 4. Solar wings ──────────────────────────────────────────────────────────
  solarWing(root, 1);
  solarWing(root, -1);

  // ── 5. Window rows and beacons ──────────────────────────────────────────────
  // Instanced: the reference has dozens of lit windows, and one draw call is the
  // difference between a detail that's free and a detail that costs a frame.
  const windowRows: Matrix4[] = [];
  const m = new Matrix4();

  for (let i = 0; i < 64; i++) {
    const x = -0.72 + (i / 63) * 1.44;
    // Skip the hub, where the reference has no window band.
    if (Math.abs(x) < 0.12) continue;
    for (const side of [-1, 1]) {
      if (rng() < 0.25) continue; // a few dark windows, so the row isn't mechanical
      windowRows.push(m.clone().makeTranslation(x, side * 0.072, 0.055));
      windowRows.push(m.clone().makeTranslation(x, side * 0.072, -0.055));
    }
  }

  // Windows around the habitation ring.
  for (let i = 0; i < 48; i++) {
    const angle = (i / 48) * Math.PI * 2;
    if (rng() < 0.2) continue;
    windowRows.push(
      m.clone().makeTranslation(0.02, Math.cos(angle) * 0.42, Math.sin(angle) * 0.42),
    );
  }

  const windows = new InstancedMesh(geo.window, materials.window, windowRows.length);
  const scratch = new Matrix4().makeScale(0.018, 0.012, 0.012);
  windowRows.forEach((translation, i) => {
    windows.setMatrixAt(i, translation.multiply(scratch));
  });
  windows.instanceMatrix.needsUpdate = true;
  root.add(windows);

  // Red beacons at the extremities, as in the reference.
  for (const [x, y, z] of [
    [-0.95, 0.13, 0],
    [-0.95, -0.13, 0],
    [0.95, 0.13, 0],
    [0.95, -0.13, 0],
    [0, 0.44, 0],
    [0, -0.44, 0],
  ] as [number, number, number][]) {
    const beacon = new Mesh(geo.beacon, materials.beacon);
    beacon.scale.setScalar(0.016);
    beacon.position.set(x, y, z);
    root.add(beacon);
  }

  // Runtime hierarchy, so callers can find the parts rather than guessing at children.
  root.userData['sculptRuntime'] = {
    parts: { spine, ring, windows },
    sockets: { dockPort: { x: 0.86, y: 0, z: 0 }, dockStarboard: { x: -0.86, y: 0, z: 0 } },
  };

  return {
    root,
    update(dt: number) {
      // A habitation ring spins for gravity. Slow enough to read as deliberate.
      ring.rotation.x += dt * 0.12;
    },
  };
};
