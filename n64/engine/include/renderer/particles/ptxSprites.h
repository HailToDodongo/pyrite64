/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once
#include <t3d/t3d.h>
#include <t3d/tpx.h>
#include "renderer/particles/ptxSystem.h"

namespace P64::PTX
{
  class Sprites
  {
    public:
      struct Conf {
        uint32_t maxSize{};
        uint8_t isRotating{false};
        uint8_t noRng{false};
      };

      System system;
      float simTimer = 0.0f;

    private:
      sprite_t *sprite{};
      rspq_block_t *setupDPL{};
      float animTimer = 0.0f;
      uint16_t mirrorPt = 32;
      color_t color{};
      Conf conf{};

    public:
      explicit Sprites(const char* spritePath, const Conf &conf);
      ~Sprites();

      void setColor(color_t newColor) { this->color = newColor; }
      [[nodiscard]] color_t getColor() const { return color; }

      void add(const fm_vec3_t &pos, uint32_t seed, color_t col, float scale = 1.0f);
      void add(const fm_vec3_t &pos, uint32_t seed, float scale = 1.0f) {
        add(pos, seed, color, scale);
      }

      void draw(float deltaTime);
      void clear();
  };
}