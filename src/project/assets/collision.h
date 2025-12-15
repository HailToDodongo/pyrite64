/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <vector>
#include "glm/vec3.hpp"

namespace Project::Assets::Collision
{
  std::vector<int16_t> createBVH(
    const std::vector<glm::i16vec3> &vertices,
    const std::vector<uint16_t> &indices
  );
}
