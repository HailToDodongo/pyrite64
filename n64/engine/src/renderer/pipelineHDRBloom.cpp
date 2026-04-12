/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "renderer/pipelineHDRBloom.h"
#include "../debug/overlay.h"
#include "hdr/rspHDR.h"
#include "lib/memory.h"
#include "renderer/drawLayer.h"
#include "scene/globalState.h"
#include "scene/scene.h"
#include "vi/swapChain.h"

namespace {
  constexpr int SCREEN_WIDTH = 320;
  constexpr int SCREEN_HEIGHT = 240;

  constexpr bool DEBUG_BLOOM = false;

  P64::Renderer::HDR::Config config{
    .blurSteps = 4,
    .blurBrightness = 1.0f,
    .hdrFactor = 2.0f,
    .bloomThreshold = 0.2f,
    .scalingUseRDP = true,
  };

  surface_t *fb = nullptr;
}

void P64::RenderPipelineHDRBloom::init()
{
  // should be prevented in the editor already:
  assertf(scene.getConf().screenWidth == SCREEN_WIDTH, "Ucode can only handle 320x240 resolution");
  assertf(scene.getConf().screenHeight == SCREEN_HEIGHT, "Ucode can only handle 320x240 resolution");
  assertf(!(scene.getConf().flags & SceneConf::FLAG_SCR_32BIT), "Ucode can only handle RGBA16 output");

  for(auto &fb : surfFbColor) {
    fb = surface_alloc(FMT_RGBA16, SCREEN_WIDTH, SCREEN_HEIGHT);
    Mem::clearSurface(fb);
  }

  RspHDR::init();

  VI::SwapChain::setFrameBuffers(surfFbColor);

  VI::SwapChain::setDrawPass([this](surface_t *surf, uint32_t fbIndex, auto done) {
    surfColor = surf;
    surfDepth = &Mem::allocDepthBuffer(state.screenSize[0], state.screenSize[1]);

    rdpq_attach(surf, surfDepth);
    fb = surf;
    scene.draw(VI::SwapChain::getDeltaTime());
    Debug::Overlay::draw(surf);
    rdpq_detach_cb((void(*)(void*))((void*)done), (void*)fbIndex);
  });
}

P64::RenderPipelineHDRBloom::~RenderPipelineHDRBloom()
{
  for(auto &fb : surfFbColor) {
    if(fb.buffer)surface_free(&fb);
  }

  RspHDR::destroy();
  Mem::freeDepthBuffer();
}

void P64::RenderPipelineHDRBloom::preDraw()
{
  //rdpq_set_color_image(&surfHDRSafe);
  setupLayer();

  postProc[frameIdx].setConf(config);
  postProc[frameIdx].beginFrame();

  if(scene.getConf().flags & SceneConf::FLAG_CLR_DEPTH) {
    t3d_screen_clear_depth();
  }
  if(scene.getConf().flags & SceneConf::FLAG_CLR_COLOR) {
    t3d_screen_clear_color(scene.getConf().clearColor);
  }
}

void P64::RenderPipelineHDRBloom::draw()
{
  uint32_t frameIdxLast = (frameIdx+BUFF_COUNT-1) % BUFF_COUNT;

  DrawLayer::draw3D();
  DrawLayer::drawPtx();

  postProc[frameIdx].endFrame();
  assert(fb != nullptr);
  auto surfBlur = postProc[frameIdxLast].applyEffects(*fb);

  rdpq_set_color_image(fb);

  if(DEBUG_BLOOM)
  {
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER_TEX);
    rdpq_mode_blender(0);
    rdpq_mode_antialias(AA_NONE);
    rdpq_mode_filter(FILTER_POINT);

    rdpq_blitparms_t param{};
    param.scale_x = 4.0f;
    param.scale_y = 4.0f;
    rdpq_tex_blit(&surfBlur, 0, 0, &param);
  }

  DrawLayer::draw2D();
  DrawLayer::nextFrame();

  frameIdx = (frameIdx + 1) % BUFF_COUNT;
}
