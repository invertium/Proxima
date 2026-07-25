// Ship model loading. The generated GLBs load straight from public/assets/ships/ —
// this is the whole asset import path.

import {
  Box3,
  Group,
  Mesh,
  MeshStandardMaterial,
  Matrix4,
  Object3D,
  Vector3,
  type Material,
} from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { MeshoptDecoder } from 'three/addons/libs/meshopt_decoder.module.js';
import { MODEL_YAW, allModels } from '../sim/data';

const loader = new GLTFLoader().setMeshoptDecoder(MeshoptDecoder);

/**
 * Generators don't agree on which axis a hull points down — each TRELLIS batch comes
 * out on a different one, and hand-fixing every model is exactly the per-asset busywork
 * this pipeline exists to avoid.
 *
 * So derive it: a ship is longest along its keel, widest across its wings, and shortest
 * top-to-bottom. Sort the bounding box and rotate so length lands on +X (the sim's
 * forward), width on Z, and height on Y.
 *
 * Bow-vs-stern is the one thing a box genuinely cannot tell us; that lives in
 * MODEL_YAW, applied below.
 */
const axisAlign = (model: string, size: Vector3): Matrix4 => {
  const axes: [number, Vector3][] = [
    [size.x, new Vector3(1, 0, 0)],
    [size.y, new Vector3(0, 1, 0)],
    [size.z, new Vector3(0, 0, 1)],
  ];
  axes.sort((a, b) => b[0] - a[0]);

  // The keel pick is a comparison between the two longest extents, and on these hulls
  // that margin can be tiny — the interceptor's top two differ by 0.19%. A re-export
  // with different tessellation would flip the sort and render the player's ship
  // sideways, so say so rather than failing silently later.
  const margin = axes[0]![0] > 0 ? (axes[0]![0] - axes[1]![0]) / axes[0]![0] : 1;
  if (margin < 0.05) {
    console.warn(
      `[ships] ${model}: keel axis is ambiguous (top two extents differ by ${(margin * 100).toFixed(2)}%). ` +
        `If this hull renders sideways, that is why.`,
    );
  }

  const length = axes[0]![1];
  const width = axes[1]![1];
  const height = axes[2]![1];

  // Columns map model axes onto world X/Y/Z; transpose is the rotation that gets us there.
  const basis = new Matrix4().makeBasis(length, height, width);
  if (basis.determinant() < 0) basis.makeBasis(length, height, width.clone().negate());
  return basis.transpose();
};

/** One loaded prototype per distinct GLB; every ship instance is a clone of it. */
const prototypes = new Map<string, Object3D>();

const load = async (model: string): Promise<Object3D> => {
  const cached = prototypes.get(model);
  if (cached) return cached;

  const gltf = await loader.loadAsync(`assets/ships/${model}.glb`);
  const root = gltf.scene;

  // Normalise: the generators emit meshes centred arbitrarily at ~1 unit. Recentre on
  // the origin and scale to unit length so the catalogue's `scale` is the only number
  // that decides how big a hull reads in the sector.
  const box = new Box3().setFromObject(root);
  const size = new Vector3();
  const centre = new Vector3();
  box.getSize(size);
  box.getCenter(centre);

  const longest = Math.max(size.x, size.y, size.z) || 1;
  root.position.sub(centre);

  const holder = new Group();
  holder.add(root);
  holder.scale.setScalar(1 / longest);
  holder.applyMatrix4(axisAlign(model, size));

  // Then turn the hull the right way round. applyMatrix4 pre-multiplies, so this yaw
  // is about world +Y — which is what we want. Rotating about the model's own Y
  // (holder.rotateY) would compose with the alignment and tilt the ship instead.
  holder.applyMatrix4(new Matrix4().makeRotationY(MODEL_YAW[model] ?? 0));

  root.traverse((o) => {
    const mesh = o as Mesh;
    if (!mesh.isMesh) return;
    mesh.castShadow = false;
    mesh.receiveShadow = false;
    mesh.frustumCulled = true;
    const mat = mesh.material as Material;
    if ((mat as MeshStandardMaterial).isMeshStandardMaterial) {
      const std = mat as MeshStandardMaterial;
      std.metalness = 0.15;
      std.roughness = 0.5;
    }
  });

  const wrapper = new Group();
  wrapper.add(holder);
  prototypes.set(model, wrapper);
  return wrapper;
};

/** Warms the cache for every model the campaign can show, so nothing pops in mid-fight. */
export const preloadShipModels = async (): Promise<void> => {
  await Promise.all(allModels().map(load));
};

/**
 * A fresh instance of a model. Materials are shared with the prototype on purpose —
 * every Pact gunship draws from one material, which keeps the draw calls batched.
 */
export const makeShip = (model: string, scale: number): Object3D => {
  const proto = prototypes.get(model);
  if (!proto) throw new Error(`ship model "${model}" not preloaded`);

  const instance = proto.clone(true);
  instance.scale.setScalar(scale);
  return instance;
};
