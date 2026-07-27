// The Three.js view of the sector. Reads snapshots, owns no game state.
//
// Performance notes: no shadow maps (nothing in space casts a useful one), a single
// Points cloud for the starfield, one shared material per hull type via cloned
// prototypes, and a logarithmic depth buffer because the sector spans ~220 000 units
// while a ship is ~1 000.

import {
  AmbientLight,
  BufferGeometry,
  Color,
  DirectionalLight,
  Float32BufferAttribute,
  Fog,
  Mesh,
  MeshBasicMaterial,
  type Object3D,
  PerspectiveCamera,
  Points,
  PointsMaterial,
  Scene,
  SphereGeometry,
  Vector3,
  WebGLRenderer,
} from 'three';
import { ENEMIES, shipDef } from '../sim/data';
import type { Vec3 } from '../sim/math';
import { makeRng } from '../sim/math';
import type { SimEvent, Snapshot } from '../sim/types';
import { CombatFx } from './fx';
import { type Body, createBody, PLANET_PALETTES } from './models/planet';
import { createStarbase, type Starbase } from './models/starbase';
import { makeShip, preloadShipModels } from './ships';

const STARFIELD_COUNT = 4000;
const STARFIELD_RADIUS = 400000;

// Camera shake (Ships/Spaceship.h): hits add trauma, which decays and drives a
// per-axis wobble. Trauma rather than a fixed shake means a big hit reads bigger.
const TRAUMA_DECAY_PER_SEC = 1.6;
const HIT_TRAUMA_PER_DAMAGE = 0.04;
const MAX_SHAKE_PITCH = 1.6;
const MAX_SHAKE_YAW = 1.6;
const MAX_SHAKE_ROLL = 2.4;

const TORPEDO_POOL = 24;

/**
 * How far behind the newest snapshot the render clock runs, in seconds.
 *
 * The sim ticks at a fixed 60Hz in a worker; the renderer draws on rAF. The two are
 * not phase-locked, so some frames reuse a snapshot and others skip one — which at
 * 2100 units/second is a very visible stutter, worst while accelerating or turning
 * because the per-tick delta is changing. Rendering slightly in the past means there
 * is always a later sample to interpolate toward, which removes the judder entirely
 * at the cost of ~33ms of latency nobody can perceive.
 */
const INTERP_DELAY = 2 / 60;

export class SectorView {
  readonly scene = new Scene();
  readonly camera: PerspectiveCamera;
  private readonly renderer: WebGLRenderer;
  private readonly fx: CombatFx;
  private readonly contacts = new Map<number, Object3D>();
  /** Static sector bodies, tracked so a new game can tear them down. */
  private readonly bodies: Object3D[] = [];
  /** Bodies and stations that animate (axial spin, ring rotation). */
  private readonly animated: (Body | Starbase)[] = [];
  private playerMesh: Object3D | null = null;
  private playerModel = '';

  /** Smoothed camera state, so the view lags the ship instead of snapping to it. */
  private readonly camPos = new Vector3();
  private readonly camLook = new Vector3();
  private primed = false;

  private trauma = 0;
  private readonly shakeRng = makeRng(31);
  private readonly torpedoes: Mesh[] = [];

  /** Recent snapshots, oldest first, used to interpolate between fixed sim ticks. */
  private readonly buffer: Snapshot[] = [];
  private renderTime = 0;
  private clockPrimed = false;

