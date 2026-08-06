/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene/object.h"
#include "scene/components/audio2d.h"

#include "audio/audio.h"

#include <cstring>

namespace
{
  struct InitData
  {
    uint16_t assetIdx;
    uint16_t bankIdx;   // sound-font for sequences, 0xFFFF = XM companion bank
    uint16_t volume;
    uint8_t flags;
    uint8_t padding;
    float pitch;
  };
}

namespace P64::Comp
{
  void Audio2D::initDelete([[maybe_unused]] Object &obj, Audio2D* data, uint16_t* initData_)
  {
    auto initData = (InitData*)initData_;
    if (initData == nullptr) {
      if(data->seq)Audio::engine().destroyPlayer(data->seq);
      if(data->stream)Audio::engine().destroyStreamPlayer(data->stream);
      if(data->sample && Audio::sfx)Audio::sfx->stopNote(data->handle);
      delete data->ownedBank; // after the player, it releases its retained samples first
      data->~Audio2D();
      return;
    }

    new(data) Audio2D();

    data->volume = (float)initData->volume * (1.0f / 0xFFFF);
    data->pitch = initData->pitch;
    data->flags = initData->flags;

    switch(data->mode())
    {
      case MODE_SAMPLE:
        data->sample = &Audio::sample(initData->assetIdx);
        break;

      case MODE_SEQUENCE: {
        const char *seqPath = Audio::path(initData->assetIdx);
        TSQ::SoundBank *bank;
        if(initData->bankIdx == 0xFFFF) {
          // XM: the converter bakes a companion bank next to the sequence
          char bankPath[256];
          strlcpy(bankPath, seqPath, sizeof(bankPath));
          auto len = strlen(bankPath);
          assertf(len > 3, "Audio2D: invalid sequence path: %s", seqPath);
          strcpy(&bankPath[len-3], "tsf");
          data->ownedBank = TSQ::SoundBank::load(bankPath);
          bank = data->ownedBank;
        } else {
          bank = Audio::bank(initData->bankIdx);
        }
        assertf(bank, "Audio2D: sequence needs a sound-font");
        data->seq = Audio::engine().createPlayer(bank, seqPath, data->isLooping());
        if(data->seq)data->seq->setVolume(data->volume);
      } break;

      case MODE_STREAM:
        data->stream = Audio::engine().createStreamPlayer();
        if(data->stream && !data->stream->open(Audio::path(initData->assetIdx))) {
          Audio::engine().destroyStreamPlayer(data->stream);
          data->stream = nullptr;
        }
        if(data->stream) {
          data->stream->setLooping(data->isLooping());
          data->stream->setVolume(data->volume);
        }
        break;
    }

    if(data->flags & FLAG_AUTO_PLAY)data->play();
  }

  void Audio2D::play()
  {
    switch(mode())
    {
      case MODE_SAMPLE:
        if(sample && Audio::sfx) {
          Audio::sfx->stopNote(handle);
          handle = Audio::sfx->playNote(*sample, 60, 127, 0.0f, 0.0f, volume, 0, isLooping());
          if(pitch != 0.0f)Audio::sfx->setPitch(handle, pitch);
        }
        break;
      case MODE_SEQUENCE: if(seq)seq->play(); break;
      case MODE_STREAM: if(stream)stream->play(); break;
    }
  }

  void Audio2D::stop()
  {
    switch(mode())
    {
      case MODE_SAMPLE: if(Audio::sfx)Audio::sfx->stopNote(handle); break;
      case MODE_SEQUENCE: if(seq)seq->stop(); break;
      case MODE_STREAM: if(stream)stream->stop(); break;
    }
  }

  void Audio2D::setVolume(float vol)
  {
    volume = vol;
    switch(mode())
    {
      case MODE_SAMPLE: if(Audio::sfx)Audio::sfx->setVolume(handle, vol); break;
      case MODE_SEQUENCE: if(seq)seq->setVolume(vol); break;
      case MODE_STREAM: if(stream)stream->setVolume(vol); break;
    }
  }

  bool Audio2D::isPlaying() const
  {
    switch(mode())
    {
      case MODE_SAMPLE: return Audio::sfx && Audio::sfx->isNotePlaying(handle);
      case MODE_SEQUENCE: return seq && seq->isPlaying();
      case MODE_STREAM: return stream && stream->isPlaying();
    }
    return false;
  }
}
