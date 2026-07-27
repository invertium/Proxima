// Haven Starbase — procedural, built in code from art_src/refs/station-haven.png.
//
// Reconstruction-by-code (img2threejs method): no mesh file, no texture file. Every form
// here is a primitive, and the only "texture" is instanced emissive geometry for the
// window rows. That keeps it a source file instead of a multi-megabyte GLB, and the
// station can be re-proportioned by editing numbers.
//
// Identity-defining systems from the reference, in the order a viewer reads them:
//   1. a segmented spine of stacked hull modules, ribbed collars between them
//   2. a thick plated habitation torus ringing the middle, on four slab spokes
//   3. clawed docking clusters capping both ends
//   4. two flat solar wings on the vertical axis
//   5. amber windows recessed in dark strips, and red beacons at every extremity
//
// Proportions are measured off the reference as fractions of the half-span, so the
// numbers below are literally what the image shows: ring outer radius 0.268, hub half
// 0.071, clusters from 0.70 out to 1.0, wing tips at 0.635.
//
// PERFORMANCE. Detail this dense would be ~600 draw calls one Mesh at a time. Instead
// every static part is baked into merged geometry per material, so the whole station
// measures 8 draw calls / 15.7k triangles (renderer.info via tools/model-shot.mjs) no
// matter how many greebles get added. That is the entire reason more fidelity was
// affordable here: the cost is build-time, paid once, not per frame.
//
// Built at roughly 2 units across so the caller can scale it by the landmark radius.
// Stylised, not exact: a single orthographic view cannot show the far side, so the model
// is bilaterally symmetric by construction.

import {
  BoxGeometry,
  type BufferGeometry,
  Color,
  CylinderGeometry,
  Group,
  InstancedMesh,
  type Matrix4,
  Mesh,
  MeshBasicMaterial,
  MeshStandardMaterial,
  Object3D,
  SphereGeometry,
  TorusGeometry,
} from 'three';
import { mergeGeometries } from 'three/examples/jsm/utils/BufferGeometryUtils.js';
import { makeRng } from '../../sim/math';

const HULL = 0xc2c6cc;
const HULL_DARK = 0x8a9099;
/** Recess/shadow line colour: what makes plating read as plating rather than one slab. */
const TRIM = 0x2a2f38;
/**
 * Solar cells. Much lighter than the true near-black of a photovoltaic panel: at 0x161f36
 * the array rendered as an empty wire rectangle against space, because a dark blue on a
 * dark blue background is nothing at all. The reference reads as a panel, so this does.
 */
const PANEL = 0x33436b;
const AMBER = 0xffb648;
const BEACON = 0xff3b30;

type Vec3 = [number, number, number];
type MatKey = 'hull' | 'hullDark' | 'trim' | 'panel';

/** Shared across every starbase instance, so the whole station is a handful of draws. */
const materials: Record<MatKey, MeshStandardMaterial> = {
  // A station is a lit structure, not a rock. With one key light most faces here point
  // away from it and the hull read as near-black; a little self-illumination puts it
  // back to the grey-white of the reference without flattening the shading.
  hull: new MeshStandardMaterial({
    color: HULL,
    roughness: 0.58,
    metalness: 0.2,
    emissive: HULL,
    emissiveIntensity: 0.34,
  }),
  hullDark: new MeshStandardMaterial({
    color: HULL_DARK,
    roughness: 0.7,
    metalness: 0.28,
    emissive: HULL_DARK,
    emissiveIntensity: 0.26,
  }),
  trim: new MeshStandardMaterial({ color: TRIM, roughness: 0.85, metalness: 0.4 }),
  panel: new MeshStandardMaterial({
    color: PANEL,
    roughness: 0.32,
    metalness: 0.55,
    emissive: PANEL,
    emissiveIntensity: 0.4,
  }),
};

/** Unit primitives, instantiated once and transformed into the merge buffers. */
const unit = {
  box: new BoxGeometry(1, 1, 1),
  drum: new CylinderGeometry(1, 1, 1, 12),
  // Octagonal prism for the hub. Pre-rotated by half a segment so a FLAT face points at
  // ±Z, which is where the command panel goes — land a vertex there and the panel floats.
  oct: new CylinderGeometry(1, 1, 1, 8).rotateY(Math.PI / 8),
  window: new BoxGeometry(1, 1, 1),
  beacon: new SphereGeometry(1, 6, 5),
};

