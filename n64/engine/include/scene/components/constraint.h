/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "scene/object.h"
#include "scene/sceneManager.h"

namespace P64::Comp
{
  struct Constraint
  {
    static constexpr uint32_t ID = 7;

    static constexpr uint8_t TYPE_COPY_OBJ = 0;
    static constexpr uint8_t TYPE_REL_OFFSET = 1;
    static constexpr uint8_t TYPE_COPY_CAM = 2;
    static constexpr uint8_t TYPE_BILLBOARD_Y = 3;
    static constexpr uint8_t TYPE_BILLBOARD_XYZ = 4;

    static constexpr uint8_t FLAG_USE_POS = 1 << 0;
    static constexpr uint8_t FLAG_USE_SCALE = 1 << 1;
    static constexpr uint8_t FLAG_USE_ROT = 1 << 2;

    fm_vec3_t localRefPos{};
    uint16_t refObjId{};
    uint8_t type{};
    uint8_t flags{};

    static uint32_t getAllocSize([[maybe_unused]] uint16_t* initData)
    {
      return sizeof(Constraint);
    }

    static void initDelete([[maybe_unused]] Object& obj, Constraint* data, uint16_t* initData);
    static void update(Object& obj, Constraint* data, float deltaTime);
    static void draw(Object& obj, Constraint* data, float deltaTime);
  };
}