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
    fm_vec3_t pos{};
    fm_vec3_t target{};
    float fov{};
    float near{};
    float far{};

    uint8_t vpIdx{0};
    uint8_t needsProjUpdate{false};

    void update(float deltaTime);
    void attach();

    void setPos(fm_vec3_t newPos) {
      pos = newPos;
    }

    void setTarget(fm_vec3_t newPos) {
      target = newPos;
    }

    void move(fm_vec3_t dir) {
      target += dir;
      pos += dir;
    }

    [[nodiscard]] const fm_vec3_t &getTarget() const { return target; }
    [[nodiscard]] const fm_vec3_t &getPos() const { return pos; }

    fm_vec3_t getScreenPos(const fm_vec3_t &worldPos);
  };
}