const scratch = new Object3D();

const matrixOf = (size: Vec3, at: Vec3, rot: Vec3 = [0, 0, 0]): Matrix4 => {
  scratch.position.set(...at);
  scratch.rotation.set(...rot);
  scratch.scale.set(...size);
  scratch.updateMatrix();
  return scratch.matrix.clone();
};

/**
 * Collects transformed primitives into per-material buffers, then bakes each buffer into
 * one Mesh. Windows and beacons stay instanced because they want their own materials and,
 * in the beacons' case, animation.
 */
class Assembly {
  private readonly buffers: Record<MatKey, BufferGeometry[]> = {
    hull: [],
    hullDark: [],
    trim: [],
    panel: [],
  };
  private readonly windows: Matrix4[] = [];
  private readonly windowTints: Color[] = [];
  /** Two blink phases, so the beacons don't pulse in unison like a single lamp. */
  private readonly beacons: [Matrix4[], Matrix4[]] = [[], []];
  /** Lamps that light in sequence rather than blinking — see `beaconChase`. */
  private readonly chase: Matrix4[] = [];

  add(material: MatKey, geometry: BufferGeometry, size: Vec3, at: Vec3, rot?: Vec3): void {
    this.buffers[material].push(geometry.clone().applyMatrix4(matrixOf(size, at, rot)));
  }

  box(material: MatKey, size: Vec3, at: Vec3, rot?: Vec3): void {
    this.add(material, unit.box, size, at, rot);
  }

  /** A drum lying along X, which is how every spine collar is shaped. */
  drum(material: MatKey, radius: number, length: number, x: number): void {
    this.add(material, unit.drum, [radius, length, radius], [x, 0, 0], [0, 0, Math.PI / 2]);
  }

  window(size: Vec3, at: Vec3, rot: Vec3 | undefined, tint: Color): void {
    this.windows.push(matrixOf(size, at, rot));
    this.windowTints.push(tint);
  }

  beacon(at: Vec3, phase: 0 | 1, scale = 0.011): void {
    this.beacons[phase].push(matrixOf([scale, scale, scale], at));
  }

  /**
   * A lamp in a running sequence. Collection order is the chase order, so pushing these
   * around the ring makes the light travel around it.
   */
  beaconChase(at: Vec3, scale = 0.011): void {
    this.chase.push(matrixOf([scale, scale, scale], at));
  }

  /** Bakes everything collected so far. Returns the group and the animated beacon meshes. */
  bake(): {
    root: Group;
    lamps: { mesh: InstancedMesh; phase: 0 | 1 }[];
    chase: InstancedMesh | null;
  } {
    const root = new Group();

    for (const key of Object.keys(this.buffers) as MatKey[]) {
      const parts = this.buffers[key];
      if (parts.length === 0) continue;
      const merged = mergeGeometries(parts, false);
      if (merged) root.add(new Mesh(merged, materials[key]));
      for (const part of parts) part.dispose();
    }

    if (this.windows.length > 0) {
      // Unlit: windows read as light sources, not as lit surfaces.
      const mesh = new InstancedMesh(
        unit.window,
        new MeshBasicMaterial({ color: 0xffffff }),
        this.windows.length,
      );
      this.windows.forEach((m, i) => {
        mesh.setMatrixAt(i, m);
        mesh.setColorAt(i, this.windowTints[i]!);
      });
      mesh.instanceMatrix.needsUpdate = true;
      if (mesh.instanceColor) mesh.instanceColor.needsUpdate = true;
      root.add(mesh);
    }

    const lamps: { mesh: InstancedMesh; phase: 0 | 1 }[] = [];
    ([0, 1] as const).forEach((phase) => {
      const set = this.beacons[phase];
      if (set.length === 0) return;
      const mesh = new InstancedMesh(
        unit.beacon,
        new MeshBasicMaterial({ color: BEACON }),
        set.length,
      );
      set.forEach((m, i) => {
        mesh.setMatrixAt(i, m);
      });
      mesh.instanceMatrix.needsUpdate = true;
      root.add(mesh);
      lamps.push({ mesh, phase });
    });

    let chase: InstancedMesh | null = null;
    if (this.chase.length > 0) {
      chase = new InstancedMesh(
        unit.beacon,
        new MeshBasicMaterial({ color: 0xffffff }),
        this.chase.length,
      );
      this.chase.forEach((m, i) => {
        chase!.setMatrixAt(i, m);
        chase!.setColorAt(i, new Color(BEACON));
      });
      chase.instanceMatrix.needsUpdate = true;
      root.add(chase);
    }

    return { root, lamps, chase };
  }
}

