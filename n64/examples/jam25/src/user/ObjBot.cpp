#include "globals.h"
#include "script/userScript.h"
#include "systems/context.h"
#include "systems/marker.h"

#include "scene/components/collBody.h"

namespace
{
  constexpr float MOVE_SPEED = 100.0f;
  constexpr float MOVE_SPEED_SLOWDOWN = 0.75f;

  constexpr float ROT_SPEED = 1.6f;
  constexpr float ROT_SPEED_SLOWDOWN = 0.8f;

  constexpr fm_vec3_t UP{0.0f, 1.0f, 0.0f};
  constexpr fm_vec3_t COLL_OFFSET{0.0f, 20.0f, 0.0f};
}

namespace P64::Script::C4F4D286D6CB0DE3
{
  P64_DATA(
    [[P64::Name("Active")]]
    uint32_t active = 0;

    float turnVelocity;
    float moveVelocity;
  );

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    if(isDelete) {
      return;
    }

    debugf("Bot: id=%d parent=%d\n", obj.id, obj.group);
    data->moveVelocity = 0;
    data->turnVelocity = 0;

    if(data->active) {
      User::ctx.controlledId = obj.id;
    }
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    auto coll = obj.getComponent<Comp::CollBody>();
    //if(!coll)return;
    auto &bcs = coll->bcs;

    bcs.velocity.y -= 1000 * deltaTime;

    if(obj.id != User::ctx.controlledId)
    {
      auto &cam = SceneManager::getCurrent().getActiveCamera();
      auto screenPos = cam.getScreenPos(obj.pos + fm_vec3_t{0.0f, 20.0f, 0.0f});
      if(screenPos.z > 1)return;
      if(screenPos.x < 16 || screenPos.x > 320-16)return;
      if(screenPos.y < 16 || screenPos.y > 240-16)return;
      //debugf("screenPos.z: %f\n", 1.0f - screenPos.z);
      int16_t s = (1.0f - screenPos.z) * 150;

      User::Marker::addMarker({
        .pos = {(int16_t)screenPos.x, (int16_t)screenPos.y},
        .halfSize = {s,s},
        .objectId = obj.id,
      });
      return;
    }

    auto inp = joypad_get_inputs(JOYPAD_PORT_1);

    if(inp.btn.start)SceneManager::load(SceneManager::getCurrent().getId() == 1 ? 2 : 1);

    data->turnVelocity *= ROT_SPEED_SLOWDOWN;
    if(inp.stick_x != 0)
    {
      data->turnVelocity += -inp.stick_x * (1.0f / 127.0f);
      data->turnVelocity = Math::clamp(data->turnVelocity, -1.0f, 1.0f) * ROT_SPEED;
    }

    float rotY = data->turnVelocity * deltaTime;
    if(fabsf(rotY) > 0.0001f) {
      fm_quat_rotate(&obj.rot, &obj.rot, &UP, rotY);
    }

    data->moveVelocity *= MOVE_SPEED_SLOWDOWN;
    data->moveVelocity += inp.stick_y * -0.003f;
    data->moveVelocity = Math::clamp(data->moveVelocity, -1.0f, 1.0f);

    if(fabsf(data->moveVelocity) > 0.001f)
    {
      fm_vec3_t dir{0.0f, 0.0f, data->moveVelocity};
      dir = obj.rot * dir;
      //obj.pos += dir * (MOVE_SPEED * deltaTime);

      bcs.velocity.x = 0;
      bcs.velocity.z = 0;
      bcs.velocity += dir * MOVE_SPEED;
    }

    auto camPos = obj.pos + fm_vec3_t{0.0f, 39.0f, 0.0f};
    fm_vec3_t camTarget{0.0f, 0.0f, -100.0f};
    camTarget = camPos + (obj.rot * camTarget);

    auto &cam = SceneManager::getCurrent().getActiveCamera();
    cam.setLookAt(camPos, camTarget);
  }

  void onEvent(Object& obj, Data *data, const ObjectEvent &event)
  {
    switch(event.type)
    {
      case EVENT_TYPE_ENABLE:
        break;
      case EVENT_TYPE_DISABLE:
        break;
      default: break;
    }
  }
}
