/**
 * N64MaterialBridge.ts
 * Translates Pyrite64 / Fast64 material definitions into Three.js materials
 * for the editor viewport preview.
 *
 * Three render modes are supported:
 *  - standard:     PBR-like preview (editor default)
 *  - cartoon:      MeshToonMaterial with cel-shade bands (matches N64 combiner trick)
 *  - n64-accurate: Quantized vertex colors, no mip-maps, nearest filtering
 */

import * as THREE from 'three';

// ─── Fast64 / Pyrite64 material types ────────────────────────────────────────

export type CombinerMode =
  | 'SHADE'               // vertex color only
  | 'TEXTURE'             // texture only
  | 'TEXTURE_SHADE'       // texture * vertex color
  | 'ENV_COLOR'           // flat environment color
  | 'CEL_SHADE';          // cartoon bands (Pyrite extension)

export interface PyriteN64Material {
  id:           string;
  name:         string;
  combinerMode: CombinerMode;
  textureId?:   string;    // reference into AssetRegistry
  texturePath?: string;    // resolved path
  envColor?:    [number, number, number, number];  // RGBA 0–255
  celBands?:    number;    // 2–8, only for CEL_SHADE mode
  alphaBlend:   boolean;
  doubleSided:  boolean;
}

// ─── Internal cache key ───────────────────────────────────────────────────────

type MaterialCacheKey = string;

// ─── N64MaterialBridge ────────────────────────────────────────────────────────

export class N64MaterialBridge {
  private standardCache = new Map<MaterialCacheKey, THREE.Material>();
  private cartoonCache  = new Map<MaterialCacheKey, THREE.Material>();
  private n64Cache      = new Map<MaterialCacheKey, THREE.Material>();
  private texCache      = new Map<string, THREE.Texture>();

  // ── Factories ──────────────────────────────────────────────────────────────

  /** Default grey Phong — used for meshes before a material is assigned. */
  createDefault(): THREE.Material {
    return new THREE.MeshPhongMaterial({
      color:    0x888888,
      specular: 0x111111,
    });
  }

  /** Build or retrieve the Standard (PBR-like) Three.js material. */
  fromN64Material(def: PyriteN64Material): THREE.Material {
    const key = `std_${def.id}`;
    if (this.standardCache.has(key)) return this.standardCache.get(key)!;

    let mat: THREE.Material;

    switch (def.combinerMode) {
      case 'TEXTURE':
      case 'TEXTURE_SHADE': {
        const tex = def.texturePath ? this.loadTexture(def.texturePath) : null;
        mat = new THREE.MeshPhongMaterial({
          map:         tex ?? undefined,
          vertexColors: def.combinerMode === 'TEXTURE_SHADE',
          transparent:  def.alphaBlend,
          side:         def.doubleSided ? THREE.DoubleSide : THREE.FrontSide,
        });
        break;
      }
      case 'ENV_COLOR': {
        const [r, g, b] = (def.envColor ?? [128, 128, 128, 255]).map(v => v / 255);
        mat = new THREE.MeshBasicMaterial({
          color:       new THREE.Color(r, g, b),
          transparent: def.alphaBlend,
          side:        def.doubleSided ? THREE.DoubleSide : THREE.FrontSide,
        });
        break;
      }
      case 'CEL_SHADE':
        mat = this.buildCartoonMat(def);
        break;
      default:  // SHADE
        mat = new THREE.MeshPhongMaterial({
          vertexColors: true,
          transparent:  def.alphaBlend,
          side:         def.doubleSided ? THREE.DoubleSide : THREE.FrontSide,
        });
    }

    this.standardCache.set(key, mat);
    return mat;
  }

  // ── Mode converters ────────────────────────────────────────────────────────

  /** Convert any material to cartoon mode (cel-shaded). */
  toCartoon(source: THREE.Material): THREE.Material {
    const key = `cel_${source.uuid}`;
    if (this.cartoonCache.has(key)) return this.cartoonCache.get(key)!;

    const toon = new THREE.MeshToonMaterial();
    toon.gradientMap = this.buildGradientMap(4);  // 4 bands default

    if (source instanceof THREE.MeshPhongMaterial || source instanceof THREE.MeshStandardMaterial) {
      toon.color.copy(source.color);
      if ('map' in source) toon.map = (source as THREE.MeshPhongMaterial).map;
    }
    toon.transparent = source.transparent;
    toon.side        = source.side;

    this.cartoonCache.set(key, toon);
    return toon;
  }

