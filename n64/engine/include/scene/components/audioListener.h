/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include "audio/audio.h"
#include "scene/object.h"

namespace P64::Comp
{
  /**
   * Drives the 3D audio listener from its object's position and rotation.
   * All 3D SFX are spatialized relative to it.
   *
   * Exactly one enabled listener should exist.
   * Without any, the listener keeps its last state.
   */
  struct AudioListener
  {
    static constexpr uint32_t ID = 15;

    static uint32_t getAllocSize([[maybe_unused]] uint16_t* initData)
    {
      return sizeof(AudioListener);
    }

    static void initDelete([[maybe_unused]] Object& obj, AudioListener* data, uint16_t* initData);

    static void update(Object& obj, AudioListener* data, float deltaTime);
  };
}
