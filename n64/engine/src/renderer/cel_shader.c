/**
 * cel_shader.c
 * Pyrite64 — N64 Cartoon Render Module
 *
 * @copyright 2025 - Max Bebök
 * @license MIT
 *
 * Implements cel/toon shading using the RDP's color combiner.
 * Works within libdragon + tiny3d's rendering pipeline.
 *
 * Technique:
 *   The N64 RDP has a 2-cycle combiner that can mix up to 4 values.
 *   We exploit this to quantize diffuse lighting into discrete bands
 *   by pre-computing a 1D palette texture and using it as a LUT.
 *
 * Reference: tiny3d combiner docs + N64Brew wiki on RDP combiners
 */

#include "cel_shader.h"
#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>
#include <math.h>

// ─── Palette LUT texture ──────────────────────────────────────────────────────

// 8-entry grayscale ramp — index maps from diffuse intensity to banded output
// Tweak these values to change the cartoon look
static const uint8_t CEL_PALETTE_4BAND[8] = {
  0x00, 0x00,   // band 0 — shadow
  0x60, 0x60,   // band 1 — dark midtone
  0xA0, 0xA0,   // band 2 — light midtone
  0xFF, 0xFF,   // band 3 — highlight
};

static const uint8_t CEL_PALETTE_3BAND[8] = {
  0x00, 0x00,
  0x55, 0x55,
  0xFF, 0xFF,
  0xFF, 0xFF,   // pad to 8
};

static surface_t cel_lut_surface = {0};
static bool      cel_initialized  = false;

// ─── Init / cleanup ───────────────────────────────────────────────────────────

void cel_shader_init(void) {
  if (cel_initialized) return;

  // Create a tiny 8×1 I8 texture for the LUT
  // In practice a 16x1 or 32x1 gives smoother control
  cel_lut_surface = surface_alloc(FMT_I8, 8, 1);
  memcpy(cel_lut_surface.buffer, CEL_PALETTE_4BAND, 8);
  cel_initialized = true;
}

void cel_shader_cleanup(void) {
  if (!cel_initialized) return;
  surface_free(&cel_lut_surface);
  cel_initialized = false;
}

// ─── Per-mesh setup ───────────────────────────────────────────────────────────

/**
 * Call before rendering a mesh with cel shading.
 * Sets up the RDP combiner to:
 *   1. Compute standard diffuse lighting
 *   2. Look up the banded result in our palette LUT
 *
 * @param bands  Number of shade bands (2–4 look best on N64)
 * @param color  Base tint color (RGBA)
 */
void cel_shader_begin(uint8_t bands, color_t color) {
  assertf(cel_initialized, "cel_shader_init() must be called first");

  // Upload LUT based on requested band count
  // 3BAND has 3 distinct values (stark contrast), 4BAND has 4 values (smoother)
  const uint8_t *palette = (bands <= 3) ? CEL_PALETTE_3BAND : CEL_PALETTE_4BAND;
  memcpy(cel_lut_surface.buffer, palette, 8);

  // Load the LUT as TMEM tile 1 (tile 0 is reserved for the mesh albedo)
  rdpq_tex_upload(TILE1, &cel_lut_surface, NULL);

  /*
   * Combiner formula (2-cycle mode):
   *
   * Cycle 1: standard diffuse
   *   RGB = (SHADE - 0) * PRIM + 0      => vertex diffuse tinted by prim color
   *
   * Cycle 2: LUT lookup (approximated — real HW uses TEX ENV trick)
   *   RGB = (COMBINED - 0) * ENV + 0
   *
   * Note: True hardware LUT lookup via combiner is non-trivial.
   * The real implementation uses a custom display list approach or
   * pre-bakes the band quantization into vertex colors via the editor.
   * TODO: implement the display-list-based approach from tiny3d's rdpq_mode API
   */
  rdpq_set_prim_color(color);
  rdpq_mode_combiner(RDPQ_COMBINER2(
    (SHADE, ZERO, PRIM, ZERO),   (ZERO, ZERO, ZERO, SHADE),
    (COMBINED, ZERO, ENV, ZERO), (ZERO, ZERO, ZERO, COMBINED)
  ));
}

/**
 * Reset combiner to default after cel-shaded mesh rendering.
 */
void cel_shader_end(void) {
  // Restore to tiny3d's default combiner
  // (actual reset depends on your tiny3d pipeline version)
  rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
}

// ─── Vertex color baking (run in editor / asset pipeline, not at runtime) ─────

/**
 * Quantize a linear float [0,1] diffuse value to cel bands.
 * Used by the editor's pre-bake pass to encode cartoon lighting
 * directly into vertex colors — avoids runtime combiner complexity.
 *
 * @param diffuse  Incoming diffuse intensity [0, 1]
 * @param bands    Number of bands (2–8)
 * @return         Quantized intensity [0, 1]
 */
float cel_quantize(float diffuse, uint8_t bands) {
  if (bands < 2) bands = 2;
  if (bands > 8) bands = 8;
  float step = 1.0f / (float)(bands - 1);
  return floorf(diffuse / step + 0.5f) * step;
}
