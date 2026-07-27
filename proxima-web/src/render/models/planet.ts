// Planet and star surfaces, generated into a canvas at load.
//
// The bodies were flat-shaded spheres: correct geometry, no identity. Palettes and
// banding here are read off the generated references in art_src/refs (planet-haven,
// planet-rocky, planet-ice, star-ember) — but the surfaces themselves are procedural,
// so they cost a few kilobytes of code instead of four 2 MB textures, and every world
// can be recoloured from `data.ts` without touching an image.
//
// Deterministic: seeded per body, so Haven looks the same every session.

import {
  AdditiveBlending,
  BackSide,
  CanvasTexture,
  Color,
  Mesh,
  MeshBasicMaterial,
  MeshStandardMaterial,
  RepeatWrapping,
  SphereGeometry,
  Sprite,
  SpriteMaterial,
  SRGBColorSpace,
} from 'three';
import { makeRng } from '../../sim/math';
import type { LandmarkKind } from '../../sim/types';

const TEXTURE_W = 1024;
const TEXTURE_H = 512;

/** Value noise on a torus, so the texture tiles cleanly around the equator. */
const noiseField = (seed: number, cells: number): ((u: number, v: number) => number) => {
  const rng = makeRng(seed);
  const grid: number[] = [];
  for (let i = 0; i < cells * cells; i++) grid.push(rng());

  const at = (x: number, y: number): number =>
    grid[((y + cells) % cells) * cells + ((x + cells) % cells)]!;

  const smooth = (t: number): number => t * t * (3 - 2 * t);

  return (u, v) => {
    const x = u * cells;
    const y = v * cells;
    const x0 = Math.floor(x);
    const y0 = Math.floor(y);
    const fx = smooth(x - x0);
    const fy = smooth(y - y0);

    const top = at(x0, y0) * (1 - fx) + at(x0 + 1, y0) * fx;
    const bottom = at(x0, y0 + 1) * (1 - fx) + at(x0 + 1, y0 + 1) * fx;
    return top * (1 - fy) + bottom * fy;
  };
};

/** Several octaves of the same field, which is what turns blobs into terrain. */
const fbm = (seed: number, octaves: number): ((u: number, v: number) => number) => {
  const layers = Array.from({ length: octaves }, (_, i) => ({
    field: noiseField(seed + i * 131, 4 * 2 ** i),
    weight: 1 / 2 ** i,
  }));
  const total = layers.reduce((sum, l) => sum + l.weight, 0);

  return (u, v) => layers.reduce((sum, l) => sum + l.field(u, v) * l.weight, 0) / total;
};

export interface PlanetPalette {
  /** Low ground / deep ocean. */
  low: number;
  /** Mid elevation / continent. */
  mid: number;
  /** High ground / cloud / ice. */
  high: number;
  /** Polar cap colour; null for a body with no caps. */
  cap: number | null;
  /** How strongly the surface is banded along latitude (gas giants, dust bands). */
  banding: number;
}

export const PLANET_PALETTES: Record<string, PlanetPalette> = {
  // Haven: an ocean world with green continents and white cloud bands.
  haven: { low: 0x14406e, mid: 0x2f7a4a, high: 0xe8f0f5, cap: 0xf2f7fa, banding: 0.15 },
  // Tarsis: warm rock and dust, like the rocky reference.
  rocky: { low: 0x6b3115, mid: 0xa85a24, high: 0xd79a5c, cap: 0xe8ddcf, banding: 0.35 },
  // Korrin Belt: frozen, pale, with grey ridges showing through.
  ice: { low: 0x6d8296, mid: 0x9fb8c9, high: 0xeaf3f8, cap: 0xffffff, banding: 0.1 },
};

/** Builds an equirectangular surface map for a rocky/ocean/ice world. */
const surfaceTexture = (seed: number, palette: PlanetPalette): CanvasTexture => {
  const canvas = document.createElement('canvas');
  canvas.width = TEXTURE_W;
  canvas.height = TEXTURE_H;
  const ctx = canvas.getContext('2d')!;
  const image = ctx.createImageData(TEXTURE_W, TEXTURE_H);

  const height = fbm(seed, 5);
  const low = new Color(palette.low);
  const mid = new Color(palette.mid);
  const high = new Color(palette.high);
  const cap = palette.cap === null ? null : new Color(palette.cap);
  const scratch = new Color();

  for (let y = 0; y < TEXTURE_H; y++) {
    const v = y / TEXTURE_H;
    // Latitude: 0 at the equator, 1 at either pole.
    const latitude = Math.abs(v - 0.5) * 2;

    for (let x = 0; x < TEXTURE_W; x++) {
      const u = x / TEXTURE_W;

      // Bands run along latitude; noise breaks them up so they aren't stripes.
      const band = Math.sin(v * Math.PI * 7) * 0.5 + 0.5;
      let h = height(u, v) * (1 - palette.banding) + band * palette.banding;

      // Averaging octaves pulls every value toward 0.5, so without a contrast stretch
      // almost the whole sphere lands in the lowest colour band and reads as black.
      h = Math.min(1, Math.max(0, (h - 0.5) * 2.4 + 0.48));

      // Poles read colder/higher.
      h = Math.min(1, h + latitude * latitude * 0.35);

      if (h < 0.45) scratch.copy(low).lerp(mid, h / 0.45);
      else scratch.copy(mid).lerp(high, (h - 0.45) / 0.55);

      // Polar caps blend in over the last stretch of latitude.
      if (cap && latitude > 0.72) {
        scratch.lerp(cap, Math.min(1, (latitude - 0.72) / 0.2));
      }

      const i = (y * TEXTURE_W + x) * 4;
      image.data[i] = scratch.r * 255;
      image.data[i + 1] = scratch.g * 255;
      image.data[i + 2] = scratch.b * 255;
      image.data[i + 3] = 255;
    }
  }

  ctx.putImageData(image, 0, 0);
  const texture = new CanvasTexture(canvas);
  texture.colorSpace = SRGBColorSpace;
  texture.wrapS = RepeatWrapping;
  return texture;
};

