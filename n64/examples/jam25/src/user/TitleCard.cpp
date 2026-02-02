#include <assets/assetManager.h>

#include "globals.h"
#include "script/userScript.h"

namespace
{
  constexpr float STAY_TIME = 3.0f;
  constexpr float FADE_TIME = 1.5f;

  constexpr int width = 256;
  constexpr int height = 32;
  constexpr int centerX = 320/2;
  constexpr int posX = centerX - (width / 2);
  constexpr int posY = 64;

  constexpr rdpq_textparms_t TEXT_PARAMS{
    .width = width,
    .height = height,
    .align = ALIGN_CENTER,
    .valign = VALIGN_CENTER,
    .disable_aa_fix = true,
  };
}

namespace P64::Script::C56A0C143A2EE1E7
{
  P64_DATA(
    // Put your arguments here if needed, those will show up in the editor.
    //
    // Allowed types:
    // - uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t
    // - float
    [[P64::Name("BG")]]
    AssetRef<sprite_t> texBg;
    [[P64::Name("Gradient")]]
    AssetRef<sprite_t> texGrad;
    [[P64::Name("Name")]]
    char name[32];

    float timer;
    rspq_block_t *dplBg;
  );

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    if(isDelete)
    {
      rspq_call_deferred((void(*)(void*))rspq_block_free, data->dplBg);
      return;
    }

    data->timer = 0;

    rspq_block_begin();

    rdpq_texparms_t p{};
    p.s.repeats = REPEAT_INFINITE;
    p.t.repeats = REPEAT_INFINITE;
    p.s.mirror = true;
    p.t.mirror = true;

    rdpq_tex_multi_begin();
      rdpq_sprite_upload(TILE0, data->texGrad.get(), &p);
      //rdpq_sprite_upload(TILE1, data->texGrad.get(), &p);
    rdpq_tex_multi_end();

    rdpq_mode_begin();
      rdpq_set_mode_standard();
      rdpq_mode_dithering(DITHER_BAYER_BAYER);
      rdpq_mode_combiner(RDPQ_COMBINER2(
        (0,0,0,0),         (PRIM,0,TEX0,0),
        (0,0,0,COMBINED),  (0,0,0,COMBINED)
      ));
      rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
    rdpq_mode_end();

    rdpq_texture_rectangle_scaled(TILE0,
      posX, posY, posX+width, posY+height,
      0,0,
      128, 32
    );
    data->dplBg = rspq_block_end();
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    data->timer += deltaTime;
    if(data->timer > (STAY_TIME+FADE_TIME)) {
      obj.remove();
    }
  }

  void draw(Object& obj, Data *data, float deltaTime)
  {
    DrawLayer::use2D();

    color_t color{0xFF, 0xFF, 0xFF, 0xFF};
    if(data->timer > STAY_TIME) {
      color.a = ((data->timer - STAY_TIME) / FADE_TIME) * 0xFF;
      color.a = 0xFF - color.a;
    }

    rdpq_set_prim_color(color);
    rspq_block_run(data->dplBg);

    /*auto held = joypad_get_buttons_held(JOYPAD_PORT_1);
    if(!held.a)
    {*/
    const rdpq_fontstyle_t style{.color =  color};
    rdpq_font_style(const_cast<rdpq_font_t*>(rdpq_text_get_font(User::FONT_TITLE)), 0, &style);
    rdpq_text_print(&TEXT_PARAMS, User::FONT_TITLE, posX, posY-1, data->name);

    DrawLayer::useDefault();
  }
}
