#include "script/userScript.h"
#include "scene/sceneManager.h"

namespace P64::Script::CD4C7661C1068CED
{
  P64_DATA(
    // Put your arguments and runtime values bound to an object here.
    // If you need them to show up in the editor, add a [[P64::Name("...")]] attribute.
    //
    // Types that can be set in the editor:
    // - uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t
    // - float
    // - AssetRef<sprite_t>
    // - ObjectRef
    //
    // Other types can be used but are not exposed in the editor.
    [[P64::Name("Speed")]]
    float speed;

    [[P64::Name("Type")]]
    int type;

    float time;
    fm_vec3_t startPos;
  );

  // The following functions are called by the engine at different points in the object's lifecycle.
  // If you don't need a specific function you can remove it.

  void init(Object& obj, Data *data)
  {
    // initialization, this is called once when the object spawns
    data->startPos = obj.pos;
    data->time = 0;
  }

  void destroy(Object& obj, Data *data)
  {
    // clean-up, this is called when the object gets deleted
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    data->time += data->speed * deltaTime;
    // this is called once every frame, put your main logic here
    if(data->type == 0) {
      constexpr fm_vec3_t rotAxis{0.0f, 0.5f, 0.2f};
      fm_quat_rotate(&obj.rot, &obj.rot, &rotAxis, deltaTime * data->speed);
      fm_quat_norm(&obj.rot, &obj.rot);
    }

    if(data->type == 1) {
      obj.pos.y = data->startPos.y + fm_sinf(data->time) * 140;
    }
    if(data->type == 2) {
      obj.pos.x = data->startPos.x + fm_sinf(data->time) * 140;
    }
    if(data->type == 3) {
      obj.scale.x = (fm_sinf(data->time) * 0.25f + 0.75f) * 1.2f;
      obj.scale.z = (fm_cosf(data->time) * 0.25f + 0.75f) * 1.2f;
    }
  }

}
