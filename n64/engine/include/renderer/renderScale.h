/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <t3d/t3dmath.h>

namespace P64::Renderer
{
  constexpr float DEFAULT_RENDER_SCALE = 100.0f;

  inline constinit float renderScale_{DEFAULT_RENDER_SCALE};
  inline constinit float invRenderScale_{1.0f / DEFAULT_RENDER_SCALE};

  /**
   * Render-scale: how many RSP fixed-point world units one meter maps to.
   * The engine simulates in meters; only at render entry points (model/view
   * matrices, lights, culling, debug draw, particles) are positions multiplied
   * by this factor in float space before the s16.16 fixed-point conversion.
   */
  inline float getRenderScale() { return renderScale_; }
  inline float getInvRenderScale() { return invRenderScale_; }

  inline void setRenderScale(float renderScale) {
    renderScale_ = renderScale;
    invRenderScale_ = 1.0f / renderScale;
  }

  /**
   * Builds a fixed-point model matrix from a meter-space SRT.
   * Applies the render-scale and the model's vertex-scale, so callers
   * (components and user scripts alike) never deal with any scaling.
   *
   * @param mat output matrix
   * @param scale object scale (unitless multiplier of the model's authored size)
   * @param rot rotation
   * @param pos position in meters
   * @param vertexScale the model's vertex scale (see AssetManager::getVertexScale)
   */
  inline void fillModelMatrixFP(T3DMat4FP *mat,
    const fm_vec3_t &scale, const fm_quat_t &rot, const fm_vec3_t &pos,
    float vertexScale)
  {
    t3d_mat4fp_from_srt(mat,
      scale * (vertexScale * renderScale_), rot, pos * renderScale_);
  }
}
