// Small math kit for the simulation. Deliberately allocation-light: the hot paths
// (step, AI, collision) reuse scratch vectors rather than returning fresh objects.

export interface Vec3 {
  x: number;
  y: number;
  z: number;
}

/** World convention: Y is up, ships travel the XZ plane, heading is radians CCW from +X. */
export const vec = (x = 0, y = 0, z = 0): Vec3 => ({ x, y, z });

export const copy = (out: Vec3, a: Vec3): Vec3 => {
  out.x = a.x;
  out.y = a.y;
  out.z = a.z;
  return out;
};

export const addScaled = (out: Vec3, a: Vec3, s: number): Vec3 => {
  out.x += a.x * s;
  out.y += a.y * s;
  out.z += a.z * s;
  return out;
};

export const dist = (a: Vec3, b: Vec3): number => Math.hypot(a.x - b.x, a.y - b.y, a.z - b.z);

export const dist2 = (a: Vec3, b: Vec3): number => {
  const dx = a.x - b.x;
  const dy = a.y - b.y;
  const dz = a.z - b.z;
  return dx * dx + dy * dy + dz * dz;
};

export const forward = (heading: number): Vec3 => vec(Math.cos(heading), 0, Math.sin(heading));

/** Signed bearing from `heading` to the direction of `to` as seen from `from`, in radians (-PI..PI). */
export const bearingTo = (from: Vec3, heading: number, to: Vec3): number =>
  wrapAngle(Math.atan2(to.z - from.z, to.x - from.x) - heading);

export const wrapAngle = (a: number): number => {
  let r = a % TAU;
  if (r > Math.PI) r -= TAU;
  if (r < -Math.PI) r += TAU;
  return r;
};

export const TAU = Math.PI * 2;
export const DEG = Math.PI / 180;

export const clamp = (v: number, lo: number, hi: number): number => (v < lo ? lo : v > hi ? hi : v);

/**
 * Ports UE's FMath::FInterpConstantTo — moves `current` toward `target` at a fixed
 * rate rather than exponentially, which is what gives the ship its impulse feel.
 */
export const interpConstantTo = (current: number, target: number, dt: number, rate: number): number => {
  if (rate <= 0) return target;
  const delta = target - current;
  const step = rate * dt;
  if (Math.abs(delta) <= step) return target;
  return current + Math.sign(delta) * step;
};

/**
 * Deterministic PRNG (mulberry32). The simulation must never touch Math.random():
 * replay tests and host/crew agreement both depend on the same seed producing the
 * same sector, damage rolls, and AI jitter.
 */
export const makeRng = (seed: number) => {
  let s = seed >>> 0;
  return (): number => {
    s = (s + 0x6d2b79f5) >>> 0;
    let t = s;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
};

export type Rng = ReturnType<typeof makeRng>;
