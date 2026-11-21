/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "scene/object.h"
#include "scene/components/audioListener.h"

#include "../../audio/audioManagerPrivate.h"

namespace
{
  struct InitData
  {
    uint16_t posObjId;
    uint8_t flags;
    uint8_t padding;
  };
}

namespace P64::Comp
{
  void AudioListener::initDelete(Object &obj, AudioListener* data, uint16_t* initData_)
  {
    auto initData = (InitData*)initData_;
    if (initData == nullptr) {
      data->~AudioListener();
      return;
    }
    new(data) AudioListener();
    data->posObjId = initData->posObjId == 0 ? obj.id : initData->posObjId;
    data->flags = initData->flags;
  }

  void AudioListener::update(Object& obj, AudioListener* data, [[maybe_unused]] float deltaTime)
  {
    AudioManager::setListenerSource({
      .posObjId = data->posObjId,
      .rotObjId = (data->flags & FLAG_CAMERA_ROT) ? (uint16_t)0 : obj.id,
    });
  }
}
