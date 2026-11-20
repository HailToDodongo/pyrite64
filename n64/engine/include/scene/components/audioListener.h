/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include "scene/object.h"

namespace P64::Comp
{
  /**
   * Sets the 3D audio listener for this frame.
   */
  struct AudioListener
  {
    static constexpr uint32_t ID = 15;

    static constexpr uint8_t FLAG_CAMERA_ROT = 1 << 0;

    uint16_t posObjId{};
    uint8_t flags{};

    static uint32_t getAllocSize([[maybe_unused]] uint16_t* initData)
    {
      return sizeof(AudioListener);
    }

    static void initDelete(Object& obj, AudioListener* data, uint16_t* initData);

    static void update(Object& obj, AudioListener* data, float deltaTime);
  };
}
