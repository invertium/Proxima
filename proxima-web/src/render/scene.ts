// The Three.js view of the sector. Reads snapshots, owns no game state.
//
// Performance notes: no shadow maps (nothing in space casts a useful one), a single
// Points cloud for the starfield, one shared material per hull type via cloned
// prototypes, and a logarithmic depth buffer because the sector spans ~220 000 units
// while a ship is ~1 000.

import {
  AdditiveBlending,
  AmbientLight,
  BufferGeometry,
  Color,
  DirectionalLight,
  Float32BufferAttribute,
  Fog,
  Mesh,
  MeshBasicMaterial,
  MeshStandardMaterial,
  Object3D,
  PerspectiveCamera,
  Points,
  PointsMaterial,
  Scene,
  SphereGeometry,
  Vector3,
  WebGLRenderer,
} from 'three';
import { CombatFx } from './fx';
import { makeShip, preloadShipModels } from './ships';
import { ENEMIES, shipDef } from '../sim/data';
import { makeRng } from '../sim/math';
import type { SimEvent, Snapshot } from '../sim/types';

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

export class SectorView {
  readonly scene = new Scene();
  readonly camera: PerspectiveCamera;
  private readonly renderer: WebGLRenderer;
  private readonly fx: CombatFx;
  private readonly contacts = new Map<number, Object3D>();
  /** Static sector bodies, tracked so a new game can tear them down. */
  private readonly bodies: Object3D[] = [];
  private playerMesh: Object3D | null = null;
  private playerModel = '';

  /** Smoothed camera state, so the view lags the ship instead of snapping to it. */
  private readonly camPos = new Vector3();
  private readonly camLook = new Vector3();
  private primed = false;

  private trauma = 0;
  private readonly shakeRng = makeRng(31);
  private readonly torpedoes: Mesh[] = [];

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

  /** One tick's sim events, handed straight to the effect pools. */
  ingest(events: SimEvent[]): void {
    this.fx.ingest(events);

    // Hull hits shake the bridge. Scaled by damage and clamped, so a torpedo rattles
    // the camera and a glancing beam barely registers.
    for (const ev of events) {
      if (ev.t === 'hit') this.trauma = Math.min(1, this.trauma + ev.damage * HIT_TRAUMA_PER_DAMAGE);
      else if (ev.t === 'kill') this.trauma = Math.min(1, this.trauma + 0.25);
    }
  }

  /**
   * Clears everything the previous run built, so starting a new game doesn't leave
   * the old sector's hostiles and bodies floating in the new one.
   */
  reset(): void {
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
    for (const l of snap.landmarks) {
      const isSun = l.kind === 'sun';
      const geo = new SphereGeometry(l.radius, isSun ? 48 : 32, isSun ? 32 : 24);
      const mat = isSun
        ? new MeshBasicMaterial({ color: l.color })
        : new MeshStandardMaterial({
            color: l.color,
            roughness: 0.85,
            metalness: 0.05,
            emissive: new Color(l.color).multiplyScalar(0.08),
          });

      const body = new Mesh(geo, mat);
      body.position.set(l.pos.x, l.pos.y, l.pos.z);
      this.scene.add(body);
      this.bodies.push(body);

      if (isSun) {
        // A cheap corona: one oversized additive shell, no post-processing pass.
        const glow = new Mesh(
          new SphereGeometry(l.radius * 1.5, 32, 24),
          new MeshBasicMaterial({ color: l.color, transparent: true, opacity: 0.18, blending: AdditiveBlending }),
        );
        body.add(glow);
      }
    }
  }

  update(snap: Snapshot, dt: number): void {
    if (!this.primed) {
      this.buildLandmarks(snap);
      this.primed = true;
    }

    this.syncPlayer(snap);
    this.syncContacts(snap);
    this.syncTorpedoes(snap);
    this.updateCamera(snap, dt);
    this.fx.update(dt);
    this.renderer.render(this.scene, this.camera);
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
    const k = 1 - Math.pow(0.0015, dt);
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