  constructor(canvas: HTMLCanvasElement) {
    this.renderer = new WebGLRenderer({
      canvas,
      antialias: true,
      powerPreference: 'high-performance',
      logarithmicDepthBuffer: true,
    });
    this.renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
    this.renderer.shadowMap.enabled = false;

    this.camera = new PerspectiveCamera(60, 1, 10, 800000);
    this.scene.fog = new Fog(0x05070f, 60000, 260000);
    this.scene.background = new Color(0x05070f);

    this.scene.add(new AmbientLight(0x404a66, 1.4));
    const key = new DirectionalLight(0xfff0dd, 2.2);
    key.position.set(1, 0.6, 0.4);
    this.scene.add(key);

    this.scene.add(makeStarfield());
    this.fx = new CombatFx(this.scene);

    // Torpedoes are pooled: a volley spawns three at once and they're all identical.
    const torpGeo = new SphereGeometry(90, 10, 8);
    for (let i = 0; i < TORPEDO_POOL; i++) {
      const mesh = new Mesh(torpGeo, new MeshBasicMaterial({ color: 0xffdd88 }));
      mesh.visible = false;
      this.scene.add(mesh);
      this.torpedoes.push(mesh);
    }

    this.resize();
    window.addEventListener('resize', () => this.resize());
  }

  /** Render quality: caps the device pixel ratio, which is the cheapest real lever. */
  setQuality(cap: number): void {
    this.renderer.setPixelRatio(Math.min(devicePixelRatio, cap));
  }

  /** Buffers a snapshot for interpolation. Call once per state message. */
  pushSnapshot(snap: Snapshot): void {
    this.buffer.push(snap);
    // A little over the interpolation window; anything older can never be needed.
    while (this.buffer.length > 12) this.buffer.shift();
  }

  /** One tick's sim events, handed straight to the effect pools. */
  ingest(events: SimEvent[]): void {
    this.fx.ingest(events);

    // Hull hits shake the bridge. Scaled by damage and clamped, so a torpedo rattles
    // the camera and a glancing beam barely registers.
    for (const ev of events) {
      if (ev.t === 'hit')
        this.trauma = Math.min(1, this.trauma + ev.damage * HIT_TRAUMA_PER_DAMAGE);
      else if (ev.t === 'kill') this.trauma = Math.min(1, this.trauma + 0.25);
    }
  }

  /**
   * Clears everything the previous run built, so starting a new game doesn't leave
   * the old sector's hostiles and bodies floating in the new one.
   */
  reset(): void {
    this.buffer.length = 0;
    this.clockPrimed = false;
    this.animated.length = 0;
    for (const obj of this.contacts.values()) this.scene.remove(obj);
    this.contacts.clear();

    if (this.playerMesh) {
      this.scene.remove(this.playerMesh);
      this.playerMesh = null;
      this.playerModel = '';
    }

    for (const body of this.bodies) this.scene.remove(body);
    this.bodies.length = 0;

    this.primed = false;
    this.camPos.set(0, 0, 0);
    this.camLook.set(0, 0, 0);
  }

  async load(): Promise<void> {
    await preloadShipModels();
  }

  private resize(): void {
    const w = innerWidth;
    const h = innerHeight;
    this.renderer.setSize(w, h, false);
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
  }

  /** Builds the static bodies once; they never move, so this runs on first snapshot. */
  private buildLandmarks(snap: Snapshot): void {
    snap.landmarks.forEach((l, index) => {
      // The starbase is a built object, not a body — it gets the procedural model.
      if (l.kind === 'station') {
        const station = createStarbase();
        station.root.position.set(l.pos.x, l.pos.y, l.pos.z);
        // The model is authored ~2 units across; scale it to the landmark's radius.
        station.root.scale.setScalar(l.radius * 1.6);
        this.scene.add(station.root);
        this.bodies.push(station.root);
        this.animated.push(station);
        return;
      }

      const body = createBody({
        kind: l.kind,
        radius: l.radius,
        color: l.color,
        palette: PLANET_PALETTES[l.surface],
        seed: 1000 + index * 37,
      });
      body.root.position.set(l.pos.x, l.pos.y, l.pos.z);
      this.scene.add(body.root);
      this.bodies.push(body.root);
      this.animated.push(body);
    });
  }

