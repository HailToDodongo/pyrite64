/**
 * outline.c
 * Pyrite64 — N64 Cartoon Outline Renderer (Implementation)
 *
 * @copyright 2025 - Max Bebök
 * @license MIT
 *
 * See outline.h for technique description.
 */

#include "renderer/outline.h"
#include "renderer/cel_shader.h"
#include "lib/memory.h"
#include <malloc.h>
#include <string.h>
#include <math.h>

// ─── Hull baking (asset-load-time) ───────────────────────────────────────────

T3DModel* outline_bake_hull(const T3DModel *src, float thickness) {
  assertf(src != NULL, "outline_bake_hull: src model is NULL");
  assertf(thickness > 0.0f, "outline_bake_hull: thickness must be > 0");

  // Deep-copy the model.  We only modify vertex positions.
  // In a production engine you'd have a dedicated "hull vertex buffer"
  // format that omits UVs and normals to save RDRAM.  For now we clone
  // the full model so it can be drawn with the same T3D calls.

  uint32_t modelSize = t3d_model_get_size(src);
  T3DModel *hull = (T3DModel *)memalign(16, modelSize);
  assertf(hull != NULL, "outline_bake_hull: out of memory for hull (%u bytes)", modelSize);
  memcpy(hull, src, modelSize);

  // Walk the vertex data and expand each position along its normal.
  // T3DModel stores vertices in object parts → mesh parts → vertex arrays.
  // We iterate all object parts and their mesh sub-parts.
  for (uint32_t oi = 0; oi < hull->numObjects; oi++) {
    T3DObject *obj = &hull->objects[oi];
    for (uint32_t pi = 0; pi < obj->numParts; pi++) {
      T3DObjectPart *part = &obj->parts[pi];

      // Each part has a vertex buffer.  Access the packed 16-bit positions
      // and 8-bit normals from the T3D vertex format.
      uint32_t vertCount = part->numVertices;
      if (vertCount == 0) continue;

      T3DVertPacked *verts = part->vert;
      if (!verts) continue;

      // T3DVertPacked stores pairs of vertices.
      // Each pair: pos (3 × int16), normal (3 × int8), ...
      // We expand pos += normal * thickness.
      // Since positions are 16-bit fixed-point and normals are 8-bit
      // signed, we convert thickness to the same scale.

      uint32_t pairCount = (vertCount + 1) / 2;
      for (uint32_t v = 0; v < pairCount; v++) {
        T3DVertPacked *vp = &verts[v];

        // Vertex A
        {
          float nx = (float)vp->normA[0] / 127.0f;
          float ny = (float)vp->normA[1] / 127.0f;
          float nz = (float)vp->normA[2] / 127.0f;
          float len = sqrtf(nx*nx + ny*ny + nz*nz);
          if (len > 0.001f) {
            float inv = thickness / len;
            vp->posA[0] += (int16_t)(nx * inv);
            vp->posA[1] += (int16_t)(ny * inv);
            vp->posA[2] += (int16_t)(nz * inv);
          }
        }

        // Vertex B (only if this pair has a second vertex)
        if (v * 2 + 1 < vertCount) {
          float nx = (float)vp->normB[0] / 127.0f;
          float ny = (float)vp->normB[1] / 127.0f;
          float nz = (float)vp->normB[2] / 127.0f;
          float len = sqrtf(nx*nx + ny*ny + nz*nz);
          if (len > 0.001f) {
            float inv = thickness / len;
            vp->posB[0] += (int16_t)(nx * inv);
            vp->posB[1] += (int16_t)(ny * inv);
            vp->posB[2] += (int16_t)(nz * inv);
          }
        }
      }

      // Flush data cache so RSP reads the updated vertices
      data_cache_hit_writeback(verts, pairCount * sizeof(T3DVertPacked));
    }
  }

  return hull;
}

void outline_free_hull(T3DModel *hull) {
  if (hull) {
    free(hull);
  }
}

// ─── Runtime drawing ─────────────────────────────────────────────────────────

void outline_draw_hull(const OutlineConf *conf, const T3DMat4FP *modelMat) {
  if (!conf || !conf->enabled || !conf->hullModel) return;

  // Set up back-face rendering:
  //  - Reverse winding so we draw back faces
  //  - Flat black (or custom outline color)
  //  - Z-buffer on, so the hull is properly occluded

  rdpq_mode_begin();
    rdpq_set_mode_standard();
    rdpq_mode_zbuf(true, true);
    rdpq_mode_persp(true);
    rdpq_mode_antialias(AA_NONE);

    // Flat color combiner: output = PRIM color (the outline color)
    rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
    rdpq_set_prim_color(conf->color);

    // Reverse culling direction to show back faces
    rdpq_mode_blender(0);
  rdpq_mode_end();

  // Draw the hull model with reversed face winding.
  // T3D uses FRONT face culling by default; we switch to BACK.
  t3d_state_set_drawflags(T3D_FLAG_CULL_FRONT);

  // Apply model transform and draw
  t3d_matrix_push(modelMat);
  t3d_model_draw(conf->hullModel);
  t3d_matrix_pop(1);
}

void outline_end_hull(void) {
  // Restore normal (back-face) culling
  t3d_state_set_drawflags(T3D_FLAG_CULL_BACK);

  // Restore standard rendering mode; caller is responsible for setting its combiner
  rdpq_mode_begin();
    rdpq_set_mode_standard();
    rdpq_mode_zbuf(true, true);
    rdpq_mode_persp(true);
    rdpq_mode_antialias(AA_NONE);
    rdpq_mode_filter(FILTER_BILINEAR);
    rdpq_mode_blender(0);
  rdpq_mode_end();
}

void outline_draw_cel(
  const OutlineConf *conf,
  uint8_t celBands,
  color_t celColor,
  const T3DMat4FP *modelMat,
  void (*drawFunc)(void *ctx),
  void *ctx
) {
  // Step 1: Draw the outline hull (back faces, expanded)
  outline_draw_hull(conf, modelMat);
  outline_end_hull();

  // Step 2: Draw the normal mesh with cel shading
  cel_shader_begin(celBands, celColor);
  t3d_matrix_push(modelMat);
  drawFunc(ctx);
  t3d_matrix_pop(1);
  cel_shader_end();
}
