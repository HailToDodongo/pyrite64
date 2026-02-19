/**
 * outline.h
 * Pyrite64 — N64 Cartoon Outline Renderer
 *
 * @copyright 2025 - Max Bebök
 * @license MIT
 *
 *
 * Technique:
 *   Uses a two-pass approach for silhouette/contour outlines on N64:
 *
 *   Pass 1 (back-face hull):
 *     - Flip the front-face culling to render back faces only
 *     - Apply a vertex expansion along normals (in model space)
 *     - Render with a flat black (or tinted) color
 *     - This creates a slightly-larger dark silhouette behind the mesh
 *
 *   Pass 2 (normal cel-shaded or standard draw):
 *     - Restore front-face culling
 *     - Draw the mesh normally (optionally with cel_shader)
 *     - The front faces cover most of the back-face hull, leaving only
 *       an outline at silhouette edges
 *
 *   This back-face hull technique is the standard approach for
 *   N64-class hardware that lacks post-processing edge detection.
 *   The vertex expansion is done at load time (baked into a second
 *   vertex buffer) to avoid per-frame computation.
 *
 * Integration:
 *   The outline component is attached per-object and can be toggled
 *   with a scene flag. Thickness and color are configurable.
 */

#pragma once

#include <stdint.h>
#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── Configuration ────────────────────────────────────────────────────────────

/** Per-object outline settings */
typedef struct {
  /** Outline color (typically black or very dark) */
  color_t color;
  
  /** Outline thickness in model-space units.
   *  Typical range: 0.5 – 3.0 for character models.
   *  Larger values for distant or small objects, smaller for close-ups. */
  float thickness;
  
  /** Enable/disable this object's outline */
  bool enabled;
  
  /** Outline mode: 0 = silhouette (hull back-face), 1 = contour */
  uint8_t mode;
  
  /** The pre-expanded vertex buffer for the hull pass.
   *  Built by outline_bake_hull() at load time. */
  T3DModel *hullModel;
} OutlineConf;

// ─── API ──────────────────────────────────────────────────────────────────────

/**
 * Bake an outline hull model from a source model.
 * Expands every vertex along its normal by `thickness`.
 * This is called once at asset load time, NOT per frame.
 *
 * @param src       Source T3DModel
 * @param thickness Expansion distance in model units
 * @return          A new T3DModel with expanded vertices (caller owns; freed via outline_free_hull)
 *
 * Note: The hull model shares the same skeleton/bone data as the source,
 * so animated models get correct silhouettes after skinning.
 */
T3DModel* outline_bake_hull(const T3DModel *src, float thickness);

/**
 * Free a hull model created by outline_bake_hull().
 */
void outline_free_hull(T3DModel *hull);

/**
 * Draw the outline hull (back-face pass).
 * Call this BEFORE drawing the normal mesh.
 *
 * Sets up:
 *  - Back-face culling (reversed winding)
 *  - Flat-color combiner with the outline color
 *  - Z-buffer write enabled, Z compare enabled
 *
 * @param conf    Outline configuration
 * @param modelMat  4x4 model matrix for the object
 */
void outline_draw_hull(const OutlineConf *conf, const T3DMat4FP *modelMat);

/**
 * End the outline hull pass.
 * Restores front-face culling and default combiner.
 * After this call, draw the normal mesh (with or without cel_shader).
 */
void outline_end_hull(void);

/**
 * Convenience: draw an outlined + cel-shaded object in the correct order.
 *   1. outline_draw_hull (back faces, expanded)
 *   2. outline_end_hull
 *   3. cel_shader_begin + normal draw + cel_shader_end
 *
 * @param conf       Outline config
 * @param celBands   Number of cel shade bands (2-8)
 * @param celColor   Cel shader tint color
 * @param modelMat   Object model matrix
 * @param drawFunc   Callback to draw the actual mesh (called in step 3)
 * @param drawCtx    User context pointer passed to drawFunc
 */
void outline_draw_cel(
  const OutlineConf *conf,
  uint8_t celBands,
  color_t celColor,
  const T3DMat4FP *modelMat,
  void (*drawFunc)(void *ctx),
  void *ctx
);

#ifdef __cplusplus
}
#endif
