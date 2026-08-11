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
  class Object;
  namespace Comp { struct Surface; }

  class Camera
  {
    public:
      enum class Projection : uint8_t
      {
        PERSPECTIVE = 0,
        ORTHOGRAPHIC = 1,
      };

      enum class TargetType : uint8_t
      {
        SCREEN = 0,
        SURFACE = 1,
      };

    private:
      T3DViewport viewports{};
      fm_mat4_t viewMatrix{};
      fm_vec3_t up{0,1,0};
      fm_vec3_t pos{};
      fm_vec3_t target{}; // computed

      int16_t screenArea[4]{}; // last area set via 'setScreenArea', restored when switching back to screen

      surface_t depthView{};    // sized view into a borrowed depth buffer for surface targets
      surface_t* currTgtSurf{}; // target resolved by 'attach', valid until 'detach'
      surface_t* currTgtDepth{};

      uint8_t needsProjUpdate{false};

      /**
       * Prepares rendering into the given surface by fitting the viewport to it.
       * @param surf target surface
       */
      void adjustToSurface(const surface_t &surf);
    public:
      float fov{}; // FOV in radians, only used in perspective mode
      float near{};
      float far{};
      float aspectRatio{};
      // Vertical half-size of the view volume in world units, only used in orthographic mode.
      // The horizontal half-size is derived from it via aspectRatio.
      float orthoSize{};
      Projection projection{Projection::PERSPECTIVE};
      // Visibility read-mask: only objects whose 'visMask' shares a bit with this are drawn.
      uint8_t visMask{0xFF};
      TargetType targetType{TargetType::SCREEN};
      uint16_t targetObjId{0}; // object owning the target Comp::Surface, 0 = none
      surface_t* targetSurfPtr{}; // raw target surface, takes precedence over 'targetObjId'
      surface_t* targetDepthPtr{}; // optional depth buffer for 'targetSurfPtr'

      Camera();
      CLASS_NO_COPY_MOVE(Camera);
      ~Camera();

      void update(float deltaTime);

      /**
       * Attaches the camera for rendering, applying its viewport and scissor.
       * For surface targets this also attaches the surface and a depth buffer.
       * @return false if the camera renders nothing this frame (no or deleted surface target)
       */
      bool attach();

      /**
       * Counterpart to 'attach', detaches a surface target again.
       * NOP for cameras rendering to the screen.
       */
      void detach();

      /// true if 'attach' redirected rendering into a surface, until 'detach' is called
      [[nodiscard]] bool isSurfaceAttached() const { return currTgtSurf != nullptr; }

      /**
       * Records commands to redirect rendering into the camera's attached surface target.
       * Only needed for command streams outside of 'attach'/'detach', e.g. draw-layers.
       * NOP if no surface target is attached.
       */
      void applyTargetImages();

      /**
       * Counterpart to 'applyTargetImages', records commands to restore
       * the render pipeline's framebuffer and depth buffer.
       * NOP if no surface target is attached.
       */
      void restoreTargetImages();

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
       * Renders to the screen (framebuffer) again, undoing 'setTargetSurface'.
       * This also restores the viewport to the last area set via 'setScreenArea'.
       */
      void setTargetScreen();

      /**
       * Renders into the first Surface component of the given object instead of the screen.
       * If the object is nullptr or has no Surface component, the camera renders nothing.
       * The surface must be in a format the RDP can render to (RGBA16, RGBA32 or I8/CI8).
       * @param obj object owning a Surface component, or nullptr to render nothing
       */
      void setTargetSurface(Object* obj);

      /**
       * Renders into the given raw surface instead of the screen.
       * If nullptr is passed, the camera renders nothing.
       * The surface must be in a format the RDP can render to (RGBA16, RGBA32 or I8/CI8).
       * Note: the caller must keep the surfaces alive while they are targeted,
       * for automatic lifetime handling target an object with a Surface component instead.
       * @param surf surface to render into, or nullptr to render nothing
       * @param depth optional depth buffer, must be at least the size of 'surf'.
       *              If nullptr, the main depth buffer is re-used.
       */
      void setTargetSurface(surface_t* surf, surface_t* depth = nullptr) {
        if(surf && depth) {
          assertf(depth->width >= surf->width && depth->height >= surf->height,
            "Depth buffer (%dx%d) must be at least the size of the color buffer (%dx%d)",
            depth->width, depth->height, surf->width, surf->height);
        }
        targetType = TargetType::SURFACE;
        targetObjId = 0;
        targetSurfPtr = surf;
        targetDepthPtr = surf ? depth : nullptr;
      }

      /// Switches to surface mode without a target, making the camera render nothing.
      void setTargetSurface(std::nullptr_t) {
        targetType = TargetType::SURFACE;
        targetObjId = 0;
        targetSurfPtr = nullptr;
        targetDepthPtr = nullptr;
      }

      [[nodiscard]] bool hasSurfaceTarget() const { return targetType == TargetType::SURFACE; }

      /**
       * Resolves the current target Surface component.
       * @return component, or nullptr if none is set or its owner was deleted
       */
      [[nodiscard]] Comp::Surface* resolveTargetSurface() const;

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
