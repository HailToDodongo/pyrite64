/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once

#include "assets/assetManager.h"
#include "scene/object.h"
#include "scene/sceneManager.h"

namespace P64::Comp
{
  struct Camera
  {
    struct InitData
    {
      int vpOffset[2];
      int vpSize[2];
      float fov;
      float near;
      float far;
    };

    P64::Camera camera{};

    static uint32_t getAllocSize([[maybe_unused]] InitData* initData)
    {
      return sizeof(Camera);
    }

    static void initDelete([[maybe_unused]] Object& obj, Camera* data, InitData* initData)
    {
      if (initData == nullptr) {
        SceneManager::getCurrent().removeCamera(&data->camera);
        data->~Camera();
        return;
      }

      new(data) Camera();

      SceneManager::getCurrent().addCamera(&data->camera);
      auto &cam = data->camera;
      cam.setPos(obj.pos);
      cam.setTarget(obj.pos + fm_vec3_t{{0, 0, -100}});
      cam.fov  = initData->fov;
      cam.near = initData->near;
      cam.far  = initData->far;
      for(auto &vp : cam.viewports) {
        vp = t3d_viewport_create();
        t3d_viewport_set_area(vp,
          initData->vpOffset[0], initData->vpOffset[1],
          initData->vpSize[0], initData->vpSize[1]
        );
      }
    }

    static void update([[maybe_unused]] Object& obj, [[maybe_unused]] Camera* data, [[maybe_unused]] float deltaTime) {
    }

    static void draw([[maybe_unused]] Object& obj, [[maybe_unused]] Camera* data, [[maybe_unused]] float deltaTime) {
    }
  };
}