/** A star: granulated convective cells rather than a flat disc. */
const starTexture = (seed: number, tint: number): CanvasTexture => {
  const canvas = document.createElement('canvas');
  canvas.width = TEXTURE_W;
  canvas.height = TEXTURE_H;
  const ctx = canvas.getContext('2d')!;
  const image = ctx.createImageData(TEXTURE_W, TEXTURE_H);

  const cells = fbm(seed, 6);
  const base = new Color(tint);
  const hot = new Color(0xfff3c4);
  const cool = new Color(0xc4501a);
  const scratch = new Color();

  for (let y = 0; y < TEXTURE_H; y++) {
    for (let x = 0; x < TEXTURE_W; x++) {
      const g = cells(x / TEXTURE_W, y / TEXTURE_H);
      // Granulation: bright cells with darker lanes between them.
      // Tighten the range so the surface is mostly bright with darker lanes, rather
      // than half-and-half blobs.
      const cell = Math.min(1, Math.max(0, (g - 0.42) * 3));
      scratch
        .copy(cool)
        .lerp(base, Math.min(1, cell * 2))
        .lerp(hot, Math.max(0, cell - 0.5) * 1.6);

      const i = (y * TEXTURE_W + x) * 4;
      image.data[i] = scratch.r * 255;
      image.data[i + 1] = scratch.g * 255;
      image.data[i + 2] = scratch.b * 255;
      image.data[i + 3] = 255;
    }
  }

  ctx.putImageData(image, 0, 0);
  const texture = new CanvasTexture(canvas);
  texture.colorSpace = SRGBColorSpace;
  texture.wrapS = RepeatWrapping;
  return texture;
};

/** Radial-gradient sprite texture — the falloff is what makes a glow a glow. */
const glowTexture = (): CanvasTexture => {
  const canvas = document.createElement('canvas');
  canvas.width = canvas.height = 256;
  const ctx = canvas.getContext('2d')!;

  const g = ctx.createRadialGradient(128, 128, 0, 128, 128, 128);
  g.addColorStop(0.0, 'rgba(255,255,255,0.95)');
  g.addColorStop(0.32, 'rgba(255,220,150,0.55)');
  g.addColorStop(0.62, 'rgba(255,150,60,0.18)');
  g.addColorStop(1.0, 'rgba(255,120,40,0)');
  ctx.fillStyle = g;
  ctx.fillRect(0, 0, 256, 256);

  const texture = new CanvasTexture(canvas);
  texture.colorSpace = SRGBColorSpace;
  return texture;
};

let sharedGlow: CanvasTexture | null = null;

export interface Body {
  root: Mesh;
  update(dt: number): void;
}

/**
 * A celestial body. Planets get a lit surface plus a thin atmospheric shell; stars get
 * an unlit granulated surface and a corona.
 */
export const createBody = (opts: {
  kind: LandmarkKind;
  radius: number;
  color: number;
  palette?: PlanetPalette;
  seed: number;
}): Body => {
  const isStar = opts.kind === 'sun';
  const geometry = new SphereGeometry(opts.radius, isStar ? 48 : 40, isStar ? 32 : 28);

  const material = isStar
    ? new MeshBasicMaterial({ map: starTexture(opts.seed, opts.color) })
    : (() => {
        const map = surfaceTexture(opts.seed, opts.palette ?? PLANET_PALETTES.rocky!);
        return new MeshStandardMaterial({
          map,
          // A planet is a navigation landmark here, not a lighting study: with a single
          // key light the night side goes pure black and the body reads as a hole.
          // A little self-illumination keeps it legible without killing the terminator.
          emissiveMap: map,
          emissive: 0xffffff,
          emissiveIntensity: 0.34,
          roughness: 0.92,
          metalness: 0.0,
        });
      })();

  const root = new Mesh(geometry, material);

  if (isStar) {
    // Corona as a billboarded radial gradient. A uniform-alpha shell has a hard outer
    // edge and reads as a flat brown ring; the falloff is the whole effect.
    sharedGlow ??= glowTexture();
    const corona = new Sprite(
      new SpriteMaterial({
        map: sharedGlow,
        color: opts.color,
        transparent: true,
        blending: AdditiveBlending,
        depthWrite: false,
      }),
    );
    corona.scale.setScalar(opts.radius * 5);
    root.add(corona);
  } else {
    // Atmospheric limb: a back-faced shell so it only shows at the edge of the disc,
    // which is what gives a planet its rim rather than a flat cut-out silhouette.
    const atmosphere = new Mesh(
      new SphereGeometry(opts.radius * 1.03, 32, 24),
      new MeshBasicMaterial({
        color: opts.color,
        transparent: true,
        opacity: 0.22,
        blending: AdditiveBlending,
        side: BackSide,
        depthWrite: false,
      }),
    );
    root.add(atmosphere);
  }

  return {
    root,
    update(dt: number) {
      // Slow axial spin, so a body is never a static billboard.
      root.rotation.y += dt * (isStar ? 0.006 : 0.012);
    },
  };
};
