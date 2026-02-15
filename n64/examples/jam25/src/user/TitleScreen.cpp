#include "globals.h"
#include "script/userScript.h"
#include "systems/screenFade.h"
#include "vi/swapChain.h"
#include <scene/components/nodeGraph.h>

#include "systems/dialog.h"
#include "../p64/assetTable.h"

namespace
{
  constexpr float FADE_TIME = 1.0f;

  constexpr rdpq_textparms_t TEXT_CENTER{
    .width = 320,
    .align = ALIGN_CENTER,
    .disable_aa_fix = true
  };

  fm_vec3_t getCamPos(float t)
  {
    // circle around planet (origin)
    float radius = 360.0f;// + (sinf(t) * 150.0f);
    float speed = 10.2f; // radians per second
    float x = radius * cosf(speed * t);
    float z = radius * sinf(speed * t);
    return {x, 50, z};
  }
}

namespace P64::Script::C0CC4367B29A36ED
{
  P64_DATA(
    [[P64::Name("Title")]]
    AssetRef<sprite_t> texTitle;

    float fadeTime;
    float focusTime;
    bool isFadeOut;

    float camTime;
    int selOption;
    joypad_8way_t lastDirInp;
  );

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    data->fadeTime = 1.0f;
    data->focusTime = 1.0f;
    data->camTime = 0;
    data->selOption = 0;
    data->isFadeOut = false;
    data->lastDirInp = JOYPAD_8WAY_NONE;
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    auto pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
    auto upDown = joypad_get_direction(JOYPAD_PORT_1, JOYPAD_2D_ANY);

    // Camera
    data->camTime += deltaTime * 0.02f;
    auto &sc = SceneManager::getCurrent();
    auto &cam = sc.getActiveCamera();

    auto headPos = obj.getScene().getObjectById(3)->pos;
    headPos.x = 0;

    obj.pos = getCamPos(data->camTime);
    fm_vec3_t target{0,0,0};

    fm_vec3_lerp(&obj.pos, &target, &obj.pos, data->focusTime);
    fm_vec3_lerp(&target, &headPos, &target, data->focusTime);

    cam.setLookAt(obj.pos, target);

    // Menu logic
    if(upDown == data->lastDirInp) {
      upDown = JOYPAD_8WAY_NONE;
    } else {
      data->lastDirInp = upDown;
    }

    auto graphComp = obj.getComponent<Comp::NodeGraph>();
    if(!graphComp->isRunning())
    {
      if(upDown == JOYPAD_8WAY_UP) {
        data->selOption = (data->selOption + 1) % 2;
        AudioManager::play2D("sfx/UiSelect.wav64"_asset).setVolume(0.3f);
      } else if(upDown == JOYPAD_8WAY_DOWN) {
        data->selOption = (data->selOption + 1) % 2;
        AudioManager::play2D("sfx/UiSelect.wav64"_asset).setVolume(0.3f);
      } else
      {
        if(pressed.start || pressed.a) {
          AudioManager::play2D("sfx/UiOk.wav64"_asset).setVolume(0.3f);
          graphComp->run(data->selOption);
        }
      }
    }
  }

  void draw(Object& obj, Data *data, float deltaTime)
  {
    float screenMidX = rdpq_get_attached()->width * 0.5f;
    uint8_t alpha = 0xFF;
    if(data->isFadeOut)
    {
      data->fadeTime = fmaxf(data->fadeTime - deltaTime, 0.0f);
      data->focusTime = fmaxf(data->focusTime - deltaTime*0.5f, 0.0f);
      alpha = (data->fadeTime / FADE_TIME) * 255;
    }

    if(alpha == 0)return;

    DrawLayer::use2D();

    rdpq_set_mode_standard();
    rdpq_mode_begin();
      rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
      rdpq_mode_alphacompare(15);
      rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
    rdpq_mode_end();

    auto logo = data->texTitle.get();

    rdpq_set_prim_color({0xFF, 0xFF, 0xFF, alpha});
    rdpq_sprite_blit(logo, screenMidX - logo->width/2, 50, nullptr);

    uint8_t textAlpha = 0xFF - (fm_sinf(get_ticks_ms() * 0.005f) * 0.5f + 0.5f) * 150;
    if(data->isFadeOut)textAlpha = alpha;

    rdpq_fontstyle_t style{.color =  {0xFF, 0xFF, 0xFF, textAlpha}};
    rdpq_font_style(const_cast<rdpq_font_t*>(rdpq_text_get_font(User::FONT_TITLE)), 0, &style);

    constexpr float posYStart = 150;
    constexpr float optionSpacing = 16.0f;

    float posY = posYStart - 12;

    User::Dialog::drawBoxBgSart();
    rdpq_set_prim_color({0,0,0, (uint8_t)(alpha/2)});
    for(int i=0; i < 2; ++i)
    {
      //rdpq_set_prim_color({0x44, 0x44, 0x99, 0x80});
      User::Dialog::drawBoxBg(screenMidX - (140/2), posY, 140, 16);
      posY += optionSpacing;
    }

    rdpq_fontstyle_t optStyleNormal{.color =  {0x80, 0x80, 0x80, alpha}};
    rdpq_fontstyle_t optStyleSelected{.color =  {0xDD, 0xCC, 0x20, textAlpha}};
    rdpq_fontstyle_t optStyleCredits{.color =  {0xFF, 0xFF, 0xFF, alpha}};

    constexpr const char* OPTIONS[2] {
      "Start Game",
      "Credits"
    };
    for(int i=0; i < 2; ++i)
    {
      rdpq_font_style(const_cast<rdpq_font_t*>(rdpq_text_get_font(User::FONT_TEXT)), 0, data->selOption == i ? &optStyleSelected : &optStyleNormal);
      rdpq_text_printf(&TEXT_CENTER, User::FONT_TEXT, 0, posYStart + i * optionSpacing, OPTIONS[i]);
    }

    rdpq_font_style(const_cast<rdpq_font_t*>(rdpq_text_get_font(User::FONT_SMALL)), 0, &optStyleCredits);
    rdpq_text_printf(&TEXT_CENTER, User::FONT_SMALL, 0, 210, "©2025 Max Bebök (HailToDodongo)");

    DrawLayer::useDefault();
  }

  void onEvent(Object& obj, Data *data, const ObjectEvent &event)
  {
    if(event.type != 0)return;
    if(event.value == "MenuOut"_hash)
    {
      data->isFadeOut = true;
    }
  }
}
