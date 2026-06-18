#include "script/userScript.h"
#include "scene/sceneManager.h"
#include "scene/components/light.h"

namespace P64::Script::CEC77FAB51D7B753
{
  P64_DATA();

  void init(Object& obj, Data *data)
  {
  }

  void destroy(Object& obj, Data *data)
  {
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    auto held = joypad_get_buttons_held(JOYPAD_PORT_1);
    auto light = obj.getComponent<Comp::Light>();

    constexpr float moveSpeed = 8.0f;
    constexpr float sizeSpeed = 8.0f;

    light->size += held.a * sizeSpeed;
    light->size -= held.b * sizeSpeed;
  
    if(held.z) {
      obj.pos.y += held.c_up * moveSpeed;
      obj.pos.y -= held.c_down * moveSpeed;
    } else {
      obj.pos.z -= held.c_up * moveSpeed;
      obj.pos.z += held.c_down * moveSpeed;
    }
    obj.pos.x -= held.c_left * moveSpeed;
    obj.pos.x += held.c_right * moveSpeed;
  }

}