// ── Detail helpers ─────────────────────────────────────────────────────────────────

const WARM = new Color(AMBER);
const PALE = new Color(0xffe6b0);
const COOL = new Color(0x9fd4ff);

/** Windows are not one bulb: a station has warm quarters and cold-lit working spaces. */
const windowTint = (rng: () => number): Color => {
  const roll = rng();
  if (roll < 0.12) return COOL;
  if (roll < 0.42) return PALE;
  return WARM;
};

/**
 * A recessed window strip on one face of a module. The recess is the point — bare amber
 * quads floating on a hull face read as dashes painted on, which is exactly how the first
 * version of this model looked. Sinking them into a dark band gives them a socket.
 */
const windowStrip = (
  a: Assembly,
  rng: () => number,
  opts: {
    /** Centre of the strip. */
    at: Vec3;
    /** Strip length along X and its height. */
    length: number;
    height: number;
    /** Which face: 'z' faces ±Z, 'y' faces ±Y. */
    face: 'y' | 'z';
    /** Face offset from the module centre — where the hull surface sits. */
    offset: number;
    count: number;
  },
): void => {
  const { at, length, height, face, offset, count } = opts;
  const sign = Math.sign(offset);
  const depth = Math.abs(offset);

  const recess: Vec3 = face === 'z' ? [length, height, 0.004] : [length, 0.004, height];
  const recessAt: Vec3 =
    face === 'z' ? [at[0], at[1], at[2] + sign * depth] : [at[0], at[1] + sign * depth, at[2]];
  a.box('trim', recess, recessAt);

  const pane = height * 0.5;
  for (let i = 0; i < count; i++) {
    // Inset from the strip ends so the recess frames the row.
    const t = count === 1 ? 0.5 : (i + 0.5) / count;
    const x = at[0] + (t - 0.5) * length * 0.86;
    if (rng() < 0.18) continue; // a few dark windows, so the row isn't mechanical
    const size: Vec3 =
      face === 'z' ? [length / count / 2.6, pane, 0.004] : [length / count / 2.6, 0.004, pane];
    const pos: Vec3 =
      face === 'z'
        ? [x, at[1], at[2] + sign * (depth + 0.004)]
        : [x, at[1] + sign * (depth + 0.004), at[2]];
    a.window(size, pos, undefined, windowTint(rng));
  }
};

/**
 * One spine module: a plated box with chamfer strips top and bottom, a dark seam at each
 * end, and window rows on all four faces.
 */
const spineModule = (
  a: Assembly,
  rng: () => number,
  s: number,
  x0: number,
  x1: number,
  half: number,
): void => {
  const length = x1 - x0;
  const cx = s * (x0 + length / 2);
  const len = length * 0.96;

  a.box('hull', [len, half * 2, half * 2], [cx, 0, 0]);
  // Chamfer plating: narrower in Z, proud in Y — the bevel the reference reads as.
  a.box('hullDark', [len, half * 0.5, half * 2.12], [cx, half * 0.86, 0]);
  a.box('hullDark', [len, half * 0.5, half * 2.12], [cx, -half * 0.86, 0]);
  a.box('hullDark', [len, half * 2.12, half * 0.5], [cx, 0, half * 0.86]);
  a.box('hullDark', [len, half * 2.12, half * 0.5], [cx, 0, -half * 0.86]);
  // Seams at both ends of the module.
  for (const end of [-1, 1]) {
    a.box('trim', [length * 0.05, half * 2.2, half * 2.2], [cx + end * len * 0.5, 0, 0]);
  }

  const panes = Math.max(2, Math.round(length / 0.026));
  for (const sign of [1, -1]) {
    windowStrip(a, rng, {
      at: [cx, half * 0.2, 0],
      length: len * 0.82,
      height: half * 0.5,
      face: 'z',
      offset: sign * (half + 0.002),
      count: panes,
    });
    windowStrip(a, rng, {
      at: [cx, 0, half * 0.2],
      length: len * 0.82,
      height: half * 0.5,
      face: 'y',
      offset: sign * (half + 0.002),
      count: panes,
    });
  }
};

