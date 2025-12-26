/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "renderer/pipelineBigTex.h"

#include "bigtex/bigtex.h"
#include "bigtex/memory.h"
#include "debug/debugDraw.h"
#include "renderer/drawLayer.h"
#include "scene/scene.h"
#include "vi/swapChain.h"

namespace BigTex = P64::Renderer::BigTex;

extern "C" {
  // see: ./bigtex/applyTexture.S
  extern uint32_t BigTex_applyTexture(uint32_t fbTexIn, uint32_t fbTexInEnd, uint32_t fbOut64);
}

namespace
{
  constexpr int SCREEN_WIDTH = 320;
  constexpr int SCREEN_HEIGHT = 240;
  constexpr int SHADE_BLEND_SLICES = 16;

  constinit BigTex::FrameBuffers fbs{};
  constinit uint32_t frameIdx{0};
  constinit surface_t *currSurf{};
}

void P64::RenderPipelineBigTex::init()
{
  assertf(scene.getConf().screenWidth == SCREEN_WIDTH, "Ucode can only handle 320x240 resolution");
  assertf(scene.getConf().screenHeight == SCREEN_HEIGHT, "Ucode can only handle 320x240 resolution");
  assertf(!(scene.getConf().flags & SceneConf::FLAG_SCR_32BIT), "Ucode can only handle RGBA16 output");

  BigTex::ucodeInit();
  fbs = BigTex::allocBuffers();

  /*
  heap_stats_t h;
  sys_get_heap_stats(&h);
  debugf("BigTex Heap - used: %d bytes, free: %d bytes\n", h.used, h.total - h.used);
*/
  VI::SwapChain::setFrameBuffers(fbs.color);

  VI::SwapChain::setDrawPass([this](surface_t *surf, uint32_t fbIndex, auto done) {
    currSurf = surf;
    rdpq_attach(surf, BigTex::getZBuffer());
    scene.draw(VI::SwapChain::getDeltaTime());
    Debug::draw(static_cast<uint16_t*>(surf->buffer));
    rdpq_detach_cb((void(*)(void*))((void*)done), (void*)fbIndex);
  });
}

P64::RenderPipelineBigTex::~RenderPipelineBigTex()
{
  BigTex::freeBuffers(fbs);
  BigTex::ucodeDestroy();
}

void P64::RenderPipelineBigTex::preDraw()
{
  setupLayer();
  surface_t *surfDepth = BigTex::getZBuffer();

  if(scene.getConf().flags & SceneConf::FLAG_CLR_COLOR) {
    assertf(false, "Clearing screen not supported in BigTex pipeline");
  }

  if(scene.getConf().flags & SceneConf::FLAG_CLR_DEPTH) {
    rdpq_set_color_image(surfDepth);
    rdpq_mode_push();
      rdpq_set_mode_fill(color_from_packed16(0xFFFE));
      rdpq_fill_rectangle(0,0, SCREEN_WIDTH, SCREEN_HEIGHT);
    rdpq_mode_pop();
  }

  uvTex.use();

  rdpq_set_color_image(&fbs.uv[frameIdx]);
  rdpq_set_z_image(surfDepth);
}

void P64::RenderPipelineBigTex::draw()
{
  uint32_t frameIdxLast = (frameIdx + 2) % 3;
  //DrawLayer::draw(DrawLayer::LAYER_TRANS);

  rdpq_sync_pipe();
  rdpq_mode_zbuf(false, false);

  rdpq_sync_tile();
  rdpq_sync_load();
  rdpq_set_color_image(currSurf);
  rdpq_set_mode_standard();

  rdpq_set_lookup_address(1, fbs.shade[frameIdxLast].buffer);
  rdpq_set_lookup_address(2, currSurf->buffer);

  rspq_flush();

  uint64_t ticks = get_ticks();
  uint32_t FB_SIZE_IN = SCREEN_WIDTH * SCREEN_HEIGHT * 4;
  auto *texIn = (uint64_t*)CachedAddr(fbs.uv[frameIdxLast].buffer);

  switch(drawMode)
  {
    case DRAW_MODE_DEF: default:
    {
      uint32_t quarterSlice = FB_SIZE_IN / SHADE_BLEND_SLICES / 3;
      uint32_t stepSizeTexIn = quarterSlice * 2;
      uint32_t stepSizeTexInRSP = quarterSlice * 1;

      uint32_t ptrInPos = (uint32_t)(texIn);
      uint32_t ptrOutPos = (uint32_t) CachedAddr(currSurf->buffer);

      // version without shading, this is the same as above without the RDP tasks inbetween
      stepSizeTexIn =  FB_SIZE_IN / SHADE_BLEND_SLICES;
      for(int p=0; p<SHADE_BLEND_SLICES; ++p)
      {
        if(p % 4 == 0) {
          BigTex::ucodeFillTextures(
            ptrInPos, ptrInPos + stepSizeTexIn, ptrOutPos
          );
          rspq_flush();
        } else {
          BigTex_applyTexture(ptrInPos, ptrInPos + stepSizeTexIn, ptrOutPos);
          data_cache_hit_writeback_invalidate((char*)CachedAddr(ptrOutPos + stepSizeTexIn/2) - 0x1000, 0x1000);
        }

        ptrOutPos += stepSizeTexIn / 2;
        ptrInPos += stepSizeTexIn;
      }
    }
    break;
    case DRAW_MODE_UV: BigTex::applyTexturesUV(texIn, (uint16_t*)currSurf->buffer, FB_SIZE_IN); break;
    case DRAW_MODE_MAT: BigTex::applyTexturesMat(texIn, (uint16_t*)currSurf->buffer, FB_SIZE_IN); break;
  }

  ticks = get_ticks() - ticks;
  //debugf("Time: %lldus\n", TICKS_TO_US(ticks));

  setupLayer();
  DrawLayer::draw3D(); // @TODO: split to get 3D after PP
  DrawLayer::drawPtx();
  DrawLayer::draw2D();
  DrawLayer::nextFrame();

  frameIdx = (frameIdx + 1) % 3;
}
