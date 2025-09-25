/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once

#include <t3d/t3d.h>
#include <t3d/t3dmath.h>

namespace P64
{
  struct Camera
  {
    T3DViewport viewports[3]{};
    T3DVec3 pos{};
    T3DVec3 target{};
    float fov{};
    float near{};
    float far{};

    uint8_t vpIdx{0};
    uint8_t needsProjUpdate{false};

    void update(float deltaTime);
    void attach();

    void setPos(T3DVec3 newPos) {
      pos = newPos;
    }

    void setTarget(T3DVec3 newPos) {
      target = newPos;
    }

    void move(T3DVec3 dir) {
      target += dir;
      pos += dir;
    }

    [[nodiscard]] const T3DVec3 &getTarget() const { return target; }
    [[nodiscard]] const T3DVec3 &getPos() const { return pos; }
  };
}