  update(snap: Snapshot, dt: number): void {
    if (!this.primed) {
      this.buildLandmarks(snap);
      this.primed = true;
    }

    const view = this.interpolated(dt) ?? snap;

    this.syncPlayer(view);
    this.syncContacts(view);
    this.syncTorpedoes(view);
    this.updateCamera(view, dt);
    for (const body of this.animated) body.update(dt);
    this.fx.update(dt);
    this.renderer.render(this.scene, this.camera);
  }

  /**
   * A view of the world at `renderTime`, blended between the two snapshots that
   * bracket it. Returns null until there is enough history to interpolate.
   */
  private interpolated(dt: number): Snapshot | null {
    const newest = this.buffer[this.buffer.length - 1];
    if (!newest || this.buffer.length < 2) return null;

    const target = newest.time - INTERP_DELAY;
    if (!this.clockPrimed) {
      this.renderTime = target;
      this.clockPrimed = true;
    } else {
      this.renderTime += dt;
      // Resync rather than drift: running ahead means extrapolating, and falling far
      // behind means the ship visibly lags the controls.
      if (this.renderTime > target + 0.05 || this.renderTime < target - 0.5) {
        this.renderTime = target;
      }
    }

    let older = this.buffer[0]!;
    let newer = newest;
    for (let i = 1; i < this.buffer.length; i++) {
      if (this.buffer[i]!.time >= this.renderTime) {
        older = this.buffer[i - 1]!;
        newer = this.buffer[i]!;
        break;
      }
    }

    const span = newer.time - older.time;
    const alpha = span > 0 ? Math.min(1, Math.max(0, (this.renderTime - older.time) / span)) : 1;
    return blend(older, newer, alpha);
  }

  private syncPlayer(snap: Snapshot): void {
    const def = shipDef(snap.player.shipType);

    // Swapping hulls at the drydock is just a different prototype clone.
    if (this.playerModel !== def.model) {
      if (this.playerMesh) this.scene.remove(this.playerMesh);
      this.playerMesh = makeShip(def.model, def.scale);
      this.playerModel = def.model;
      this.scene.add(this.playerMesh);
    }

    const p = this.playerMesh!;
    p.position.set(snap.player.pos.x, snap.player.pos.y, snap.player.pos.z);
    p.rotation.y = -snap.player.heading;
  }

  private syncContacts(snap: Snapshot): void {
    const seen = new Set<number>();

    for (const c of snap.contacts) {
      seen.add(c.id);
      let obj = this.contacts.get(c.id);
      if (!obj) {
        // The snapshot doesn't carry the archetype, so resolve it by display name.
        const def = Object.values(ENEMIES).find((e) => e.name === c.name) ?? ENEMIES.scout;
        obj = makeShip(def.model, def.scale);
        this.contacts.set(c.id, obj);
        this.scene.add(obj);
      }
      obj.position.set(c.pos.x, c.pos.y, c.pos.z);
      obj.rotation.y = -c.heading;
    }

    // Anything gone from the snapshot died or left sensor range.
    for (const [id, obj] of this.contacts) {
      if (seen.has(id)) continue;
      this.scene.remove(obj);
      this.contacts.delete(id);
    }
  }

  private syncTorpedoes(snap: Snapshot): void {
    snap.torpedoes.forEach((t, i) => {
      const mesh = this.torpedoes[i];
      if (!mesh) return;
      mesh.visible = true;
      mesh.position.set(t.pos.x, t.pos.y, t.pos.z);
      (mesh.material as MeshBasicMaterial).color.setHex(t.friendly ? 0xffdd88 : 0xff7755);
    });
    for (let i = snap.torpedoes.length; i < this.torpedoes.length; i++) {
      this.torpedoes[i]!.visible = false;
    }
  }

