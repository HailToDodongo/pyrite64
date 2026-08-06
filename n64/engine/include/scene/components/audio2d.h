/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "audio/audio.h"
#include "scene/object.h"

namespace P64::Comp
{
  /**
   * Plays a non-positional audio asset:
   * - `.tsw` samples via the shared Audio::sfx player
   * - `.tsq` sequences via an owned SequencePlayer (needs a bank)
   * - `.wav64` streams via an owned StreamPlayer
   * The mode is baked at build time from the referenced asset's type.
   */
  struct Audio2D
  {
    static constexpr uint32_t ID = 6;

    static constexpr uint8_t FLAG_LOOP = 1 << 0;
    static constexpr uint8_t FLAG_AUTO_PLAY = 1 << 1;

    static constexpr uint8_t MODE_SAMPLE = 0;
    static constexpr uint8_t MODE_SEQUENCE = 1;
    static constexpr uint8_t MODE_STREAM = 2;
    static constexpr uint8_t MODE_SHIFT = 2;

    TSQ::SfxSample *sample{};      ///< MODE_SAMPLE, asset-owned
    TSQ::SequencePlayer *seq{};    ///< MODE_SEQUENCE, engine-owned, destroyed with the component
    TSQ::StreamPlayer *stream{};   ///< MODE_STREAM, engine-owned, destroyed with the component
    TSQ::SoundBank *ownedBank{};   ///< XM only: companion bank loaded (and freed) by the component
    TSQ::SfxHandle handle{};       ///< MODE_SAMPLE, handle of the last play()

    float volume{1.0f};
    float pitch{0.0f};   ///< semitone offset, samples only
    uint8_t flags{0};

    static uint32_t getAllocSize([[maybe_unused]] uint16_t* initData)
    {
      return sizeof(Audio2D);
    }

    static void initDelete([[maybe_unused]] Object& obj, Audio2D* data, uint16_t* initData);

    static void update([[maybe_unused]] Object& obj, [[maybe_unused]] Audio2D* data,
                       [[maybe_unused]] float deltaTime) {}

    [[nodiscard]] constexpr uint8_t mode() const { return flags >> MODE_SHIFT; }
    [[nodiscard]] constexpr bool isLooping() const { return flags & FLAG_LOOP; }

    /** (Re-)starts playback. Samples retrigger, sequences/streams restart from the top. */
    void play();

    /** Stops playback (samples/sequences fade out). */
    void stop();

    /** Sets the volume of whatever is or will be playing. */
    void setVolume(float vol);

    [[nodiscard]] bool isPlaying() const;
  };
}