/** A ribbed collar between two modules. */
const collar = (a: Assembly, s: number, x0: number, x1: number, radius: number): void => {
  const cx = s * (x0 + (x1 - x0) / 2);
  a.drum('hullDark', radius, (x1 - x0) * 0.9, cx);
  a.drum('trim', radius * 1.08, (x1 - x0) * 0.32, cx);
};

// ── The spine, measured off the reference as fractions of the half-span ─────────────

/** [start, end, half-height]; `collar` entries are drums, `module` entries are plated. */
const SPINE: { x0: number; x1: number; half: number; kind: 'module' | 'collar' }[] = [
  { x0: 0.072, x1: 0.108, half: 0.03, kind: 'collar' },
  { x0: 0.108, x1: 0.205, half: 0.045, kind: 'module' },
  { x0: 0.205, x1: 0.292, half: 0.038, kind: 'collar' }, // passes through the ring
  { x0: 0.292, x1: 0.33, half: 0.033, kind: 'collar' },
  { x0: 0.33, x1: 0.442, half: 0.05, kind: 'module' },
  { x0: 0.442, x1: 0.474, half: 0.034, kind: 'collar' },
  { x0: 0.474, x1: 0.578, half: 0.046, kind: 'module' },
  { x0: 0.578, x1: 0.608, half: 0.035, kind: 'collar' },
  { x0: 0.608, x1: 0.7, half: 0.052, kind: 'module' },
];

const RING_INNER = 0.215;
const RING_OUTER = 0.268;
const RING_MID = (RING_INNER + RING_OUTER) / 2;
const RING_DEPTH = 0.062;
const HUB_HALF = 0.071;

/**
 * The docking cluster: a neck, a body, and three clawed arms in a vertical stack. This is
 * the busiest silhouette on the station in the reference, so it carries the most detail
 * and every prong tip gets a beacon.
 */
const dockingCluster = (a: Assembly, rng: () => number, s: number): void => {
  // Neck and body.
  a.box('hullDark', [0.062, 0.086, 0.086], [s * 0.731, 0, 0]);
  a.box('hull', [0.114, 0.12, 0.12], [s * 0.817, 0, 0]);
  a.box('trim', [0.01, 0.128, 0.128], [s * 0.762, 0, 0]);
  for (const sign of [1, -1]) {
    windowStrip(a, rng, {
      at: [s * 0.817, 0, 0],
      length: 0.092,
      height: 0.03,
      face: 'z',
      offset: sign * 0.062,
      count: 4,
    });
  }

  // Vertical spar the arms hang off.
  a.box('hullDark', [0.048, 0.33, 0.052], [s * 0.85, 0, 0]);

  // Three arms, each ending in a two-pronged clamp.
  [-0.132, 0, 0.132].forEach((y, i) => {
    const reach = i === 1 ? 1.0 : 0.88; // the centre arm is the longest, as in the reference
    const x0 = 0.85;
    const x1 = 0.85 + 0.108 * reach;

    a.box('hull', [x1 - x0, 0.062, 0.062], [(s * (x0 + x1)) / 2, y, 0]);
    a.box('hullDark', [(x1 - x0) * 0.9, 0.07, 0.03], [(s * (x0 + x1)) / 2, y, 0]);
    windowStrip(a, rng, {
      at: [(s * (x0 + x1)) / 2, y, 0],
      length: (x1 - x0) * 0.7,
      height: 0.02,
      face: 'z',
      offset: 0.033,
      count: 3,
    });

    // The clamp: two prongs opening outward, which is what makes it read as a dock.
    for (const z of [1, -1]) {
      a.box('hullDark', [0.052, 0.034, 0.022], [s * (x1 + 0.02), y, z * 0.036]);
      a.box('hull', [0.02, 0.042, 0.02], [s * (x1 + 0.044), y, z * 0.046]);
      a.beacon([s * (x1 + 0.056), y, z * 0.046], (i % 2) as 0 | 1, 0.008);
    }
    a.box('trim', [0.016, 0.048, 0.076], [s * (x1 + 0.006), y, 0]);
  });

  // Mast and navigation lamp at the very tip — the outermost thing on the silhouette.
  a.box('hullDark', [0.03, 0.016, 0.016], [s * 0.985, 0, 0]);
  a.beacon([s * 1.005, 0, 0], 0, 0.013);
  a.beacon([s * 0.85, 0.176, 0], 1, 0.01);
  a.beacon([s * 0.85, -0.176, 0], 1, 0.01);
};

