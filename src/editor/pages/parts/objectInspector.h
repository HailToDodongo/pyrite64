/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "glm/ext/vector_float3.hpp"

namespace Editor
{
  class ObjectInspector
  {
    private:
      glm::vec3 euler;
      uint32_t lastSelected;

    public:
      ObjectInspector();

      void draw();
  };
}