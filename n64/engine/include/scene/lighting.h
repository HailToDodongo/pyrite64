/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <fgeom.h>
#include <graphics.h>

#include "renderer/renderScale.h"

namespace P64
{
  constexpr uint32_t MAX_LIGHTS = 6;

  struct Light
  {
    fm_vec3_t dirOrPos{};
    float strength{};
    color_t color{};
  };

  class Lighting
  {
    private:
      uint32_t lightCount{0};

      void addLight(const Light& l) {
        if(lightCount >= MAX_LIGHTS)return;
        lights[lightCount++] = l;
      }

    public:
      Light lights[MAX_LIGHTS]{};

      void reset() {
        lightCount = 0;
      }

      uint32_t getLightCount() const {
        return lightCount;
      }

      void apply() const;

      void addAmbientLight(const color_t col) {
        addLight({.strength = -1, .color = col});
      }

      void addDirLight(const color_t col, const fm_vec3_t& dir) {
        addLight({.dirOrPos = dir, .color = col});
      }

      /**
       * Adds a point light for the current frame.
       * @param pos position in meters
       * @param size roughly the maximum radius in meters the light will cover
       */
      void addPointLight(const color_t col, const fm_vec3_t& pos, float size) {
        // lights are applied in render units
        float renderScale = Renderer::getRenderScale();
        size = fmaxf(size * renderScale, 0.001f);
        addLight({.dirOrPos = pos * renderScale, .strength = size, .color = col});
      }
  };
}
