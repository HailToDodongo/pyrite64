/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "scene/object.h"
#include "scene/components/audioListener.h"

#include "audio/audio.h"
#include "lib/math.h"

namespace P64::Comp
{
  void AudioListener::initDelete([[maybe_unused]] Object &obj, AudioListener* data, uint16_t* initData)
  {
    if (initData == nullptr) {
      data->~AudioListener();
      return;
    }
    new(data) AudioListener();
  }

  void AudioListener::update(Object& obj, [[maybe_unused]] AudioListener* data, [[maybe_unused]] float deltaTime)
  {
    Audio::engine().setListener(
      Audio::toMeters(obj.pos),
      obj.rot * Math::VEC3_FORWARD,
      obj.rot * Math::VEC3_UP
    );
  }
}
