#include "script/userScript.h"
#include "scene/sceneManager.h"

namespace
{
  constexpr float bobSpeed = 2.0f;
  constexpr float wobbleAmplitude = 40.0f;
  constexpr float wobbleSpeed = 1.2f;
  constexpr float rotAmplitude = 0.10f;
  constexpr float rotSpeed = 1.1f;
}

namespace P64::Script::C3F36ABD43F3FD05
{
  P64_DATA(
    // Put your arguments here if needed, those will show up in the editor.
    //
    // Types that can be set in the editor:
    // - uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t
    // - float
    // - AssetRef<sprite_t>
    uint32_t currEvent;
    float time;   // Time accumulator for animation
  );

  // The following functions are called by the engine at different points in the object's lifecycle.
  // If you don't need a specific function you can remove it.

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    if(isDelete) {
      // do cleanup
      return;
    }
    data->currEvent = 0;
    data->time = 0.0f;
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    data->time += deltaTime;

    float wobbleOffset = fm_cosf(data->time * wobbleSpeed) * wobbleAmplitude;
    wobbleOffset += 5;

    // Rotation (roll and pitch)
    float roll = fm_sinf(data->time * rotSpeed) * rotAmplitude; // Z axis (side to side)
    float pitch = fm_cosf(data->time * bobSpeed) * (rotAmplitude * 0.7f); // X axis (up/down)
    fm_quat_from_euler_zyx(&obj.rot, pitch, 0.0f, roll);

    switch(data->currEvent)
    {
      case "MoveIn"_hash:
      {
        // Move towards target Z as before
        if(obj.pos.z < 0)
        {
          constexpr float targetZ = -310.0f;
          constexpr float slowDownDist = 130.0f;
          float distToTarget = targetZ - obj.pos.z;
          float speedFactor = fminf(distToTarget / slowDownDist, 1.0f);
          obj.pos.z += 600.0f * speedFactor * deltaTime;
          if(obj.pos.z > targetZ)
          {
            obj.pos.z = targetZ;
          }
        }
      } break;
      default: break;
    }

    obj.pos.x = wobbleOffset;
  }

  void onEvent(Object& obj, Data *data, const ObjectEvent &event)
  {
    if(event.type != 0)return;
    data->currEvent = event.value;
    data->time = 0.0f;
  }
}
