// Transient combat effects. Everything here is pooled — beams and explosions are the
// only things that spawn during a fight, so allocating per shot would be the one
// reliable way to make the frame times spiky.

import {
  AdditiveBlending,
  BufferGeometry,
  Color,
  Float32BufferAttribute,
  Line,
  LineBasicMaterial,
  Mesh,
  MeshBasicMaterial,
  Object3D,
  Scene,
  SphereGeometry,
} from 'three';
import { BEAM_DRAW_TIME } from '../sim/data';
import type { SimEvent } from '../sim/types';

const BEAM_POOL = 24;
const BLAST_POOL = 16;
const BLAST_LIFE = 0.7;

interface Pooled<T extends Object3D> {
  obj: T;
  life: number;
}

export class CombatFx {
  private readonly beams: Pooled<Line>[] = [];
  private readonly blasts: Pooled<Mesh>[] = [];

  constructor(scene: Scene) {
    const friendly = new LineBasicMaterial({ color: 0x66ddff, transparent: true, blending: AdditiveBlending });

    for (let i = 0; i < BEAM_POOL; i++) {
      const geo = new BufferGeometry();
      geo.setAttribute('position', new Float32BufferAttribute(new Float32Array(6), 3));
      const line = new Line(geo, friendly.clone());
      line.visible = false;
      line.frustumCulled = false;
      scene.add(line);
      this.beams.push({ obj: line, life: 0 });
    }

    const blastGeo = new SphereGeometry(1, 16, 12);
    for (let i = 0; i < BLAST_POOL; i++) {
      const mesh = new Mesh(
        blastGeo,
        new MeshBasicMaterial({ color: 0xffaa44, transparent: true, blending: AdditiveBlending }),
      );
      mesh.visible = false;
      scene.add(mesh);
      this.blasts.push({ obj: mesh, life: 0 });
    }
  }

  /** Consumes one tick's worth of sim events. */
  ingest(events: SimEvent[]): void {
    for (const ev of events) {
      if (ev.t === 'beam') {
        const slot = this.beams.find((b) => b.life <= 0);
        if (!slot) continue;

        const pos = slot.obj.geometry.getAttribute('position');
        pos.setXYZ(0, ev.from.x, ev.from.y, ev.from.z);
        pos.setXYZ(1, ev.to.x, ev.to.y, ev.to.z);
        pos.needsUpdate = true;

        (slot.obj.material as LineBasicMaterial).color = new Color(ev.friendly ? 0x66ddff : 0xff5544);
        slot.obj.visible = true;
        slot.life = BEAM_DRAW_TIME;
      } else if (ev.t === 'kill' || ev.t === 'hit') {
        const slot = this.blasts.find((b) => b.life <= 0);
        if (!slot) continue;

        slot.obj.position.set(ev.pos.x, ev.pos.y, ev.pos.z);
        slot.obj.scale.setScalar(ev.t === 'kill' ? 1400 : 320);
        slot.obj.visible = true;
        slot.life = ev.t === 'kill' ? BLAST_LIFE : BLAST_LIFE * 0.35;
      }
    }
  }

  update(dt: number): void {
    for (const b of this.beams) {
      if (b.life <= 0) continue;
      b.life -= dt;
      const mat = b.obj.material as LineBasicMaterial;
      mat.opacity = Math.max(0, b.life / BEAM_DRAW_TIME);
      if (b.life <= 0) b.obj.visible = false;
    }

    for (const b of this.blasts) {
      if (b.life <= 0) continue;
      b.life -= dt;
      const t = Math.max(0, b.life / BLAST_LIFE);
      const mat = b.obj.material as MeshBasicMaterial;
      mat.opacity = t;
      // Expand as it fades, so a kill reads as a shockwave rather than a fading ball.
      b.obj.scale.multiplyScalar(1 + dt * 2.2);
      if (b.life <= 0) b.obj.visible = false;
    }
  }
}
