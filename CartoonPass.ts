/**
 * CartoonPass.ts
 * Post-processing pass that applies cartoon / cel-shade aesthetics.
 *
 * Composited in EffectComposer after the main RenderPass.
 * Mimics what the N64 C cartoon module does via RDP tricks:
 *  - Discrete shade banding
 *  - Hard inked outline edges
 *
 * Uses Three.js ShaderPass with a custom GLSL shader.
 */

import * as THREE from 'three';
import { Pass, FullScreenQuad } from 'three/examples/jsm/postprocessing/Pass';

// ─── Cartoon shader GLSL ──────────────────────────────────────────────────────

const CartoonShader = {
  uniforms: {
    tDiffuse:   { value: null as THREE.Texture | null },
    tDepth:     { value: null as THREE.Texture | null },
    bands:      { value: 4.0 },
    outlineStr: { value: 0.8 },
    resolution: { value: new THREE.Vector2(1, 1) },
  },

  vertexShader: /* glsl */ `
    varying vec2 vUv;
    void main() {
      vUv = uv;
      gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
    }
  `,

  fragmentShader: /* glsl */ `
    uniform sampler2D tDiffuse;
    uniform sampler2D tDepth;
    uniform float     bands;
    uniform float     outlineStr;
    uniform vec2      resolution;
    varying vec2      vUv;

    // Detect edges by sampling neighboring depth values (Sobel-ish)
    float edgeDetect() {
      vec2 texel = 1.0 / resolution;
      float d0 = texture2D(tDepth, vUv).r;
      float d1 = texture2D(tDepth, vUv + vec2( texel.x, 0.0)).r;
      float d2 = texture2D(tDepth, vUv + vec2(-texel.x, 0.0)).r;
      float d3 = texture2D(tDepth, vUv + vec2(0.0,  texel.y)).r;
      float d4 = texture2D(tDepth, vUv + vec2(0.0, -texel.y)).r;
      float edge = abs(d0 - d1) + abs(d0 - d2) + abs(d0 - d3) + abs(d0 - d4);
      return clamp(edge * 80.0, 0.0, 1.0);
    }

    // Quantize brightness to discrete bands
    vec3 celShade(vec3 color) {
      float luma   = dot(color, vec3(0.299, 0.587, 0.114));
      float banded = floor(luma * bands) / bands;
      float ratio  = banded / max(luma, 0.001);
      return color * ratio;
    }

    void main() {
      vec4  src   = texture2D(tDiffuse, vUv);
      vec3  shaded = celShade(src.rgb);
      float edge  = edgeDetect();

      // Blend in black ink outline
      vec3  inked = mix(shaded, vec3(0.0), edge * outlineStr);
      gl_FragColor = vec4(inked, src.a);
    }
  `,
};

// ─── CartoonPass ─────────────────────────────────────────────────────────────

export class CartoonPass extends Pass {
  private material: THREE.ShaderMaterial;
  private fsQuad:   FullScreenQuad;

  // Expose to let the viewport read/write these
  bands      = 4;
  outlineStr = 0.8;

  constructor() {
    super();

    this.material = new THREE.ShaderMaterial({
      uniforms:       THREE.UniformsUtils.clone(CartoonShader.uniforms),
      vertexShader:   CartoonShader.vertexShader,
      fragmentShader: CartoonShader.fragmentShader,
    });

    this.fsQuad = new FullScreenQuad(this.material);
  }

  setSize(width: number, height: number): void {
    this.material.uniforms['resolution'].value.set(width, height);
  }

  render(
    renderer: THREE.WebGLRenderer,
    writeBuffer: THREE.WebGLRenderTarget,
    readBuffer: THREE.WebGLRenderTarget,
  ): void {
    this.material.uniforms['tDiffuse'].value   = readBuffer.texture;
    this.material.uniforms['bands'].value      = this.bands;
    this.material.uniforms['outlineStr'].value = this.outlineStr;

    if (this.renderToScreen) {
      renderer.setRenderTarget(null);
    } else {
      renderer.setRenderTarget(writeBuffer);
      if (this.clear) renderer.clear();
    }

    this.fsQuad.render(renderer);
  }

  dispose(): void {
    this.material.dispose();
    this.fsQuad.dispose();
  }
}
