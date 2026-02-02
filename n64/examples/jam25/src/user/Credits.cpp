#include "globals.h"
#include "script/userScript.h"
#include "../p64/assetTable.h"
#include "systems/fonts.h"
#include "systems/screenFade.h"

namespace
{
  constexpr rdpq_textparms_t TEXT_PARAMS{
    .width = 320,
    .height = 32,
    .align = ALIGN_CENTER,
    .valign = VALIGN_TOP,
    .disable_aa_fix = true,
  };

  constexpr rdpq_textparms_t TXTP_DESC{
    .width = 320/2,
    .height = 64,
    .align = ALIGN_RIGHT,
    .valign = VALIGN_TOP,
    .disable_aa_fix = true,
  };
  constexpr rdpq_textparms_t TXTP_NAME{
    .width = 320/2,
    .height = 64,
    .align = ALIGN_LEFT,
    .valign = VALIGN_TOP,
    .disable_aa_fix = true,
  };

  constexpr float spaceMid = 8.0f;
  constexpr float centerX = 320 / 2.0f;
  constexpr float centerY = 240 / 2.0f;

  constexpr color_t COL_DEF = {0xFF, 0xFF, 0xFF, 0xFF};
  constexpr color_t COL_SUB = {0xAA, 0xAA, 0xAA, 0xFF};

  inline void setColor(uint8_t font, color_t col)
  {
    const rdpq_fontstyle_t style{.color = col};
    rdpq_font_style(const_cast<rdpq_font_t*>(rdpq_text_get_font(font)), 0, &style);
  }

  void titleBlock(float &posY, const char* txt)
  {
    posY += 12;
    rdpq_text_print(&TEXT_PARAMS, P64::User::FONT_TITLE, 0, posY, txt);
    posY += 32;
  }

  void splitTextBlock(float &posY, const char* txtLeft, const char* txtRight)
  {
    setColor(P64::User::FONT_SMALL, COL_SUB);
    rdpq_text_print(&TXTP_DESC, P64::User::FONT_SMALL, -spaceMid, posY, txtLeft);
    setColor(P64::User::FONT_SMALL, COL_DEF);
    rdpq_text_print(&TXTP_NAME, P64::User::FONT_SMALL, centerX+spaceMid, posY, txtRight);
    posY += 36;
  }

  void logoSubtext(float &posY, sprite_t *tex, const char *subText, float offsetX = 0)
  {
    rdpq_mode_combiner(RDPQ_COMBINER_TEX);
    rdpq_sprite_blit(tex, centerX - (tex->width / 2.0f) + offsetX, posY, nullptr);

    posY += tex->height + 8;
    rdpq_text_print(&TEXT_PARAMS, P64::User::FONT_SMALL, 0, posY, subText);
    posY += 40;
  }

  constinit sprite_t *texTitle{};
  constinit sprite_t *logoPyrite{};
  constinit sprite_t *logoTiny3d{};
  constinit sprite_t *logoLibdragon{};
  constinit sprite_t *bgSpike{};
}

namespace P64::Script::C10BED11E4F936F7
{
  P64_DATA(
    // Put your arguments here if needed, those will show up in the editor.
    //
    // Allowed types:
    // - uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t
    // - float
    int state;
    float timer;
    float logoAlpha;
    float scrollEnd;
    float colorTimer;
    rspq_block_t *dplBgTex;
  );

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    if(isDelete)
    {
      rspq_call_deferred((void(*)(void*))rspq_block_free, data->dplBgTex);
      texTitle = nullptr;
      return;
    }

    memset(data, 0, sizeof(Data));
    obj.pos.y = 200;

    texTitle = (sprite_t*)AssetManager::getByIndex("titlescreen_text.rgba16.sprite"_asset);
    logoPyrite = (sprite_t*)AssetManager::getByIndex("ui/logoPyrite.sprite"_asset);
    logoTiny3d = (sprite_t*)AssetManager::getByIndex("ui/logoTiny3d.sprite"_asset);
    logoLibdragon = (sprite_t*)AssetManager::getByIndex("ui/logoLibdragon.sprite"_asset);
    bgSpike = (sprite_t*)AssetManager::getByIndex("ui/triSpike.i4.sprite"_asset);

    rspq_block_begin();
      rdpq_texparms_t bgParam{};
      bgParam.s.repeats = REPEAT_INFINITE;
      bgParam.t.repeats = REPEAT_INFINITE;
      bgParam.t.mirror = true;
      bgParam.s.mirror = true;
      bgParam.s.scale_log = 0;
      bgParam.t.scale_log = -2;
      rdpq_sprite_upload(TILE0, bgSpike, &bgParam);

      rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (0,0,0,1)));
      rdpq_fill_rectangle(0, 0, 16, 240);
      rdpq_fill_rectangle(320-16, 0, 320, 240);

      rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (0,0,0,TEX0)));
    data->dplBgTex = rspq_block_end();
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    auto held = joypad_get_buttons_held(JOYPAD_PORT_1);
    if(held.a || held.b)
    {
      deltaTime *= 10.0f;
    }

    data->logoAlpha = fminf(data->logoAlpha + deltaTime*0.25f, 1.0f);
    data->colorTimer += deltaTime;

    switch(data->state)
    {
      case 0:
        data->state = 1;
        break;
      case 1:
        obj.pos.y -= deltaTime * 16;
        break;
      case 2:
        data->timer -= deltaTime;
        if(data->timer <= 0)
        {
          data->state = 3;
          User::ScreenFade::fadeOut(0, 7.0f);
        }
        break;
      case 3:
        if(User::ScreenFade::isDone())
        {
          SceneManager::load(2);
          obj.remove();
        }
      default: break;
    }
  }

  void draw(Object& obj, Data *data, float deltaTime)
  {
    float baseY = obj.pos.y;
    baseY = roundf(baseY);

    DrawLayer::use2D();

    rdpq_set_mode_standard();

    rdpq_mode_begin();
      rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
      rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
    rdpq_mode_end();

    float spriteY = baseY;
    spriteY = fminf(spriteY, 80);

    if(data->logoAlpha > 0)
    {
      uint8_t alpha = data->logoAlpha * 255;
      rdpq_set_prim_color({0xFF, 0xFF, 0xFF, alpha});
      rdpq_sprite_blit(texTitle, centerX - (texTitle->width / 2.0f), spriteY, nullptr);
      rdpq_set_prim_color({0xFF, 0xFF, 0xFF, 0xFF});
    }

    baseY += texTitle->height + 16;

    auto rainbowCol = User::getRainbowColor(data->colorTimer);
    rdpq_set_prim_color(rainbowCol);
    rspq_block_run(data->dplBgTex);
    float offsetSide = 16.0f - fmodf(obj.pos.y * 0.5f, 16.0f);
    rdpq_texture_rectangle_scaled(TILE0, 16,     0, 32,     240, 0,  offsetSide, 32, 240+offsetSide);
    rdpq_texture_rectangle_scaled(TILE0, 320-32, 0, 320-16, 240, 32, offsetSide, 64, 240+offsetSide);

    rdpq_mode_alphacompare(0);

    //const rdpq_fontstyle_t style{.color =  asecolor};
    //rdpq_font_style(const_cast<rdpq_font_t*>(rdpq_text_get_font(User::FONT_TITLE)), 0, &style);

    setColor(User::FONT_SMALL, COL_SUB);
    rdpq_text_print(&TEXT_PARAMS, User::FONT_SMALL, 0, baseY, "Made for the");
    setColor(User::FONT_SMALL, COL_DEF);

    baseY += 10;
    rdpq_text_print(&TEXT_PARAMS, User::FONT_TITLE, 0, baseY, "N64Brew Game Jam 2025");
    baseY += 50;

    titleBlock(baseY, "- Developer -");
    splitTextBlock(baseY, "Created by", "HailToDodongo\n(Max Bebök)");

    titleBlock(baseY, "- Music -");
    splitTextBlock(baseY, "Title-Screen", "sea_of_symbols\nJohn Oestmann (CC0)");
    splitTextBlock(baseY, "1. Map\n'A Broken World'", "0x2A731 discovery fr.\nJohn Oestmann (CC0)");
    splitTextBlock(baseY, "2. Map\n'Cardboard Course'", "orions-geology-workshop\nJohn Oestmann (CC0)");
    splitTextBlock(baseY, "Credits", "proto_spokehul\nJohn Oestmann (CC0)");

    titleBlock(baseY, "- Created with - ");

    logoSubtext(baseY, logoPyrite, "github.com/HailToDodongo/pyrite64");
    logoSubtext(baseY, logoTiny3d, "github.com/HailToDodongo/tiny3d");
    logoSubtext(baseY, logoLibdragon, "github.com/DragonMinded/libdragon", -16.0f);

    titleBlock(baseY, "Special Thanks");

    rdpq_text_print(&TEXT_PARAMS, User::FONT_SMALL, 0, baseY,
      "To the N64Brew server,\n"
      "blender, and the fast64 project"
    );
    baseY += 38;

    baseY += 96;

    setColor(User::FONT_TITLE, rainbowCol);
    rdpq_text_print(&TEXT_PARAMS, User::FONT_TITLE, 0, baseY, "Thank you for playing!");
    setColor(User::FONT_TITLE, COL_DEF);

    if(data->state == 1 && (baseY+10) < centerY)
    {
      data->state =  2;
      data->timer = 2.0f;
    }

    //rdpq_text_print(&TEXT_PARAMS, User::FONT_TITLE, 0, 64, "ABCD abcd yy");
    //rdpq_text_print(&TEXT_PARAMS, User::FONT_TITLE, 0, 128.5f, "ABCD abcd yy");

    DrawLayer::useDefault();
  }
}
