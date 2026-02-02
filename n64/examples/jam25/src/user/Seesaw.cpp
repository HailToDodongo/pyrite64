#include <debug/debugDraw.h>

#include "script/userScript.h"
#include "scene/sceneManager.h"
#include "systems/context.h"

namespace P64::Script::CF977D94F4DB7A71
{
  P64_DATA(
    fm_vec3_t playerPos;
    float lastDiffX;
    float rot;
    uint32_t state;
  );

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    if(isDelete) {
      return;
    }

    memset(data, 0, sizeof(Data));
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    fm_vec3_t rotAxis = {0, 0, 1};
    fm_quat_from_axis_angle(&obj.rot, &rotAxis, data->rot);
    fm_quat_norm(&obj.rot, &obj.rot);

    if(fabsf(data->lastDiffX) > 0.001f)
    {
      float diffX = obj.pos.x - data->playerPos.x;
      data->rot += diffX * deltaTime * 0.001f;
      data->state = 0;
    } else
    {
      float rotBack = deltaTime * 0.2f;
      if(data->rot < 0.0f) {
        data->rot += rotBack;
        if(data->rot > 0.0f)data->rot = 0.0f;
      } else if(data->rot > 0.0f) {
        data->rot -= rotBack;
        if(data->rot < 0.0f)data->rot = 0.0f;
      }
    }
    data->lastDiffX *= 0.9f;
  }

  void onCollision(Object& obj, Data *data, const Coll::CollEvent& event)
  {
    if(User::ctx.controlledId != event.otherBCS->obj->id)return;
    data->playerPos = event.otherBCS->obj->pos;
    data->lastDiffX = obj.pos.x - data->playerPos.x;
    data->state = 1;
  }
}
