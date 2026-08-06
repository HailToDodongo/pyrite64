/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "scene/object.h"
#include "scene/components/audio3d.h"

#include "audio/audio.h"

namespace
{
  struct InitData
  {
    uint16_t assetIdx;
    uint16_t volume;
    uint8_t flags;
    uint8_t padding[3];
    float nearDist;
    float maxDist;
    float rolloff;
    float dopplerScale;
    float lowPassStart;
    float reverbNear;
    float reverbFar;
    float pitch;
  };
}

namespace P64::Comp
{
  void Audio3D::initDelete(Object &obj, Audio3D* data, uint16_t* initData_)
  {
    auto initData = (InitData*)initData_;
    if (initData == nullptr) {
      Audio::sfx->stopNote(data->handle);
      data->~Audio3D();
      return;
    }

    new(data) Audio3D();

    data->volume = (float)initData->volume * (1.0f / 0xFFFF);
    data->pitch = initData->pitch;
    data->flags = initData->flags;
    data->conf.nearDist = initData->nearDist;
    data->conf.maxDist = initData->maxDist;
    data->conf.rolloff = initData->rolloff;
    data->conf.dopplerScale = initData->dopplerScale;
    data->conf.lowPassStart = initData->lowPassStart;
    data->conf.reverbNear = initData->reverbNear;
    data->conf.reverbFar = initData->reverbFar;

    data->sample = &Audio::sample(initData->assetIdx);

    if(data->flags & FLAG_AUTO_PLAY)data->play(obj);
  }

  void Audio3D::update(Object& obj, Audio3D* data, [[maybe_unused]] float deltaTime)
  {
    Audio::sfx->setPosition(data->handle, Audio::toMeters(obj.pos));
  }

  void Audio3D::play(const Object &obj)
  {
    if(!sample)return;
    Audio::sfx->stopNote(handle);
    handle = Audio::sfx->playNote3D(*sample, 60, Audio::toMeters(obj.pos), &conf,
      127, volume, 0, (flags & FLAG_LOOP) != 0);
    if(pitch != 0.0f)Audio::sfx->setPitch(handle, pitch);
  }

  void Audio3D::stop()
  {
    Audio::sfx->stopNote(handle);
  }

  void Audio3D::setOcclusion(float amount)
  {
    Audio::sfx->setOcclusion(handle, amount);
  }

  bool Audio3D::isPlaying() const
  {
    return Audio::sfx->isNotePlaying(handle);
  }
}
