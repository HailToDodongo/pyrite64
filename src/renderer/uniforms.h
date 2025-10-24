/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "glm/mat4x4.hpp"

namespace Renderer
{
  struct UniformGlobal
  {
    glm::mat4 projMat{};
    glm::mat4 cameraMat{};
  };

  struct UniformN64Material
  {
    glm::u8vec4 cc0Color{};
    glm::u8vec4 cc0Alpha{};
    glm::u8vec4 cc1Color{};
    glm::u8vec4 cc1Alpha{};
    glm::u8vec4 colPrim{};
    glm::u8vec4 colEnv{};
  };

  struct UniformsObject
  {
    glm::mat4 modelMat{};
    UniformN64Material mat{};
    uint32_t objectID{};
  };
}
