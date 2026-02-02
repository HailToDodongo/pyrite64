#include "script/userScript.h"
#include "scene/sceneManager.h"

namespace P64::Script::C6588C0666A90159
{
  P64_DATA(
    [[P64::Name("Speed")]]
    float speed;
    [[P64::Name("Type")]]
    uint8_t type;
    [[P64::Name("Axis X")]]
    uint8_t axisX;
    [[P64::Name("Axis Y")]]
    uint8_t axisY;
    [[P64::Name("Axis Z")]]
    uint8_t axisZ;

    [[P64::Name("Radius")]]
    uint8_t radius;

    fm_vec3_t orgPos;
    float timer;
  );

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    if(isDelete) {
      return;
    }
    data->orgPos = obj.pos;
    data->timer = 0;
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    data->timer += deltaTime;
    if (data->timer > 3600.0f) {
      data->timer = 0.0f;
    }

    fm_vec3_t axis{
      (float)data->axisX,
      (float)data->axisY,
      (float)data->axisZ,
    };

    switch(data->type)
    {
      // rotate around
      case 0: default:
        fm_quat_rotate(&obj.rot, &obj.rot, &axis, data->speed * deltaTime);
        fm_quat_norm(&obj.rot, &obj.rot);
      break;

      // move back/forth along axis
      case 1: {
        float moveOffset = fm_sinf(data->timer * data->speed) * data->radius;
        obj.pos = data->orgPos + (axis * moveOffset);
      } break;

      // Move in circle around originPos
      case 2: {
        float circ = 2.0f * T3D_PI * data->radius;
        // get speed relative as distance traveled
        float travelSpeed = data->speed / circ;

        float angle = data->timer * travelSpeed;
        float s = fm_cosf(angle) * data->radius;
        float t = fm_sinf(angle) * data->radius;
        fm_vec3_t offset;

        if(data->axisY) {
          offset = {s, 0.0f, t};
        } else if(data->axisX) {
          offset = {0.0f, s, t};
        } else {
          offset = {s, t, 0.0f};
        }

        obj.pos = data->orgPos + offset;
      }

      break;
    }
  }

  void onEvent(Object& obj, Data *data, const ObjectEvent &event)
  {
  }
}
