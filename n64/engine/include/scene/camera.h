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
    public:
      enum class Projection : uint8_t
      {
        PERSPECTIVE = 0,
        ORTHOGRAPHIC = 1,
      };

    private:
      T3DViewport viewports{};
      fm_mat4_t viewMatrix{};
      fm_vec3_t up{0,1,0};
      fm_vec3_t pos{};
      fm_vec3_t target{}; // computed

      uint8_t needsProjUpdate{false};
    public:
      float fov{}; // FOV in radians, only used in perspective mode
      float near{};
      float far{};
      float aspectRatio{};
      // Vertical half-size of the view volume in world units, only used in orthographic mode.
      // The horizontal half-size is derived from it via aspectRatio.
      float orthoSize{};
      Projection projection{Projection::PERSPECTIVE};

      Camera();
      CLASS_NO_COPY_MOVE(Camera);
      ~Camera();

      void update(float deltaTime);
      void attach();

      /**
       * Re-applies the scissor-area defined via the viewport.
       * This can be useful if you changed the scissor-area and now wish to reset it.
       */
      void reApplyScissor() {
        rdpq_set_scissor(
          viewports.offset[0],  viewports.offset[1],
          viewports.offset[0] + viewports.size[0],
          viewports.offset[1] + viewports.size[1]
        );
      }

      void setScreenArea(int x, int y, int width, int height);

      /**
       * Switches the camera over to a perspective projection.
       * @param newFov vertical fov in radians
       */
      void setPerspective(float newFov) {
        fov = newFov;
        projection = Projection::PERSPECTIVE;
      }

      /**
       * Switches the camera over to an orthographic projection.
       * @param newOrthoSize vertical half-size of the view volume in world units
       */
      void setOrthographic(float newOrthoSize) {
        orthoSize = newOrthoSize;
        projection = Projection::ORTHOGRAPHIC;
      }

      /**
       * Switches between perspective and orthographic projection,
       * keeping the settings (fov / ortho-size) of both.
       */
      void setProjection(Projection newProjection) {
        projection = newProjection;
      }

      [[nodiscard]] Projection getProjection() const { return projection; }

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

      const fm_mat4_t& getViewMatrix() const {
        return viewMatrix;
      }

      const fm_vec3_t& getUp() const {
        return up;
      }

      fm_vec3_t getScreenPos(const fm_vec3_t &worldPos);
  };
}
