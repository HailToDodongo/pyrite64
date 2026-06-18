/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <t3d/t3dmodel.h>

#include "assets/assetManager.h"
#include "lib/matrixManager.h"
#include "scene/object.h"
#include "scene/sceneManager.h"
#include "script/scriptTable.h"

namespace P64::Comp
{
  struct Light
  {
    static constexpr uint32_t ID = 2;

    struct InitData
    {
      color_t color;
      float size;
      uint8_t index;
      uint8_t type;
      int8_t dir[3];
    };

    fm_vec3_t dir{};
    float size{};
    color_t color{};
    uint8_t type{};
    uint8_t index{};

    static uint32_t getAllocSize([[maybe_unused]] InitData* initData)
    {
      return sizeof(Light);
    }

    static void initDelete([[maybe_unused]] Object& obj, Light* data, InitData* initData)
    {
      if (initData == nullptr) {
        data->~Light();
        return;
      }

      new(data) Light();
      data->color = initData->color;
      data->size = initData->size;
      data->type = initData->type;
      data->index = initData->index;
      data->dir = {
        (float)initData->dir[0] * (1.0f / 127.0f),
        (float)initData->dir[1] * (1.0f / 127.0f),
        (float)initData->dir[2] * (1.0f / 127.0f)
      };
    }

    static void update([[maybe_unused]] Object& obj, Light* data, float deltaTime) {
      auto &light = SceneManager::getCurrent().getLighting();
      switch(data->type)
      {
        case 0: light.addAmbientLight(data->color); break;
        case 1: light.addDirLight(data->color, data->dir); break;
        case 2: light.addPointLight(data->color, obj.pos, data->size); break;
        default: break;
      }
    }

    static void draw([[maybe_unused]] Object& obj, [[maybe_unused]] Light* data, float deltaTime) {

    }
  };
}