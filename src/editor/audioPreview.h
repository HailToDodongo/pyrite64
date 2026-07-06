/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>
#include <string>

/**
 * Plays audio assets (.wav / .mp3) on the host machine,
 * used to preview them in the asset inspector without building a ROM.
 * Only one preview can play at a time.
 */
namespace Editor::AudioPreview
{
  struct Info
  {
    uint32_t sampleRate{};
    uint32_t channels{};
    float duration{}; // in seconds
  };

  // Starts playing a file, replacing any running preview.
  // Returns false if the file could not be decoded or no audio device is available.
  bool play(const std::string &path);
  void stop();

  // true while the given file is the active preview and still has data left to play
  bool isPlaying(const std::string &path);

  // Playback position of the active preview, 0.0 - 1.0
  float getProgress();

  // Jumps the active preview to the given position, 0.0 - 1.0
  void seek(float progress);

  // Sample-rate/channels/duration of a file, cached after the first call.
  // Returns false if the file could not be parsed.
  bool getInfo(const std::string &path, Info &out);
}