/**
 * Flat solar wing on the vertical axis, split into cells like the reference.
 *
 * The mast is one of the ring's four spokes: it runs from the hub, through the ring, and
 * on out to the panel, so it is segmented across that whole span rather than stopping at
 * the ring.
 */
const solarWing = (a: Assembly, sign: number): void => {
  // Segmented mast from the hub out to the panel, passing through the ring.
  for (let i = 0; i < 4; i++) {
    const y0 = 0.07 + i * 0.058;
    a.box('hull', [0.034, 0.05, 0.034], [0, sign * (y0 + 0.026), 0]);
    a.box('trim', [0.042, 0.01, 0.042], [0, sign * (y0 + 0.055), 0]);
  }

  const centre = sign * 0.468;
  const halfLen = 0.168; // panel spans 0.30 .. 0.635 of the half-span
  const halfWide = 0.085;

  // Mast run from the ring to the panel root.
  a.box('hullDark', [0.026, 0.075, 0.026], [0, sign * 0.338, 0]);

  // Backing plate: without it the gaps between cells show open space, and the panel
  // renders as an empty wire rectangle instead of a solar array.
  a.box('hullDark', [halfWide * 2, halfLen * 2, 0.01], [0, centre, 0]);
  // Frame: border rails plus the central spar the two cell columns sit either side of.
  a.box('hull', [0.01, halfLen * 2, 0.016], [0, centre, 0]);
  for (const end of [-1, 1]) {
    a.box('hull', [halfWide * 2, 0.01, 0.016], [0, centre + end * halfLen, 0]);
    a.box('hull', [0.01, halfLen * 2, 0.016], [end * halfWide, centre, 0]);
  }

  for (let col = 0; col < 2; col++) {
    for (let row = 0; row < 6; row++) {
      const cell: Vec3 = [halfWide * 0.82, (halfLen * 2) / 6.9, 0.006];
      const at: Vec3 = [(col - 0.5) * halfWide, centre + (row - 2.5) * ((halfLen * 2) / 6.1), 0];
      // Both faces, so the array reads from either side.
      for (const z of [0.008, -0.008]) a.box('panel', cell, [at[0], at[1], z]);
    }
  }

  // Corner lamps, as in the reference.
  for (const dx of [-1, 1]) {
    for (const dy of [-1, 1]) {
      a.beacon([dx * halfWide, centre + dy * halfLen, 0], dx * dy > 0 ? 0 : 1, 0.008);
    }
  }
};

/** The chamfered hub where the spokes, the spine and the wing masts all meet. */
const hub = (a: Assembly, rng: () => number): void => {
  // An octagonal prism rather than a cube: the reference hub is visibly chamfered, and
  // a real chamfer is one primitive here where a boolean cut would be a mesh library.
  a.add(
    'hull',
    unit.oct,
    [HUB_HALF * 1.08, HUB_HALF * 2, HUB_HALF * 1.08],
    [0, 0, 0],
    [0, 0, Math.PI / 2],
  );
  // Collar bands at both ends, and a raised belt around the middle.
  for (const end of [-1, 1]) {
    a.add(
      'hullDark',
      unit.oct,
      [HUB_HALF * 1.14, HUB_HALF * 0.24, HUB_HALF * 1.14],
      [end * HUB_HALF * 0.92, 0, 0],
      [0, 0, Math.PI / 2],
    );
  }
  a.add(
    'hullDark',
    unit.oct,
    [HUB_HALF * 1.16, HUB_HALF * 0.9, HUB_HALF * 1.16],
    [0, 0, 0],
    [0, 0, Math.PI / 2],
  );
  a.add(
    'trim',
    unit.oct,
    [HUB_HALF * 1.18, HUB_HALF * 0.16, HUB_HALF * 1.18],
    [0, 0, 0],
    [0, 0, Math.PI / 2],
  );

  // The lit command panel the reference puts dead centre, on all four broad faces.
  // Offsets clear the raised belt: the octagon's flat face sits at cos(pi/8) x radius,
  // so the panel has to stand proud of 1.18 x 0.924 = 1.09 to be visible at all.
  const face = HUB_HALF * 1.1;
  for (const sign of [1, -1]) {
    for (const axis of ['z', 'y'] as const) {
      const plate: Vec3 =
        axis === 'z'
          ? [HUB_HALF * 1.2, HUB_HALF * 1.2, 0.006]
          : [HUB_HALF * 1.2, 0.006, HUB_HALF * 1.2];
      const plateAt: Vec3 = axis === 'z' ? [0, 0, sign * face] : [0, sign * face, 0];
      a.box('trim', plate, plateAt);

      for (let row = 0; row < 3; row++) {
        for (let col = 0; col < 3; col++) {
          if (rng() < 0.25) continue;
          const u = (col - 1) * HUB_HALF * 0.34;
          const v = (row - 1) * HUB_HALF * 0.34;
          const size: Vec3 =
            axis === 'z'
              ? [HUB_HALF * 0.22, HUB_HALF * 0.22, 0.004]
              : [HUB_HALF * 0.22, 0.004, HUB_HALF * 0.22];
          const at: Vec3 =
            axis === 'z' ? [u, v, sign * (face + 0.004)] : [u, sign * (face + 0.004), v];
          a.window(size, at, undefined, windowTint(rng));
        }
      }
    }
  }
};

