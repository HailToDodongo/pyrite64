/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once

#include "assets/assetManager.h"
#include "scene/object.h"
#include "scene/sceneManager.h"
#include "scene/camera.h"

namespace P64::Comp
{
  struct Camera
  {
    static constexpr uint32_t ID = 3;

    enum class Mode : uint8_t
    {
      MANUAL = 0,
      OBJECT = 1,
    };

    using Projection = P64::Camera::Projection;

    struct InitData
    {
      int vpOffset[2];
      int vpSize[2];
      float fov;
      float near;
      float far;
      float aspectRatio;
      float orthoSize;
      Mode mode;
      Projection projection;
    };

    P64::Camera camera{};
    Mode mode;

    /**
     * Switches the camera over to a perspective projection.
     * @param fov vertical fov in radians
     */
    void setPerspective(float fov) { camera.setPerspective(fov); }

    /**
     * Switches the camera over to an orthographic projection.
     * @param orthoSize vertical half-size of the view volume in world units
     */
    void setOrthographic(float orthoSize) { camera.setOrthographic(orthoSize); }

    /**
     * Switches between perspective and orthographic projection,
     * keeping the settings (fov / ortho-size) of both.
     */
    void setProjection(Projection projection) { camera.setProjection(projection); }

    [[nodiscard]] Projection getProjection() const { return camera.getProjection(); }

    static uint32_t getAllocSize([[maybe_unused]] InitData* initData)
    {
      return sizeof(Camera);
    }

    static void initDelete([[maybe_unused]] Object& obj, Camera* data, InitData* initData);

    static void update([[maybe_unused]] Object& obj, [[maybe_unused]] Camera* data, [[maybe_unused]] float deltaTime);

    static void draw([[maybe_unused]] Object& obj, [[maybe_unused]] Camera* data, [[maybe_unused]] float deltaTime) {
    }
  };
}