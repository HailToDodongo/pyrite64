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
  };
}
