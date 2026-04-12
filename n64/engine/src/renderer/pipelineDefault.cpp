/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "../debug/overlay.h"
#include "renderer/pipeline.h"
#include "debug/debugDraw.h"
#include "lib/memory.h"
#include "renderer/drawLayer.h"
#include "scene/globalState.h"
#include "scene/scene.h"
#include "vi/swapChain.h"

void P64::RenderPipeline::setupLayer()
{
  rdpq_mode_begin();
    rdpq_set_mode_standard();
    rdpq_mode_antialias(AA_NONE);
    rdpq_mode_zbuf(true, true);
    rdpq_mode_persp(true);
    rdpq_mode_filter(FILTER_BILINEAR);
    rdpq_mode_dithering(DITHER_NONE_NONE);
    rdpq_mode_blender(0);
    rdpq_mode_fog(0);
  rdpq_mode_end();
}

void P64::RenderPipelineDefault::init()
{
  tex_format_t fmt = (scene.getConf().flags & SceneConf::FLAG_SCR_32BIT) ? FMT_RGBA32 : FMT_RGBA16;
  for(auto &fb : surfFbColor) {
    fb = surface_alloc(fmt, state.screenSize[0], state.screenSize[1]);
  }

  VI::SwapChain::setFrameBuffers(surfFbColor);

  VI::SwapChain::setDrawPass([this](surface_t *surf, uint32_t fbIndex, auto done) {
    surfColor = surf;
    surfDepth = &Mem::allocDepthBuffer(state.screenSize[0], state.screenSize[1]);

    rdpq_attach(surf, surfDepth);
    scene.draw(VI::SwapChain::getDeltaTime());

    Debug::Overlay::draw(surf);
    rdpq_detach_cb((void(*)(void*))((void*)done), (void*)fbIndex);
  });
}

P64::RenderPipelineDefault::~RenderPipelineDefault()
{
  for(auto &fb : surfFbColor) {
    if(fb.buffer)surface_free(&fb);
  }
  Mem::freeDepthBuffer();
}

void P64::RenderPipelineDefault::preDraw()
{
  setupLayer();

  if(scene.getConf().flags & SceneConf::FLAG_CLR_DEPTH) {
    t3d_screen_clear_depth();
  }
  if(scene.getConf().flags & SceneConf::FLAG_CLR_COLOR) {
    t3d_screen_clear_color(scene.getConf().clearColor);
  }
}

void P64::RenderPipelineDefault::draw()
{
  DrawLayer::draw3D();
  DrawLayer::drawPtx();
  DrawLayer::draw2D();

  DrawLayer::nextFrame();
}