  /** Convert any material to N64-accurate mode: nearest filtering, no mipmaps. */
  toN64Accurate(source: THREE.Material): THREE.Material {
    const key = `n64_${source.uuid}`;
    if (this.n64Cache.has(key)) return this.n64Cache.get(key)!;

    const mat = source.clone();

    // Enforce nearest-neighbor filtering (N64 has no bilinear by default)
    mat.traverse?.((child) => {
      if (child instanceof THREE.Texture) {
        child.minFilter = THREE.NearestFilter;
        child.magFilter = THREE.NearestFilter;
        child.generateMipmaps = false;
        child.needsUpdate = true;
      }
    });

    // Also apply to any mapped textures directly
    if ('map' in mat && (mat as THREE.MeshPhongMaterial).map) {
      const tex = (mat as THREE.MeshPhongMaterial).map!;
      tex.minFilter = THREE.NearestFilter;
      tex.magFilter = THREE.NearestFilter;
      tex.generateMipmaps = false;
      tex.needsUpdate = true;
    }

    this.n64Cache.set(key, mat);
    return mat;
  }

  /** Convert back to standard mode. */
  toStandard(source: THREE.Material): THREE.Material {
    // If we have the original in the standard cache, return it
    // Otherwise just return the source (it may already be standard)
    return source;
  }

  // ── Helpers ────────────────────────────────────────────────────────────────

  private buildCartoonMat(def: PyriteN64Material): THREE.MeshToonMaterial {
    const toon = new THREE.MeshToonMaterial();
    const bands = def.celBands ?? 4;
    toon.gradientMap = this.buildGradientMap(bands);

    if (def.texturePath) {
      toon.map = this.loadTexture(def.texturePath);
    } else if (def.envColor) {
      const [r, g, b] = def.envColor.map(v => v / 255);
      toon.color.setRGB(r, g, b);
    }

    toon.transparent = def.alphaBlend;
    toon.side        = def.doubleSided ? THREE.DoubleSide : THREE.FrontSide;
    return toon;
  }

  /**
   * Build a 1D gradient texture with `bands` discrete steps.
   * Three.js MeshToonMaterial uses this as a lookup for diffuse lighting.
   */
  private buildGradientMap(bands: number): THREE.DataTexture {
    const n    = Math.max(2, Math.min(8, bands));
    const data = new Uint8Array(n);
    for (let i = 0; i < n; i++) {
      data[i] = Math.floor((i / (n - 1)) * 255);
    }
    const tex = new THREE.DataTexture(data, n, 1, THREE.RedFormat);
    tex.minFilter = THREE.NearestFilter;
    tex.magFilter = THREE.NearestFilter;
    tex.needsUpdate = true;
    return tex;
  }

  private loadTexture(path: string): THREE.Texture {
    if (this.texCache.has(path)) return this.texCache.get(path)!;
    const tex = new THREE.TextureLoader().load(path);
    // N64 textures are always power-of-2 and should not mip by default
    tex.generateMipmaps = false;
    tex.minFilter       = THREE.LinearFilter;
    tex.wrapS           = THREE.RepeatWrapping;
    tex.wrapT           = THREE.RepeatWrapping;
    this.texCache.set(path, tex);
    return tex;
  }

  /** Validate texture dimensions against N64 hardware limits. */
  validateTexture(width: number, height: number): { valid: boolean; reason?: string } {
    const valid = [32, 64, 128, 256];
    if (!valid.includes(width) || !valid.includes(height)) {
      return {
        valid:  false,
        reason: `Texture ${width}×${height} is not a valid N64 size. Use: ${valid.join(', ')}`,
      };
    }
    if (width === 256 || height === 256) {
      return {
        valid:  true,
        reason: '256px textures require Big-Tex render mode in Pyrite64.',
      };
    }
    return { valid: true };
  }

  dispose(): void {
    this.standardCache.forEach(m => m.dispose());
    this.cartoonCache.forEach(m => m.dispose());
    this.n64Cache.forEach(m => m.dispose());
    this.texCache.forEach(t => t.dispose());
  }
}
