/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>

namespace Build
{
  /**
   * Computes the glTF -> t3dm import scale ("base scale") for a model.
   * This is the factor that converts meter-space vertices into the quantized
   * int16 vertex units stored in the t3dm file.
   *
   * Picks the largest power of two that keeps all vertex positions within the
   * int16 range (with margin for animation), maximizing quantization precision.
   * Returns a fallback scale if the model has no usable bounds.
   */
  float computeAutoBaseScale(const std::string &gltfPath);
}
