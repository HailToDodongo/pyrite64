/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "audioPreview.h"
#include "../utils/logger.h"

#include <SDL3/SDL.h>
#include <cctype>
#include <unordered_map>
#include <vector>

#define MINIMP3_IMPLEMENTATION
#include <minimp3/minimp3_ex.h>

namespace
{
  SDL_AudioStream *stream{nullptr};
  std::string currentPath{};

  // decoded PCM of the active preview, kept around for replay/seeking
  std::vector<uint8_t> pcm{};
  SDL_AudioSpec pcmSpec{};

  std::unordered_map<std::string, Editor::AudioPreview::Info> infoCache{};

  bool ensureAudioInit()
  {
    if (SDL_WasInit(SDL_INIT_AUDIO))return true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
      Utils::Logger::log(std::string{"AudioPreview: failed to init SDL audio: "} + SDL_GetError(), Utils::Logger::LEVEL_ERROR);
      return false;
    }
    return true;
  }

  bool isMP3(const std::string &path)
  {
    if (path.size() < 4)return false;
    auto ext = path.substr(path.size() - 4);
    for (auto &c : ext)c = (char)tolower(c);
    return ext == ".mp3";
  }

  bool decodeFile(const std::string &path, std::vector<uint8_t> &out, SDL_AudioSpec &spec)
  {
    if (isMP3(path))
    {
      mp3dec_t dec;
      mp3dec_file_info_t info{};
      if (mp3dec_load(&dec, path.c_str(), &info, nullptr, nullptr) != 0 || !info.buffer) {
        return false;
      }
      spec = {SDL_AUDIO_S16, info.channels, info.hz};
      out.assign((uint8_t*)info.buffer, (uint8_t*)info.buffer + info.samples * sizeof(mp3d_sample_t));
      free(info.buffer);
      return !out.empty();
    }

    uint8_t *buf{nullptr};
    uint32_t len{0};
    if (!SDL_LoadWAV(path.c_str(), &spec, &buf, &len)) {
      return false;
    }
    out.assign(buf, buf + len);
    SDL_free(buf);
    return !out.empty();
  }

  Editor::AudioPreview::Info infoFromPCM(const std::vector<uint8_t> &data, const SDL_AudioSpec &spec)
  {
    size_t frames = data.size() / SDL_AUDIO_FRAMESIZE(spec);
    return {
      .sampleRate = (uint32_t)spec.freq,
      .channels = (uint32_t)spec.channels,
      .duration = (float)frames / (float)spec.freq,
    };
  }

  void restartFromOffset(size_t byteOffset)
  {
    SDL_ClearAudioStream(stream);
    SDL_PutAudioStreamData(stream, pcm.data() + byteOffset, (int)(pcm.size() - byteOffset));
    SDL_FlushAudioStream(stream); // no more data will follow, let the stream drain fully
    SDL_ResumeAudioStreamDevice(stream);
  }
}

namespace Editor::AudioPreview
{
  bool play(const std::string &path)
  {
    if (!ensureAudioInit())return false;

    // same file again (replay after stop/finish): reuse the decoded PCM
    if (stream && currentPath == path) {
      restartFromOffset(0);
      return true;
    }

    stop();

    SDL_AudioSpec spec{};
    std::vector<uint8_t> data{};
    if (!decodeFile(path, data, spec)) {
      Utils::Logger::log("AudioPreview: failed to decode: " + path, Utils::Logger::LEVEL_ERROR);
      return false;
    }

    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!stream) {
      Utils::Logger::log(std::string{"AudioPreview: failed to open audio device: "} + SDL_GetError(), Utils::Logger::LEVEL_ERROR);
      return false;
    }

    pcm = std::move(data);
    pcmSpec = spec;
    currentPath = path;
    infoCache[path] = infoFromPCM(pcm, pcmSpec);

    restartFromOffset(0);
    return true;
  }

  void stop()
  {
    if (stream) {
      SDL_DestroyAudioStream(stream); // also closes the device it opened
      stream = nullptr;
    }
    currentPath.clear();
    pcm.clear();
  }

  bool isPlaying(const std::string &path)
  {
    if (!stream || currentPath != path)return false;
    return SDL_GetAudioStreamQueued(stream) > 0;
  }

  float getProgress()
  {
    if (!stream || pcm.empty())return 0.0f;
    float left = (float)SDL_GetAudioStreamQueued(stream) / (float)pcm.size();
    return 1.0f - left;
  }

  void seek(float progress)
  {
    if (!stream || pcm.empty())return;
    if (progress < 0.0f)progress = 0.0f;
    if (progress > 1.0f)progress = 1.0f;

    size_t frameSize = SDL_AUDIO_FRAMESIZE(pcmSpec);
    size_t offset = (size_t)(progress * (float)pcm.size());
    offset -= offset % frameSize; // keep sample frames intact
    restartFromOffset(offset);
  }

  bool getInfo(const std::string &path, Info &out)
  {
    auto it = infoCache.find(path);
    if (it != infoCache.end()) {
      out = it->second;
      return true;
    }

    if (isMP3(path))
    {
      // scans the frame headers only, no full decode
      mp3dec_ex_t dec;
      if (mp3dec_ex_open(&dec, path.c_str(), MP3D_SEEK_TO_SAMPLE) != 0)return false;
      out = {
        .sampleRate = (uint32_t)dec.info.hz,
        .channels = (uint32_t)dec.info.channels,
        .duration = (float)(dec.samples / dec.info.channels) / (float)dec.info.hz,
      };
      mp3dec_ex_close(&dec);
    } else {
      SDL_AudioSpec spec{};
      uint8_t *buf{nullptr};
      uint32_t len{0};
      if (!SDL_LoadWAV(path.c_str(), &spec, &buf, &len))return false;
      std::vector<uint8_t> data(buf, buf + len);
      SDL_free(buf);
      out = infoFromPCM(data, spec);
    }

    infoCache[path] = out;
    return true;
  }
}
