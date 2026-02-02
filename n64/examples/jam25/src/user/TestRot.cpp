#include "script/userScript.h"

namespace P64::Script::C56418A1C2516806
{
  P64_DATA(
    [[P64::Name("Speed")]]
    float speed;
  );

  void update(Object& obj, Data *data, float deltaTime)
  {
    fm_vec3_t r{0.0f, 0.0f, 1.0f};
    fm_quat_rotate(&obj.rot, &obj.rot, &r, data->speed * deltaTime);
    fm_quat_norm(&obj.rot, &obj.rot);
  }
}
