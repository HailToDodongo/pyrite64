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
    std::vector<T3DM::Model> models{};
    std::vector<T3DM::Bone> skeletons{};
    std::vector<T3DM::Anim> animations{};
    std::unordered_map<std::string, Material> materials{};
  };
}