  private updateCamera(snap: Snapshot, dt: number): void {
    const p = snap.player;
    const back = 3200;
    const up = 1100;

    const targetX = p.pos.x - Math.cos(p.heading) * back;
    const targetZ = p.pos.z - Math.sin(p.heading) * back;

    const want = new Vector3(targetX, p.pos.y + up, targetZ);
    const look = new Vector3(p.pos.x, p.pos.y, p.pos.z);

    // Exponential smoothing, frame-rate independent.
    const k = 1 - 0.0015 ** dt;
    this.camPos.lerp(want, this.camPos.lengthSq() === 0 ? 1 : k);
    this.camLook.lerp(look, this.camLook.lengthSq() === 0 ? 1 : k);

    this.camera.position.copy(this.camPos);
    this.camera.lookAt(this.camLook);

    // Apply the shake after aiming, so it rattles the view rather than the target.
    if (this.trauma > 0) {
      this.trauma = Math.max(0, this.trauma - TRAUMA_DECAY_PER_SEC * dt);
      // Squared falls off faster than linear, which reads as a jolt settling.
      const t = this.trauma * this.trauma;
      const jitter = () => (this.shakeRng() * 2 - 1) * t * DEG_TO_RAD;
      this.camera.rotation.x += jitter() * MAX_SHAKE_PITCH;
      this.camera.rotation.y += jitter() * MAX_SHAKE_YAW;
      this.camera.rotation.z += jitter() * MAX_SHAKE_ROLL;
    }
  }
}

const DEG_TO_RAD = Math.PI / 180;

export const mix = (a: number, b: number, t: number): number => a + (b - a) * t;

const mixVec = (a: Vec3, b: Vec3, t: number): Vec3 => ({
  x: mix(a.x, b.x, t),
  y: mix(a.y, b.y, t),
  z: mix(a.z, b.z, t),
});

/** Angles must take the short way round, or a ship crossing 0 spins the long way. */
export const mixAngle = (a: number, b: number, t: number): number => {
  let delta = (b - a) % (Math.PI * 2);
  if (delta > Math.PI) delta -= Math.PI * 2;
  if (delta < -Math.PI) delta += Math.PI * 2;
  return a + delta * t;
};

/**
 * Blends the *positional* parts of two snapshots. Everything else (hull numbers, the
 * comms log, arcs) comes from the newer sample — those are read, not watched moving,
 * and interpolating them would only make readouts lag.
 */
export const blend = (a: Snapshot, b: Snapshot, t: number): Snapshot => ({
  ...b,
  player: {
    ...b.player,
    pos: mixVec(a.player.pos, b.player.pos, t),
    heading: mixAngle(a.player.heading, b.player.heading, t),
  },
  contacts: b.contacts.map((contact) => {
    const before = a.contacts.find((c) => c.id === contact.id);
    // A contact that only exists in the newer sample just appeared; show it there.
    if (!before) return contact;
    return {
      ...contact,
      pos: mixVec(before.pos, contact.pos, t),
      heading: mixAngle(before.heading, contact.heading, t),
    };
  }),
  torpedoes: b.torpedoes.map((torp) => {
    const before = a.torpedoes.find((x) => x.id === torp.id);
    if (!before) return torp;
    return { ...torp, pos: mixVec(before.pos, torp.pos, t) };
  }),
});

const makeStarfield = (): Points => {
  const rng = makeRng(7);
  const positions = new Float32Array(STARFIELD_COUNT * 3);

  for (let i = 0; i < STARFIELD_COUNT; i++) {
    // Uniform on a sphere shell, far enough out to read as fixed background.
    const theta = rng() * Math.PI * 2;
    const phi = Math.acos(2 * rng() - 1);
    const r = STARFIELD_RADIUS * (0.7 + rng() * 0.3);
    positions[i * 3] = r * Math.sin(phi) * Math.cos(theta);
    positions[i * 3 + 1] = r * Math.cos(phi);
    positions[i * 3 + 2] = r * Math.sin(phi) * Math.sin(theta);
  }

  const geo = new BufferGeometry();
  geo.setAttribute('position', new Float32BufferAttribute(positions, 3));

  const stars = new Points(
    geo,
    new PointsMaterial({ color: 0xffffff, size: 900, sizeAttenuation: true, fog: false }),
  );
  stars.frustumCulled = false;
  return stars;
};
