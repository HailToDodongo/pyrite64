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

    struct InitData
    {
      int vpOffset[2];
      int vpSize[2];
      float fov;
      float near;
      float far;
      float aspectRatio;
      Mode mode;
    };

    P64::Camera camera{};
    Mode mode;

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