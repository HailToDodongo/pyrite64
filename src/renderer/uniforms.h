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
    glm::vec2 screenSize{};
  };

  struct UniformN64Material
  {
    glm::i32vec4 blender[2];

    //Tile settings: xy = TEX0, zw = TEX1
    glm::vec4 mask; // clamped if < 0, mask = abs(mask)
    glm::vec4 shift;
    glm::vec4 low;
    glm::vec4 high; // if negative, mirrored, high = abs(high)

    // color-combiner
    glm::i32vec4 cc0Color;
    glm::i32vec4 cc0Alpha;
    glm::i32vec4 cc1Color;
    glm::i32vec4 cc1Alpha;

    // "vec4 modes" in shader:
    uint32_t vertexFX;
    uint32_t otherModeL;
    uint32_t otherModeH;
    uint32_t flags;

    glm::vec4 lightColor[2];
    glm::vec4 lightDir[2]; // [0].w is alpha clip
    glm::vec4 colPrim;
    glm::vec4 colEnv;
    glm::vec4 ambientColor;
    glm::vec4 ck_center;
    glm::vec4 ck_scale;
    glm::vec4 primLodDepth;
    glm::vec4 k_0123;
    glm::vec2 k_45;
  };

  struct UniformsObject
  {
    glm::mat4 modelMat{};
    UniformN64Material mat{};
    uint32_t objectID{};
  };
}
