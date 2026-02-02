#include "script/userScript.h"
#include "scene/sceneManager.h"

namespace P64::Script::C48BB14F061323F6
{
  P64_DATA(
    // Put your arguments here if needed, those will show up in the editor.
    //
    // Types that can be set in the editor:
    // - uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t
    // - float
    // - AssetRef<sprite_t>
    float camRotX;
    float camRotY;
    fm_vec3_t camDir;
    fm_vec3_t camPos;
    fm_vec3_t camTarget;
  );

  // The following functions are called by the engine at different points in the object's lifecycle.
  // If you don't need a specific function you can remove it.

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    if(isDelete) {
      // do cleanup
      return;
    }
    memset(data, 0, sizeof(*data));
    data->camRotX = 1.544792654048f;
    data->camRotY = 4.05f;
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    auto held = joypad_get_buttons_held(JOYPAD_PORT_1);
    auto joypad = joypad_get_inputs(JOYPAD_PORT_1);
    float camRotSpeed = deltaTime * 0.01f;
    float camSpeed = deltaTime * 20.01f;

    data->camDir.v[0] = fm_cosf(data->camRotX) * fm_cosf(data->camRotY);
    data->camDir.v[1] = fm_sinf(data->camRotY);
    data->camDir.v[2] = fm_sinf(data->camRotX) * fm_cosf(data->camRotY);
    t3d_vec3_norm(&data->camDir);

    if(held.z) {
      data->camRotX += (float)joypad.stick_x * camRotSpeed;
      data->camRotY += (float)joypad.stick_y * camRotSpeed;
    } else {
      data->camPos.v[0] += data->camDir.v[0] * (float)joypad.stick_y * camSpeed;
      data->camPos.v[1] += data->camDir.v[1] * (float)joypad.stick_y * camSpeed;
      data->camPos.v[2] += data->camDir.v[2] * (float)joypad.stick_y * camSpeed;

      data->camPos.v[0] += data->camDir.v[2] * (float)joypad.stick_x * -camSpeed;
      data->camPos.v[2] -= data->camDir.v[0] * (float)joypad.stick_x * -camSpeed;

      if(joypad.btn.c_up)data->camPos.v[1] += camSpeed * 15.0f;
      if(joypad.btn.c_down)data->camPos.v[1] -= camSpeed * 15.0f;
    }

    data->camTarget.v[0] = data->camPos.v[0] + data->camDir.v[0];
    data->camTarget.v[1] = data->camPos.v[1] + data->camDir.v[1];
    data->camTarget.v[2] = data->camPos.v[2] + data->camDir.v[2];
  }

  void draw(Object& obj, Data *data, float deltaTime)
  {
    auto &cam = obj.getScene().getActiveCamera();
    cam.setPos(data->camPos);
    cam.setTarget(data->camTarget);
  }

  void onEvent(Object& obj, Data *data, const ObjectEvent &event)
  {
  }

  void onCollision(Object& obj, Data *data, const Coll::CollEvent& event)
  {
  }
}
