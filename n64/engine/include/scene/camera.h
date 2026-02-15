/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once

#include <t3d/t3d.h>
#include <t3d/t3dmath.h>

#include "lib/types.h"

namespace P64
{
  class Camera
  {
    private:
      T3DViewport viewports{};
      fm_mat4_t viewMatrix{};
      fm_vec3_t up{0,1,0};
      fm_vec3_t pos{};
      fm_vec3_t target{}; // computed

      uint8_t needsProjUpdate{false};
    public:
      float fov{};
      float near{};
      float far{};
      float aspectRatio{};

      Camera();
      CLASS_NO_COPY_MOVE(Camera);
      ~Camera();

      void update(float deltaTime);
      void attach();

      void setScreenArea(int x, int y, int width, int height);

      /**
       * Sets new camera values based on a look-at transform.
       * If you have an arbitrary rotation based camera prefer using 'setPosRot'.
       * @param newPos camera eye
       * @param newTarget camera target
       * @param newUp camera up vector (+Y by default)
       */
      void setLookAt(const fm_vec3_t &newPos, const fm_vec3_t &newTarget, const fm_vec3_t &newUp = {0,1,0});

      /**
       * Sets a new camera by position and rotation.
       * If you have a look-at based camera prefer using 'setLookAt'.
       * @param pos camera eye
       * @param rot rotation
       */
      void setPosRot(const fm_vec3_t &newPos, const fm_quat_t &rot);

      [[nodiscard]] const fm_vec3_t &getTarget() const { return target; }
      [[nodiscard]] const fm_vec3_t &getPos() const { return pos; }

      [[nodiscard]] fm_vec3_t getViewDir() const {
        fm_vec3_t dir{};
        fm_vec3_sub(&dir, &target, &getPos());
        fm_vec3_norm(&dir, &dir);
        return dir;
      }

      fm_vec3_t getScreenPos(const fm_vec3_t &worldPos);
  };
}