export interface Starbase {
  root: Group;
  /** The habitation ring turns and the beacons blink; call each frame. */
  update(dt: number): void;
}

export const createStarbase = (): Starbase => {
  const rng = makeRng(4242); // deterministic: the same station every session

  // One assembly: nothing on this station moves as a rigid body, so every static part
  // bakes into the same handful of merged meshes.
  const hullParts = new Assembly();

  // ── 1. Hub and spine ────────────────────────────────────────────────────────────
  hub(hullParts, rng);
  for (const s of [1, -1]) {
    for (const seg of SPINE) {
      if (seg.kind === 'module') spineModule(hullParts, rng, s, seg.x0, seg.x1, seg.half);
      else collar(hullParts, s, seg.x0, seg.x1, seg.half);
    }
    // Spine running lights along the top and bottom.
    for (const x of [0.24, 0.4, 0.55]) {
      hullParts.beacon([s * x, 0.056, 0], 0, 0.007);
      hullParts.beacon([s * x, -0.056, 0], 1, 0.007);
    }
  }

  // ── 2. Habitation ring ───────────────────────────────────────────────────────────
  //
  // The ring lies in the XY plane — the plane of the spine AND the solar masts — not
  // around the spine's axis. That is what the reference shows, and it is the whole
  // reason the station reads as a wheel: the four spokes inside the ring are not extra
  // structure at all, they ARE the two spine arms and the two mast roots. Building it
  // around the spine (as this model first did) put it edge-on to the same view the
  // reference is drawn from, and it vanished into a vertical bar.
  //
  // Braced on all four sides like that, the ring cannot turn, so it does not — a
  // spinning habitat would have to be a free wheel, and this one is a truss. The
  // sequenced rim lamps below carry the sense of motion instead.
  const PLATES = 44;
  // Plates all but touch. At 0.94 the gaps read as a ring of loose tiles rather than a
  // built band; the seam strip below is what should show the joins, not empty space.
  const tangential = ((2 * Math.PI * RING_MID) / PLATES) * 1.005;

  for (let i = 0; i < PLATES; i++) {
    const angle = (i / PLATES) * Math.PI * 2;
    const at: Vec3 = [Math.cos(angle) * RING_MID, Math.sin(angle) * RING_MID, 0];
    const rot: Vec3 = [0, 0, angle];
    const rib = i % 4 === 0;

    hullParts.box(
      rib ? 'hullDark' : 'hull',
      [RING_OUTER - RING_INNER, tangential, rib ? RING_DEPTH * 1.16 : RING_DEPTH],
      at,
      rot,
    );
    // Seam between plates, so the ring reads as built rather than extruded.
    hullParts.box('trim', [RING_OUTER - RING_INNER, tangential * 0.06, RING_DEPTH * 1.04], at, rot);

    // The window band runs along the ring's BROAD faces — the annulus you look at — not
    // its rim. Putting it on the rim (as this first did) hides it edge-on from exactly
    // the angle the ring is shaped to be seen from.
    if (!rib) {
      for (const z of [1, -1]) {
        for (const r of [RING_MID - 0.013, RING_MID + 0.013]) {
          if (rng() < 0.2) continue;
          const face = z * (RING_DEPTH / 2 + 0.002);
          hullParts.box(
            'trim',
            [0.017, tangential * 0.78, 0.004],
            [Math.cos(angle) * r, Math.sin(angle) * r, face],
            rot,
          );
          // Two panes per plate rather than one block — the reference's band is a row of
          // small lit ports, and one big square per plate reads as dice pips.
          for (const t of [-0.24, 0.24]) {
            if (rng() < 0.15) continue;
            const a2 = angle + (t * tangential) / r;
            hullParts.window(
              [0.009, tangential * 0.26, 0.004],
              [Math.cos(a2) * r, Math.sin(a2) * r, face + z * 0.004],
              [0, 0, a2],
              windowTint(rng),
            );
          }
        }
      }
    }
  }

  // Rims cap the plate stack inside and out. Bright, like the reference's edge highlight.
  for (const radius of [RING_INNER, RING_OUTER]) {
    const rim = new TorusGeometry(radius, 0.011, 8, 80);
    hullParts.add('hull', rim, [1, 1, 1], [0, 0, 0]);
    rim.dispose();
  }

  // Junction blocks where the ring is braced to the spine (E/W) and the masts (N/S).
  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * Math.PI * 2;
    for (const radius of [RING_INNER + 0.006, RING_OUTER - 0.006]) {
      hullParts.box(
        'hullDark',
        [0.03, 0.098, RING_DEPTH * 1.3],
        [Math.cos(angle) * radius, Math.sin(angle) * radius, 0],
        [0, 0, angle],
      );
    }
  }

  // Rim lamps in a running sequence — a light travelling the ring, which reads as a
  // working structure without claiming the truss rotates.
  const RIM_LAMPS = 16;
  for (let i = 0; i < RIM_LAMPS; i++) {
    const angle = (i / RIM_LAMPS) * Math.PI * 2;
    hullParts.beaconChase(
      [Math.cos(angle) * (RING_OUTER + 0.011), Math.sin(angle) * (RING_OUTER + 0.011), 0],
      0.009,
    );
  }

  // ── 4. Docking clusters and solar wings ──────────────────────────────────────────
  for (const s of [1, -1]) dockingCluster(hullParts, rng, s);
  solarWing(hullParts, 1);
  solarWing(hullParts, -1);

  // ── 5. Bake ──────────────────────────────────────────────────────────────────────
  const baked = hullParts.bake();

  const root = new Group();
  root.add(baked.root);

  const lampColor = new Color();
  const chaseColor = new Color();
  let clock = 0;

  // Runtime hierarchy, so callers can find the parts rather than guessing at children.
  root.userData.sculptRuntime = {
    parts: { hull: baked.root, rimLamps: baked.chase },
    sockets: { dockPort: { x: 0.86, y: 0, z: 0 }, dockStarboard: { x: -0.86, y: 0, z: 0 } },
  };

  return {
    root,
    update(dt: number) {
      clock += dt;

      // Beacons blink out of phase. A station with steady lamps reads as a prop; the
      // blink is what says "crewed and under power" at a glance.
      for (const { mesh, phase } of baked.lamps) {
        const t = (clock * 0.6 + phase * 0.5) % 1;
        lampColor.setHex(BEACON).multiplyScalar(t < 0.5 ? 1 : 0.22);
        (mesh.material as MeshBasicMaterial).color.copy(lampColor);
      }

      // The rim sequence: a light running around the ring, brightest at the head and
      // trailing off behind it.
      const chase = baked.chase;
      if (chase) {
        const head = (clock * 0.35) % 1;
        for (let i = 0; i < chase.count; i++) {
          // Distance behind the head, wrapped — so the tail fades rather than cutting.
          const behind = (head - i / chase.count + 1) % 1;
          const lit = 0.18 + 0.82 * Math.max(0, 1 - behind * 6);
          chaseColor.setHex(BEACON).multiplyScalar(lit);
          chase.setColorAt(i, chaseColor);
        }
        if (chase.instanceColor) chase.instanceColor.needsUpdate = true;
      }
    },
  };
};
