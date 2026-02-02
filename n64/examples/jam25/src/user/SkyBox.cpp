#include <scene/components/model.h>

#include "script/userScript.h"
#include "systems/context.h"

namespace
{
  //constexpr fm_vec3_t rotAxis = {-0.9f,0.1f,0.3f};
  constexpr fm_vec3_t rotAxis = {-0.9f,0.5f,0.3f};

  color_t lerpColor(const color_t &c0, const color_t &c1, float t)
  {
    uint8_t r = c0.r + (c1.r - c0.r) * t;
    uint8_t g = c0.g + (c1.g - c0.g) * t;
    uint8_t b = c0.b + (c1.b - c0.b) * t;
    uint8_t a = c0.a + (c1.a - c0.a) * t;
    return {r, g, b, a};
  }
}

namespace P64::Script::C6912C680DE13230
{
  P64_DATA(
    // Put your arguments here if needed, those will show up in the editor.
    //
    // Allowed types:
    // - uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t
    // - float
    float speed;
  );

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    //fm_quat_rotate(&obj.rot, &obj.rot, &rotAxis, 3.2f);
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    //fm_quat_rotate(&obj.rot, &obj.rot, &rotAxis, -deltaTime * data->speed);
    //fm_quat_norm(&obj.rot, &obj.rot);
    //obj.pos = SceneManager::getCurrent().getActiveCamera().getPos();
  }

  void draw(Object& obj, Data *data, float deltaTime)
  {
    /*
    obj.pos.y = 0;
    auto &scene = obj.getScene();
    float height = scene.getActiveCamera().getTarget().y;

    constexpr float minHeight = 0.4f;
    constexpr float maxHeight = 400.0f;
    float lerp = (height) / (maxHeight) - minHeight;
    lerp = Math::clamp(lerp, 0.0f, 1.0f);

    auto model = obj.getComponent<Comp::Model>();
    auto fogColor = lerpColor(
      {0xFF,0xFF,0xFF,0xFF},
      model->material.colorEnv,
      lerp
    );
    //auto &layer0 = scene.getConf().layerSetup.layerConf[0];
    scene.getConf().clearColor = fogColor;
    debugf("Fog Color: %d %d %d %d | %f\n", fogColor.r, fogColor.g, fogColor.b, fogColor.a, lerp);
*/
  }
}
