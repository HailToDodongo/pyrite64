/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>
#include "assets/assetManager.h"

namespace P64::Audio
{
  /**
   * Custom distance-falloff curve for 3D audio.
   * @param t normalized distance, 0 at 'nearDist' and 1 at 'maxDist'
   * @param rolloff the 'rolloff' setting
   * @return volume, should be 0 at/after t=1
   */
  using FalloffFunc = float(*)(float t, float rolloff);

  /**
   * Settings for 3D audio
   */
  struct Conf3D
  {
    float nearDist{1.0f};  // full volume inside this distance
    float maxDist{50.0f};  // silent after this distance
    float rolloff{1.0f};   // inverse-distance rolloff, higher = faster falloff, 0 = linear
    FalloffFunc falloff{nullptr}; // optional custom falloff curve
  };

  /**
   * Audio handle, returned by the audio manager when playing audio.
   * This can be used to change settings after it started playing.
   *
   * Internally, this will only store 4 bytes as a reference,
   * so this object is fast and safe to copy and move.
   *
   * If the audio is already stopped, the handle will be invalidated.
   * You are still able to safely call methods on it, but they will be ignored.
   *
   * A default constructed handle will be invalid by default.
   */
  class Handle
  {
    private:
      static constexpr uint16_t SLOT_3D = 0x8000;

      uint16_t slot{0};
      uint16_t uuid{0};

    public:
      Handle() = default;
      explicit Handle(uint16_t _slot, uint16_t _uuid) : slot{_slot}, uuid{_uuid} {}

      static Handle create3D(uint16_t emitter, uint16_t uuid) {
        return Handle{(uint16_t)(emitter | SLOT_3D), uuid};
      }

      [[nodiscard]] constexpr bool is3D() const { return slot & SLOT_3D; }

      /**
       * Stops the audio, if already stopped nothing will happen.
       * Note that stopping will make the handle invalid.
       */
      void stop();
      void setVolume(float volume);
      void setSpeed(float speed);

      /**
       * Checks if the audio has finished.
       * Note: a looping 3D sound out of range is still considered playing.
       */
      bool isDone();

      /**
       * Sets the position of a 3D sound in world-space.
       * @param pos position in meters
       */
      void setPosition(const fm_vec3_t &pos);
  };
}

/**
 * Global audio manager.
 * This will manage creation and playback of all audio in the engine.
 */
namespace P64::AudioManager
{
  extern uint64_t ticksUpdate;

  void setMasterVolume(float volume);

  Audio::Handle play2D(wav64_t *audio, bool loop = false);
  Audio::Handle play2D(xm64player_t *audio);

  inline Audio::Handle play2D(uint32_t assetId, bool loop = false) {
    return play2D((wav64_t*)AssetManager::getByIndex(assetId), loop);
  }

  /**
   * Plays a sound at a position in the world.
   * Volume and panning depend on the position relative to the listener (see 'setListener').
   *
   * Looping sounds free their channel while out of range,
   * and continue where they would be by now once back in range.
   * One-shot sounds are stopped once out of range.
   *
   * @param audio audio to play
   * @param pos position in meters, can be updated via the handle
   * @param conf 3D settings, copied into the sound
   * @param loop true to loop the sound
   */
  Audio::Handle play3D(wav64_t *audio, const fm_vec3_t &pos, const Audio::Conf3D &conf = {}, bool loop = false);

  inline Audio::Handle play3D(uint32_t assetId, const fm_vec3_t &pos, const Audio::Conf3D &conf = {}, bool loop = false) {
    return play3D((wav64_t*)AssetManager::getByIndex(assetId), pos, conf, loop);
  }

  /**
   * Sets the listener all 3D sounds are relative to.
   * This is set automatically by the 'Audio Listener' component.
   *
   * @param pos position in meters
   * @param forward forward direction (normalized)
   * @param up up direction (normalized)
   */
  void setListener(const fm_vec3_t &pos, const fm_vec3_t &forward, const fm_vec3_t &up);

  void stopAll();
}
