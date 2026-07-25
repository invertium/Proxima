// The console scope: a bow-up tactical radar, or the whole-sector navigation map.
//
// Canvas, so it repaints wholesale every frame without touching a single DOM node —
// which is exactly why the interactive controls must live outside it.

import { BEAM_RANGE, SECTOR_SPAN, TORPEDO_ARC_DEG } from '../../sim/data';
import type { Snapshot } from '../../sim/types';

export class Scope {
  private readonly ctx: CanvasRenderingContext2D;

  constructor(private readonly canvas: HTMLCanvasElement) {
    this.ctx = canvas.getContext('2d')!;
  }

  draw(s: Snapshot, mode: 'tactical' | 'map'): void {
    const dpr = Math.min(devicePixelRatio, 2);
    const size = this.canvas.clientWidth;
    if (size <= 0) return;
    if (this.canvas.width !== size * dpr) this.canvas.width = this.canvas.height = size * dpr;

    this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    this.ctx.clearRect(0, 0, size, size);

    if (mode === 'map') this.drawMap(s, size);
    else this.drawTactical(s, size);
  }

  private drawTactical(s: Snapshot, size: number): void {
    const ctx = this.ctx;
    const cx = size / 2;
    const cy = size / 2;
    // Sensor damage shrinks the scope. The stat existed and was being thrown away.
    const range = s.player.stats.radarRange;
    const scale = size / 2 / range;

    ctx.strokeStyle = '#12283f';
    ctx.lineWidth = 1;
    for (let r = 1; r <= 3; r++) {
      ctx.beginPath();
      ctx.arc(cx, cy, (size / 2) * (r / 3), 0, Math.PI * 2);
      ctx.stroke();
    }
    ctx.font = '10px ui-monospace, monospace';
    ctx.fillStyle = '#3d5a7d';
    ctx.fillText(`${(range / 1000).toFixed(0)} km`, cx + 4, cy - (size / 2) * 0.98 + 12);

    // Both arcs: torpedoes cover a wider cone than the beam, which is the whole reason
    // to draw them separately.
    const wedge = (arcDeg: number, radius: number, fill: string) => {
      const half = (arcDeg / 2) * (Math.PI / 180);
      ctx.fillStyle = fill;
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.arc(cx, cy, radius * scale, -Math.PI / 2 - half, -Math.PI / 2 + half);
      ctx.closePath();
      ctx.fill();
    };
    wedge(TORPEDO_ARC_DEG, BEAM_RANGE * 0.8, 'rgba(251, 191, 36, .07)');
    wedge(s.player.stats.beamArcDeg, BEAM_RANGE, 'rgba(56, 189, 248, .11)');

    const project = (p: { x: number; z: number }) => {
      const dx = p.x - s.player.pos.x;
      const dz = p.z - s.player.pos.z;
      const a = -s.player.heading - Math.PI / 2;
      return {
        x: cx + (dx * Math.cos(a) - dz * Math.sin(a)) * scale,
        y: cy + (dx * Math.sin(a) + dz * Math.cos(a)) * scale,
      };
    };

    for (const l of s.landmarks) {
      const p = project(l.pos);
      ctx.fillStyle = l.kind === 'station' ? '#4ade80' : '#2e4a6b';
      ctx.beginPath();
      ctx.arc(p.x, p.y, 5, 0, Math.PI * 2);
      ctx.fill();
      ctx.fillStyle = '#4a6c92';
      ctx.fillText(l.name, p.x + 8, p.y + 3);
    }

    if (s.event) {
      const p = project(s.event.pos);
      ctx.strokeStyle = '#fbbf24';
      ctx.beginPath();
      ctx.arc(p.x, p.y, 9, 0, Math.PI * 2);
      ctx.stroke();
      ctx.fillStyle = '#fbbf24';
      ctx.fillText(s.event.kind.toUpperCase(), p.x + 12, p.y + 3);
    }

    for (const t of s.torpedoes) {
      const p = project(t.pos);
      ctx.fillStyle = t.friendly ? '#fde68a' : '#fb7185';
      ctx.fillRect(p.x - 2, p.y - 2, 4, 4);
    }

    for (const c of s.contacts) {
      const p = project(c.pos);
      const selected = c.id === s.player.targetId;
      ctx.fillStyle = selected ? '#fbbf24' : c.hostile ? '#f87171' : '#94a3b8';
      ctx.beginPath();
      ctx.arc(p.x, p.y, selected ? 6 : 4, 0, Math.PI * 2);
      ctx.fill();

      if (selected) {
        ctx.strokeStyle = '#fbbf24';
        ctx.beginPath();
        ctx.arc(p.x, p.y, 11, 0, Math.PI * 2);
        ctx.stroke();
      }
      if (c.scanned) {
        ctx.strokeStyle = '#34d399';
        ctx.strokeRect(p.x - 8, p.y - 8, 16, 16);
      }
      ctx.fillStyle = selected ? '#fde68a' : '#8fa8c4';
      ctx.fillText(c.name, p.x + 9, p.y - 6);
    }

    // Own ship.
    ctx.fillStyle = '#4ade80';
    ctx.beginPath();
    ctx.moveTo(cx, cy - 8);
    ctx.lineTo(cx - 5, cy + 6);
    ctx.lineTo(cx + 5, cy + 6);
    ctx.closePath();
    ctx.fill();
  }

