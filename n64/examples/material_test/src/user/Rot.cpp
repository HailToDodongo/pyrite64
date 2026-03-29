#include "script/userScript.h"
#include "scene/sceneManager.h"

namespace P64::Script::C7D41AC8E22C9937
{
  P64_DATA(
    [[P64::Name("Speed")]]
    float speed = 1.0f;
  );


  void update(Object& obj, Data *data, float deltaTime)
  {
    constexpr fm_vec3_t rotAxis{0.0f, 0.5f, 0.2f};
    fm_quat_rotate(&obj.rot, &obj.rot, &rotAxis, deltaTime * data->speed);
    fm_quat_norm(&obj.rot, &obj.rot);
  }

}
