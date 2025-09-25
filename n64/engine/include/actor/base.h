/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>
#include "lib/matrixManager.h"

namespace P64::Actor
{
  class Base
  {
    private:

    protected:

    public:
      fm_vec3_t pos{};
      fm_vec3_t scale{};
      fm_quat_t rot{};

      RingMat4FP matFP{};

      struct Args {
        uint16_t id;
        uint16_t flags;
      };

      explicit Base(const Args &args) {}
      virtual ~Base() = default;

      virtual void update(float deltaTime) = 0;
      virtual void draw(float deltaTime) {}
  };
}