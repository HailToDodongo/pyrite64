/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

namespace P64::Audio
{
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
      uint16_t slot{0};
      uint16_t uuid{0};

    public:
      Handle() = default;
      explicit Handle(uint16_t _slot, uint16_t _uuid) : slot{_slot}, uuid{_uuid} {}

      /**
       * Stops the audio, if already stopped nothing will happen.
       * Note that stopping will make the handle invalid.
       */
      void stop();
      void setVolume(float volume);
      void setSpeed(float speed);
  };
}

/**
 * Global audio manager.
 * This will manage creation and playback of all audio in the engine.
 */
namespace P64::AudioManager
{
  Audio::Handle play2D(wav64_t *audio);

  inline Audio::Handle play2D(uint32_t assetId) {
    return play2D((wav64_t*)AssetManager::getByIndex(assetId));
  }

  void stopAll();
}