  private drawMap(s: Snapshot, size: number): void {
    const ctx = this.ctx;
    const pad = 26;
    const toScreen = (p: { x: number; z: number }) => ({
      x: pad + ((p.x + SECTOR_SPAN / 2) / SECTOR_SPAN) * (size - pad * 2),
      y: pad + ((p.z + SECTOR_SPAN / 2) / SECTOR_SPAN) * (size - pad * 2),
    });

    ctx.strokeStyle = '#12283f';
    ctx.strokeRect(pad, pad, size - pad * 2, size - pad * 2);
    ctx.fillStyle = '#4a6c92';
    ctx.font = '10px ui-monospace, monospace';
    ctx.fillText('SECTOR — THE VEIL', pad, pad - 8);

    const me = toScreen(s.player.pos);

    // A dashed course line to the objective, so the helm can see the heading it wants.
    if (s.objective) {
      const target = toScreen(s.objective.pos);
      ctx.strokeStyle = '#3f4f2a';
      ctx.setLineDash([4, 5]);
      ctx.beginPath();
      ctx.moveTo(me.x, me.y);
      ctx.lineTo(target.x, target.y);
      ctx.stroke();
      ctx.setLineDash([]);
    }

    for (const l of s.landmarks) {
      const p = toScreen(l.pos);
      const isObjective = s.objective?.name === l.name;
      ctx.fillStyle = l.kind === 'sun' ? '#fbbf24' : l.kind === 'station' ? '#4ade80' : '#38bdf8';
      ctx.beginPath();
      ctx.arc(p.x, p.y, l.kind === 'sun' ? 9 : 6, 0, Math.PI * 2);
      ctx.fill();

      if (isObjective) {
        ctx.strokeStyle = '#fbbf24';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.arc(p.x, p.y, 14, 0, Math.PI * 2);
        ctx.stroke();
        ctx.lineWidth = 1;
      }
      ctx.fillStyle = isObjective ? '#fde68a' : '#7d9dc4';
      const label = isObjective && s.objective ? `${l.name} · ${(s.objective.range / 1000).toFixed(0)} km` : l.name;
      ctx.fillText(label, p.x + 11, p.y + 3);
    }

    if (s.event) {
      const p = toScreen(s.event.pos);
      ctx.fillStyle = '#fbbf24';
      ctx.beginPath();
      ctx.moveTo(p.x, p.y - 7);
      ctx.lineTo(p.x + 7, p.y);
      ctx.lineTo(p.x, p.y + 7);
      ctx.lineTo(p.x - 7, p.y);
      ctx.closePath();
      ctx.fill();
    }

    ctx.save();
    ctx.translate(me.x, me.y);
    ctx.rotate(s.player.heading + Math.PI / 2);
    ctx.fillStyle = '#4ade80';
    ctx.beginPath();
    ctx.moveTo(0, -8);
    ctx.lineTo(-5, 6);
    ctx.lineTo(5, 6);
    ctx.closePath();
    ctx.fill();
    ctx.restore();
  }
}
