/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "scene/object.h"
#include "scene/components/audio3d.h"

#include "audio/audioManager.h"

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
  };
}

namespace P64::Comp
{
  void Audio3D::initDelete(Object &obj, Audio3D* data, uint16_t* initData_)
  {
    auto initData = (InitData*)initData_;
    if (initData == nullptr) {
      data->handle.stop();
      data->~Audio3D();
      return;
    }

    new(data) Audio3D();

    data->volume = (float)initData->volume * (1.0f / 0xFFFF);
    data->flags = initData->flags;
    data->conf.nearDist = initData->nearDist;
    data->conf.maxDist = initData->maxDist;
    data->conf.rolloff = initData->rolloff;

    data->audio = (wav64_t*)AssetManager::getByIndex(initData->assetIdx);
    assert(data->audio);

    if(data->flags & FLAG_AUTO_PLAY)data->play(obj);
  }

  void Audio3D::update(Object& obj, Audio3D* data, [[maybe_unused]] float deltaTime)
  {
    data->handle.setPosition(obj.pos);
  }

  void Audio3D::play(const Object &obj)
  {
    handle.stop();
    handle = AudioManager::play3D(audio, obj.pos, conf, (flags & FLAG_LOOP) != 0);
    handle.setVolume(volume);
  }

  void Audio3D::stop()
  {
    handle.stop();
  }
}
