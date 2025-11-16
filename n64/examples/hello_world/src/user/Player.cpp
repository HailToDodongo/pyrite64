#include "scene/sceneManager.h"

namespace P64::Script::C0FCD808BF296813
{
  struct Data
  {
    // Put your arguments here if needed, those will show up in the editor.
    //
    // Allowed types:
    // - uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t
    // - float

    float camAngleTarget = 0.0f;
    float camAngle = 0.0f;
  };

  void update(Object& obj, Data *data)
  {
    auto pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
    auto held = joypad_get_buttons(JOYPAD_PORT_1);
    auto inp = joypad_get_inputs(JOYPAD_PORT_1);

    data->camAngleTarget += -inp.cstick_x * 0.0005f;
    data->camAngle = t3d_lerp_angle(data->camAngle, data->camAngleTarget, 0.2f);

    float speed = 3.5f;
    float stick[2] = {
      inp.stick_x / 100.0f,
      inp.stick_y / 100.0f
    };
    fm_vec3_t camDist{0, 100.0f, 120.0f};

    if(inp.btn.d_left)speed *= 10;

    float cosA = fm_cosf(data->camAngle);
    float sinA = fm_sinf(data->camAngle);

    fm_vec3_t camOffset{
      camDist.z * sinA,
      camDist.y,
      camDist.z * cosA
    };

    fm_vec3_t move{stick[0], 0, -stick[1]};
    // rotate move by camAngle
    sinA = -sinA;

    fm_vec3_t moveRotated{
      move.x * cosA - move.z * sinA,
      move.y,
      move.x * sinA + move.z * cosA
    };
    if(inp.btn.c_up)moveRotated.y += 1.0f;
    if(inp.btn.c_down)moveRotated.y -= 1.0f;

    obj.pos += moveRotated * speed;
    //debugf("pos: %.2f, %.2f, %.2f\n", obj.pos.x, obj.pos.y, obj.pos.z);

    auto &cam = SceneManager::getCurrent().getActiveCamera();
    cam.setTarget(obj.pos);
    cam.setPos(obj.pos + camOffset);
  }

  void draw(Object& obj, Data *data)
  {
  }
}
