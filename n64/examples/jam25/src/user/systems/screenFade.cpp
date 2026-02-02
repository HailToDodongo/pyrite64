/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "screenFade.h"
#include <libdragon.h>
#include <audio/audioManager.h>
#include <lib/types.h>
#include <lib/math.h>
#include <vi/swapChain.h>
#include "../../p64/assetTable.h"

#include "context.h"

namespace
{
  constexpr float BAR_FADE_SPEED = 4.0f;
  constexpr float FADE_VOLUME = 0.25f;

  float fadeTime{0};
  float fadeTimeTarget{0};
  float fadeStep{0};

  float barTimer{0};
  float barTimerTarget{0};

  color_t fadeColor{0, 0, 0, 0};

  void drawNGonOverlay(int sides, const fm_vec3_t &pos, float radiusInner, float radiusOuter, float angle, color_t color)
  {
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
    rdpq_set_prim_color(color);

    fm_vec3_t lastInner, lastOuter;
    float angleSection = (360.0_deg / (float)sides);

    for(int i=0; i<=sides; ++i) {
      fm_vec3_t dirVec{fm_cosf(angle), fm_sinf(angle), 0};
      auto inner = pos + dirVec * radiusInner;
      auto outer = pos + dirVec * radiusOuter;

      if(i > 0) {
        rdpq_triangle(&TRIFMT_FILL, lastInner.v, outer.v, inner.v);
        rdpq_triangle(&TRIFMT_FILL, lastInner.v, outer.v, lastOuter.v);
      }

      lastInner = inner;
      lastOuter = outer;
      angle += angleSection;
    }
  }
}

void P64::User::ScreenFade::fadeOut(int type, float time)
{
  if(fadeTimeTarget == 1.0f)return;
  fadeStep = 1.0f / time;
  fadeTimeTarget = 1.0f;

  AudioManager::play2D("sfx/FadeOut.wav64"_asset)
    .setVolume(FADE_VOLUME);

  if(coro_get_current()) {
    while(!isDone())coro_yield();
  }
}

void P64::User::ScreenFade::fadeIn(int type, float time)
{
  if(fadeTimeTarget == 0.0f)return;
  fadeStep = -1.0f / time;
  fadeTimeTarget = 0.0f;

  AudioManager::play2D("sfx/FadeIn.wav64"_asset)
    .setVolume(FADE_VOLUME);

  if(coro_get_current()) {
    while(!isDone())coro_yield();
  }
}

void P64::User::ScreenFade::setFadeState(bool isFadedIn)
{
  fadeTime = isFadedIn ? 0.0f : 1.0f;
  fadeTimeTarget = fadeTime;
  fadeStep = 0.0f;
}

bool P64::User::ScreenFade::isDone()
{
  return fadeTime == fadeTimeTarget;
}

void P64::User::ScreenFade::draw()
{
  AudioManager::setMasterVolume(1.0f - fadeTime);
  float deltaTime = VI::SwapChain::getDeltaTime();
  if(fadeStep != 0.0f)
  {
    fadeTime += fadeStep * deltaTime;
    if(fadeTime < 0.0f || fadeTime > 1.0f)
    {
      fadeTime = fadeTimeTarget;
      fadeStep = 0;
    }
  }
  fadeColor.a = (uint8_t)(255.0f * fadeTime);

  if(ctx.isCutscene || ctx.forceBars) {
    barTimer += deltaTime * BAR_FADE_SPEED;
  } else {
    barTimer -= deltaTime * BAR_FADE_SPEED;
  }
  barTimer = Math::clamp(barTimer, 0.0f, 1.0f);

  VI::SwapChain::setVBlank(fadeColor.a > 230);
  if(fadeColor.a != 0)
  {
    /*rdpq_set_mode_standard();
    rdpq_set_prim_color(fadeColor);
    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
    rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (0,0,0,PRIM)));
    rdpq_fill_rectangle(0, 0, 320, 240);*/

    constexpr float SCREEN_WIDTH = 320.0f;
    constexpr float SCREEN_HEIGHT = 240.0f;
    float s = Math::easeOutSin(fadeTime);
    float angle = 1.75f * ((fadeTime < fadeTimeTarget) ? -s : s);
    drawNGonOverlay(5,
      {SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f, 0},
      (1.0f-s) * 230.0f, 245.0f,
      angle, {0x00, 0x00, 0x00, 0xFF}
    );
  }

  if(barTimer != 0)
  {
    rdpq_set_mode_fill({0, 0, 0, 0xFF});
    rdpq_fill_rectangle(0, 0, 320, barTimer * 16);
    rdpq_fill_rectangle(0, 240 - (barTimer * 16), 320, 240);
  }
}
