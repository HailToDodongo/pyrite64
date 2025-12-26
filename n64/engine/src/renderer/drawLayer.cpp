/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "renderer/drawLayer.h"
#include "../libdragon/rspq.h"
#include <array>
#include <vector>

#include "lib/logger.h"

namespace
{
  struct Layer
  {
    volatile uint32_t *pointer{};
    volatile uint32_t *current{};
    volatile uint32_t *sentinel{};
  };

  constexpr uint32_t LAYER_BUFFER_COUNT = 3;
  constexpr uint32_t LAYER_BUFFER_WORDS = 1024;
  std::vector<std::array<Layer, LAYER_BUFFER_COUNT>> layers{};

  constinit P64::DrawLayer::Setup *layerSetup{};

  constinit volatile uint32_t* layerMem{nullptr};
  constinit uint8_t frameIdx{0};
  constinit uint8_t currLayerIdx{0};
}

void P64::DrawLayer::init(Setup &setup)
{
  reset();
  layerSetup = &setup;
  uint32_t layerCount = setup.layerCount3D + setup.layerCountPtx + setup.layerCount2D;
  assert(layerCount > 0);

  currLayerIdx = 0;
  layers = {};
  layers.resize(layerCount-1);

  size_t allocSize = layers.size() * LAYER_BUFFER_WORDS * LAYER_BUFFER_COUNT * sizeof(uint32_t);
  Log::info("DrawLayer mem-size: %d bytes", allocSize);
  layerMem = (volatile uint32_t*)malloc_uncached(allocSize);
  sys_hw_memset64((void*)layerMem, 0, allocSize);

  auto mem = layerMem;

  for(auto &layer : layers)
  {
    layer.fill({});
    for(uint32_t i = 0; i < LAYER_BUFFER_COUNT; i++)
    {
      layer[i].pointer = mem;
      layer[i].current = mem;
      layer[i].sentinel = mem + LAYER_BUFFER_WORDS;
      mem += LAYER_BUFFER_WORDS;
    }
  }
}

void P64::DrawLayer::use(uint32_t idx)
{
  if(idx == currLayerIdx)return;

  if(idx == 0)
  {
    layers[currLayerIdx-1][frameIdx].current = LD::RSPQ::redirectEnd();
  } else {
    LD::RSPQ::redirectStart(
      layers[idx-1][frameIdx].current,
      layers[idx-1][frameIdx].sentinel
    );
  }

  currLayerIdx = idx;
}

void P64::DrawLayer::usePtx(uint32_t idx)
{
  use(idx + layerSetup->layerCount3D);
}

void P64::DrawLayer::use2D(uint32_t idx)
{
  use(idx + layerSetup->layerCount3D + layerSetup->layerCountPtx);
}

void P64::DrawLayer::draw(uint32_t layerIdx)
{
  auto &setup = layerSetup->layerConf[layerIdx];
  rdpq_mode_begin();
    rdpq_mode_zbuf(
      setup.flags & Conf::FLAG_Z_COMPARE,
      setup.flags & Conf::FLAG_Z_WRITE
    );
    rdpq_mode_blender(setup.blender);
  rdpq_mode_end();

  if(layerIdx == 0)return;
  assertf(layerIdx-1 < layers.size(), "Invalid layer index %lu", layerIdx);

  auto &layer = layers[layerIdx-1];
  debugf("DrawLayer: Drawing layer %d (ptr=%p, curr=%p, size=%ld)\n", layerIdx, layer[frameIdx].pointer, layer[frameIdx].current, layer[frameIdx].current - layer[frameIdx].pointer);
  LD::RSPQ::exec(layer[frameIdx].pointer, layer[frameIdx].current);
}

void P64::DrawLayer::draw3D()
{
  for(int i=1; i<layerSetup->layerCount3D; ++i) {
    draw(i);
  }
}

void P64::DrawLayer::drawPtx()
{
  int idxStart = layerSetup->layerCount3D;
  for(int i=0; i<layerSetup->layerCountPtx; ++i) {
    draw(idxStart + i);
  }
}

void P64::DrawLayer::draw2D()
{
  rdpq_sync_pipe();
  rdpq_sync_load();
  rdpq_sync_tile();
  rdpq_set_mode_standard();

  int idxStart = layerSetup->layerCount3D + layerSetup->layerCountPtx;
  for(int i=0; i<layerSetup->layerCount2D; ++i) {
    draw(idxStart + i);
  }
}

void P64::DrawLayer::nextFrame()
{
  frameIdx = (frameIdx + 1) % LAYER_BUFFER_COUNT;
  currLayerIdx = 0;

  for(auto &layer : layers) {
    layer[frameIdx].current = layer[frameIdx].pointer;

    // @TODO: is this needed?
    sys_hw_memset64((void*)layer[frameIdx].pointer, 0, LAYER_BUFFER_WORDS * sizeof(uint32_t));
  }
}

void P64::DrawLayer::reset()
{
  if(layerMem)free_uncached((void*)layerMem);
  layerMem = nullptr;
  layerSetup = nullptr;
  currLayerIdx = 0;
}
