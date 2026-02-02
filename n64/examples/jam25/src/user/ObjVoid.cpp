#include "globals.h"
#include "script/userScript.h"
#include "lib/logger.h"
#include "systems/context.h"
#include "systems/fonts.h"
#include "scene/components/model.h"
#include "../p64/assetTable.h"

namespace
{
  constexpr float FADE_TIME_MAX = 2.0f;
  constexpr float TEXT_FADE_TIME = 1.0f / 0.35f;

  constexpr float ROT_SPEED = 0.6f;
  constexpr float SCALE_AMPLITUDE = 0.08f;
  constexpr float SCALE_SPEED = 4.0f;

  bool canAfford(uint32_t coins)
  {
    return P64::User::ctx.coins >= coins;
  }
}

namespace P64::Script::CD43F65D4883D4A8
{
  P64_DATA(
    [[P64::Name("Coin-Amount")]]
    uint8_t coinAmount;

    uint8_t state;
    uint8_t inRange;
    uint8_t sfxPlayed;
    float fadeTimer;
    float colorTimer;

    float textTimer;
    float baseScale;

    std::unordered_map<uint16_t, fm_vec3_t> *orgScale;
  );

  // The following functions are called by the engine at different points in the object's lifecycle.
  // If you don't need a specific function you can remove it.

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    if(isDelete) {
      delete data->orgScale;
      return;
    }
    data->state = 0;
    data->fadeTimer = -1;
    data->orgScale = nullptr;
    data->inRange = 1;
    data->textTimer = 0;
    data->colorTimer =  P64::Math::rand01() * 4.0f;
    data->baseScale = obj.scale.x;
    data->sfxPlayed = 0;

    if(!obj.hasChildren()) {
      P64::Log::error("Void object has no children!\n");
      obj.remove();
    }
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
    data->colorTimer += deltaTime;
    auto model = obj.getComponent<Comp::Model>();

    auto coll = obj.getComponent<Comp::CollBody>(1);
    coll->bcs.maskWrite = canAfford(data->coinAmount) ? 0b00 : 0b10;

    model->material.colorPrim = User::getRainbowColor(data->colorTimer * 1.0f);

    // rotate the object
    constexpr fm_vec3_t rotAxis = {0.0f, 0.5f, 0.2f};
    fm_quat_rotate(&obj.rot, &obj.rot, &rotAxis, deltaTime * ROT_SPEED);
    fm_quat_norm(&obj.rot, &obj.rot);

    // sin wave scaling
    float scaleFactor = 1.0f + (sinf(data->colorTimer * SCALE_SPEED) * SCALE_AMPLITUDE);
    scaleFactor *= data->baseScale;
    scaleFactor = fmaxf(scaleFactor, 0.001f);
    obj.scale = fm_vec3_t{scaleFactor, scaleFactor, scaleFactor};

    if(data->state == 0)
    {
      data->orgScale = new std::unordered_map<uint16_t, fm_vec3_t>{};
      obj.iterChildren([data](Object* child) {
        (*data->orgScale)[child->id] = child->scale;
        child->setEnabled(false);
        child->iterChildren([](Object* subChild) { subChild->setEnabled(false); });
      });
      data->state = 1;
    }

    if(data->inRange > 0)--data->inRange;

    if(data->fadeTimer >= 0.0f)
    {
      data->fadeTimer += deltaTime;
      data->fadeTimer = fminf(FADE_TIME_MAX, data->fadeTimer);
      float fadeRel = data->fadeTimer / FADE_TIME_MAX;

      data->baseScale *= 0.9f;
      obj.iterChildren([data, fadeRel](Object* child) {
          child->scale = (*data->orgScale)[child->id] * fadeRel;
      });

      if(!data->sfxPlayed && data->fadeTimer >= 0.8f) {
        auto sfx = AudioManager::play2D("sfx/BoxBreak.wav64"_asset);
        sfx.setSpeed(0.8f);
        sfx.setVolume(0.25f);
        data->sfxPlayed = 1;
      }

      if(data->fadeTimer >= FADE_TIME_MAX) {
        obj.remove();
      }
      return;
    }

    if(data->inRange)
    {
      data->textTimer = fminf(data->textTimer + deltaTime * TEXT_FADE_TIME, 1.0f);

      auto pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
      if(!canAfford(data->coinAmount))return;
      //if(!canAfford(data->coinAmount) || !pressed.b)return;

      if(data->fadeTimer >= 0.0f)return;

      obj.iterChildren([](Object* child) {
        child->scale = {0.01f, 0.01f, 0.01f};
        child->setEnabled(true);
        child->iterChildren([](Object* subChild) { subChild->setEnabled(true); });
      });
      data->fadeTimer = 0.0f;
      User::ctx.coins -= data->coinAmount;
    } else
    {
      data->textTimer = fmaxf(data->textTimer - deltaTime * TEXT_FADE_TIME, 0.0f);
    }
  }

  void onEvent(Object& obj, Data *data, const ObjectEvent &event)
  {
  }

  void onCollision(Object& obj, Data *data, const Coll::CollEvent& event)
  {
    data->inRange = 2;
  }

  void draw(Object& obj, Data *data, float deltaTime)
  {
    if(data->textTimer <= 0.0f || data->fadeTimer >= 0)return;

    fm_vec3_t screenPos{};
    t3d_viewport_calc_viewspace_pos(nullptr, &screenPos, &obj.pos);

    if (screenPos.z > 1.0f) return;

    DrawLayer::use2D();

      uint8_t alpha = data->textTimer * 255;
      if(canAfford(data->coinAmount))
      {
        rdpq_set_prim_color({0xCC, 0xFF, 0xCC, alpha});
      } else {
        rdpq_set_prim_color({0xFF, 0x55, 0x55, alpha});
      }

      rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
      User::Fonts::useNumber();
      User::Fonts::printNumber(screenPos.x - 6, screenPos.y - 6, data->coinAmount);

    DrawLayer::useDefault();
  }
}
