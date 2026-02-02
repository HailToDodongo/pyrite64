#include "globals.h"
#include "script/userScript.h"
#include "scene/sceneManager.h"
#include "systems/screenFade.h"
#include "../p64/sceneTable.h"

namespace P64::Script::C4640C925988CA72
{
  P64_DATA(
      [[P64::Name("Logo Pyrite")]]
      AssetRef<sprite_t> texPyrite;
      [[P64::Name("Libdragon")]]
      AssetRef<sprite_t> texLibdragon;

      int currLogo;

      float fade;
      float fadeTarget;
    );

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    if(isDelete) {
      SceneManager::load("Menu"_scene);
      User::ScreenFade::setFadeState(false);
      return;
    }

    data->currLogo = 0;
    data->fade = 0;

    User::ScreenFade::setFadeState(true);
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    float dir = std::copysign(1.0f, data->fadeTarget - data->fade);
    data->fade += dir * deltaTime;
    data->fade = fminf(fmaxf(data->fade, 0.0f), 1.0f);
  }

  void onEvent(Object& obj, Data *data, const ObjectEvent &event)
  {
    if(event.type > 10)return;
    data->currLogo = event.type;
    data->fadeTarget = event.value;
  }

  void draw(Object& obj, Data *data, float deltaTime)
  {
    float midX = rdpq_get_attached()->width / 2;
    float midY = rdpq_get_attached()->height / 2;
    midY += 8;
    rdpq_textparms_t TEXT_CENTER{
      .width = (int16_t)rdpq_get_attached()->width,
      .align = ALIGN_CENTER,
      .disable_aa_fix = true
    };

    DrawLayer::use2D();

    if(data->fade != 0)
    {
      rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
      rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
      rdpq_set_prim_color({255, 255, 255, (uint8_t)(data->fade * 255)});

      auto logo = data->currLogo == 0 ? data->texPyrite.get() : data->texLibdragon.get();

      rdpq_sprite_blit(logo, midX - (logo->width/2), midY - (logo->height/2)-16, nullptr);
    }

    DrawLayer::useDefault();
  }
}
