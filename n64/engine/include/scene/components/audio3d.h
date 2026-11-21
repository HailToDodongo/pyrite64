/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include "audio/audioManager.h"
#include "scene/object.h"

namespace P64::Comp
{
  /**
   * Plays a sound at the position of its object.
   */
  struct Audio3D
  {
    static constexpr uint32_t ID = 14;

    static constexpr uint8_t FLAG_LOOP = 1 << 0;
    static constexpr uint8_t FLAG_AUTO_PLAY = 1 << 1;

    wav64_t *audio{};
    Audio::Conf3D conf{};
    Audio::Handle handle{};
    float volume{1.0f};
    uint8_t flags{0};

    static uint32_t getAllocSize([[maybe_unused]] uint16_t* initData)
    {
      return sizeof(Audio3D);
    }

    static void initDelete(Object& obj, Audio3D* data, uint16_t* initData);

    static void update(Object& obj, Audio3D* data, float deltaTime);

    /**
     * (Re-)starts the sound at the object's position.
     * Changes to 'conf' only take effect on the next play.
     */
    void play(const Object &obj);

    void stop();

    [[nodiscard]] bool isPlaying() {
      return !handle.isDone();
    }
  };
}
