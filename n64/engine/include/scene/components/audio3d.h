/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include "audio/audio.h"
#include "scene/object.h"

namespace P64::Comp
{
  struct Audio3D
  {
    static constexpr uint32_t ID = 14;

    static constexpr uint8_t FLAG_LOOP = 1 << 0;
    static constexpr uint8_t FLAG_AUTO_PLAY = 1 << 1;

    TSQ::SfxSample *sample{};   ///< asset-owned
    TSQ::SFX3DConf conf{};      ///< per-component spatialization settings
    TSQ::Sfx3DHandle handle{};

    float volume{1.0f};
    float pitch{0.0f}; ///< semitone offset
    uint8_t flags{0};

    static uint32_t getAllocSize([[maybe_unused]] uint16_t* initData)
    {
      return sizeof(Audio3D);
    }

    static void initDelete([[maybe_unused]] Object& obj, Audio3D* data, uint16_t* initData);

    static void update(Object& obj, Audio3D* data, float deltaTime);

    /** (Re-)starts the SFX at the object's position. */
    void play(const Object &obj);

    /** Fades the SFX out. */
    void stop();

    /**
     * Game-driven occlusion (walls, doors) on top of the distance muffling.
     * @param amount 0 = free line of sight, 1 = fully occluded
     */
    void setOcclusion(float amount);

    [[nodiscard]] bool isPlaying() const;
  };
}
