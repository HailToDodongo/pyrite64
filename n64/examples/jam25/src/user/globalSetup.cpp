#include "script/globalScript.h"
#include "script/userScript.h"

#include <libdragon.h>
#include <vi/swapChain.h>

#include "globals.h"
#include "systems/context.h"
#include "systems/dialog.h"
#include "systems/dropShadows.h"
#include "systems/fonts.h"
#include "systems/marker.h"
#include "systems/screenFade.h"
#include "systems/sprites.h"
#include "script/nodeGraph.h"

namespace P64::User
{
  constinit Context ctx{};

  color_t getRainbowColor(float s) {
    float r = fm_sinf(s + 0.0f) * 127.0f + 128.0f;
    float g = fm_sinf(s + 2.0f) * 127.0f + 128.0f;
    float b = fm_sinf(s + 4.0f) * 127.0f + 128.0f;
    return color_t{(uint8_t)r, (uint8_t)g, (uint8_t)b, 255};
  }
}

namespace
{
  constexpr uint16_t baseDepth = 0xFF0E;
  constexpr uint16_t ditherScale = 0x250 / 2;
  constexpr uint16_t ditherScaleSteps = 9;

  constexpr uint32_t ditherDim = 16;
  __aligned(16) uint16_t buffDither[ditherDim*ditherDim] = {
  };
  surface_t depthDitherTex;
}

namespace P64::GlobalScript::C4F4D286D6CBAAAA
{
  void onGameInit()
  {
    User::ctx = {};
    User::ctx.controlledId = 1;
    User::Marker::init();

    NodeGraph::registerFunction("ShowMessage"_crc32, User::Dialog::showMessage);

    NodeGraph::registerFunction("FadeIn"_crc32, [](uint32_t val) {
      User::ScreenFade::fadeIn(0, (float)val * (1.0f / 1000.0f));
      return 1;
    });

    NodeGraph::registerFunction("FadeOut"_crc32, [](uint32_t val) {
      User::ScreenFade::fadeOut(0, (float)val * (1.0f / 1000.0f));
      return 1;
    });

    NodeGraph::registerFunction("SetBars"_crc32, [](uint32_t val) {
      User::ctx.forceBars = val != 0;
      return 1;
    });

    // generate bayer matrix
    for(uint32_t y = 0; y < ditherDim; ++y)
    {
      for(uint32_t x = 0; x < ditherDim; ++x)
      {
        uint32_t index = 0;
        for(uint32_t bit = 0; bit < 4; ++bit)
        {
          index |= ((x >> (3 - bit)) & 1) << (bit * 2 + 1);
          index |= ((y >> (3 - bit)) & 1) << (bit * 2 + 0);
        }
        index = 255 - index;
        uint16_t ditherVal = (ditherScale * index) / ditherScaleSteps;
        buffDither[y*ditherDim + x] = baseDepth - ditherVal;
      }
    }

    data_cache_hit_writeback(buffDither, sizeof(buffDither));
    depthDitherTex = surface_make(buffDither, FMT_RGBA16, ditherDim, ditherDim, ditherDim*2);

    User::ScreenFade::setFadeState(true);
  }

  void onScenePreLoad()
  {
    User::DropShadows::init();
    User::Sprites::init();
    User::Fonts::init();
    User::Dialog::init();
    User::ctx.forceBars = false;
  }

  void onScenePostLoad()
  {
    User::ScreenFade::fadeIn(0, 1.4f);
  }

  void onScenePostUnload()
  {
    User::DropShadows::destroy();
    User::Sprites::destroy();
    User::Fonts::destroy();
    User::Dialog::destroy();
  }

  void onSceneUpdate()
  {
    /*auto held = joypad_get_buttons_held(JOYPAD_PORT_1);
    for(uint32_t y = 0; y < ditherDim; ++y)
    {
      for(uint32_t x = 0; x < ditherDim; ++x)
      {
        uint32_t index = 0;
        for(uint32_t bit = 0; bit < 4; ++bit)
        {
          index |= ((x >> (3 - bit)) & 1) << (bit * 2 + 1);
          index |= ((y >> (3 - bit)) & 1) << (bit * 2 + 0);
        }
        uint16_t ditherVal = (ditherScale * index) / ditherScaleSteps;
        buffDither[y*ditherDim + x] = baseDepth - ditherVal;
      }
    }*/

    User::ctx.isCutscene = false;
    ++User::ctx.frame;

    User::Marker::nextFrame();
    User::DropShadows::reset();
    User::Sprites::reset();

    if(User::Dialog::update())User::ctx.isCutscene = true;
  }

  void onScenePreDraw()
  {
    auto pipeline = SceneManager::getCurrent().getRenderPipeline<RenderPipeline>();
    rdpq_set_mode_standard();
    rdpq_set_color_image(pipeline->getCurrDepthSurf());
    rdpq_set_mode_copy(false);

    rdpq_texparms_t p{};
    p.s.repeats = REPEAT_INFINITE;
    p.t.repeats = REPEAT_INFINITE;
    rdpq_tex_upload(TILE0, &depthDitherTex, &p);
    rdpq_texture_rectangle(TILE0, 0, 0, display_get_width(), display_get_height(), 0, 0);
    rdpq_set_color_image(pipeline->getCurrColorSurf());
  }

  void onScenePostDraw3D()
  {
    DrawLayer::use3D(1);
      User::DropShadows::draw();
    DrawLayer::useDefault();

    DrawLayer::usePtx();
      User::Sprites::draw();
    DrawLayer::useDefault();
  }


  constexpr color_t col[3]
  {
    {0xFF, 0, 0, 0xFF},
    {0, 0xFF, 0, 0xFF},
    {0, 0, 0xFF, 0xFF},
  };
  uint32_t colIdx = 0;

  void onSceneDraw2D()
  {
    //User::Marker::draw();
    User::Dialog::draw();
    User::ScreenFade::draw();

    // frame pacing debug
/*
    rdpq_sync_pipe();
    rdpq_set_mode_fill(col[colIdx]);
    rdpq_fill_rectangle(32, 32+(colIdx*4), 32+8, 32+8+(colIdx*4));
    colIdx = (colIdx + 1) % 3;
    */

  }
}