/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include "material.h"
#include <vector>
#include "tiny3d/tools/gltf_importer/src/structs.h"

namespace Project::Assets
{
  struct Model3D
  {
    T3DM::T3DMData t3dm{};
    std::unordered_map<std::string, Material> materials{};
    // meter -> quantized int16 vertex-unit factor the model was parsed with.
    // Auto-computed from the model bounds; must be used consistently for
    // parsing, writing and skeleton/AABB conversions of this parse result.
    float autoBaseScale{64.0f};
  };
}
