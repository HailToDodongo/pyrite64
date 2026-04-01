/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "renderer/drawLayer.h"
#include "../libdragon/rspq.h"
#include <array>
#include <vector>
#include <t3d/t3d.h>
#include <t3d/tpx.h>

#include "lib/logger.h"
#include "scene/scene.h"

#define LIBDRAGON_LAYERS 1

namespace
{
  struct Layer
  {
    #ifdef LIBDRAGON_LAYERS
      rspq_queue_t *queue{};
    #else
      volatile uint32_t *pointer{};
      volatile uint32_t *current{};
      volatile uint32_t *sentinel{};
    #endif
  };

  constexpr uint32_t LAYER_BUFFER_COUNT = 3;
  std::vector<std::array<Layer, LAYER_BUFFER_COUNT>> layers{};

  constinit P64::DrawLayer::Setup *layerSetup{};

  #ifndef LIBDRAGON_LAYERS
    constexpr uint32_t LAYER_BUFFER_WORDS = 1024;
    constexpr uint32_t LAYER_BUFFER_WORDS_2D = 1024*2;
    constinit volatile uint32_t* layerMem{nullptr};
  #endif

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

  #ifdef LIBDRAGON_LAYERS
    Log::info("DrawLayer count: %d", layers.size());
    for(auto &layer : layers)
    {
      for(auto &l : layer) {
        l.queue = rspq_queue_create();
      }
    }
  #else
    uint32_t countAlloc3D = setup.layerCount3D-1 + setup.layerCountPtx;
    size_t allocSize = countAlloc3D * LAYER_BUFFER_WORDS;
    allocSize += setup.layerCount2D * LAYER_BUFFER_WORDS_2D;
    allocSize *= (LAYER_BUFFER_COUNT * sizeof(uint32_t));

    Log::info("DrawLayer mem-size: %d bytes", allocSize);
    layerMem = (volatile uint32_t*)malloc_uncached(allocSize);
    auto mem = layerMem;

    uint32_t layerIdx = 0;
    for(auto &layer : layers)
    {
      bool is2D = layerIdx >= countAlloc3D;
      layer.fill({});
      for(uint32_t i = 0; i < LAYER_BUFFER_COUNT; i++)
      {
        layer[i].pointer = mem;
        layer[i].current = mem;
        mem += is2D ? LAYER_BUFFER_WORDS_2D : LAYER_BUFFER_WORDS;
        layer[i].sentinel = mem;
      }
      ++layerIdx;
    }
  #endif
}

void P64::DrawLayer::use(uint32_t idx)
{
  if(idx == currLayerIdx)return;

  #ifdef LIBDRAGON_LAYERS
    rspq_queue_switch(idx == 0 ? nullptr : layers[idx-1][frameIdx].queue);
  #else
    if(idx == 0)
    {
      layers[currLayerIdx-1][frameIdx].current = LD::RSPQ::redirectEnd();
    } else {
      LD::RSPQ::redirectStart(
        layers[idx-1][frameIdx].current,
        layers[idx-1][frameIdx].sentinel
      );
    }
  #endif

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
    rdpq_mode_fog((setup.fogMode != Conf::FogMode::NONE) ? RDPQ_FOG_STANDARD : 0);
  rdpq_mode_end();

  if(setup.fogMode != Conf::FogMode::NONE)
  {
    t3d_fog_set_enabled(true);
    t3d_fog_set_range(setup.fogMin, setup.fogMax);

    if(setup.fogMode == Conf::FogMode::CLEAR_COLOR) {
      rdpq_set_fog_color(SceneManager::getCurrent().getConf().clearColor);
    } else if(setup.fogMode == Conf::FogMode::CUSTOM_COLOR) {
      rdpq_set_fog_color(setup.fogColor);
    }
  } else {
    t3d_fog_set_enabled(false);
  }


  if(layerIdx == 0)return;
  assertf(layerIdx-1 < layers.size(), "Invalid layer index %lu", layerIdx);

  auto &layer = layers[layerIdx-1];

  #ifdef LIBDRAGON_LAYERS
    //debugf("Draw-Layer: %lu, frame: %lu\n", layerIdx-1, frameIdx);
    //rspq_queue_debug(layer[frameIdx].queue);
    rspq_queue_run(layer[frameIdx].queue);
    //rspq_wait();
  #else
    //uint32_t sizeWord = layer[frameIdx].current - layer[frameIdx].pointer;
    //debugf("Usage Layer %lu: %lu/%d words\n", layerIdx-1, sizeWord, layer[frameIdx].sentinel - layer[frameIdx].pointer);
    LD::RSPQ::exec(layer[frameIdx].pointer, layer[frameIdx].current);
  #endif
}

void P64::DrawLayer::draw3D()
{
  for(int i=1; i<layerSetup->layerCount3D; ++i) {
    draw(i);
  }
}

void P64::DrawLayer::drawPtx()
{
  rdpq_set_mode_standard();

  rdpq_mode_begin();
    rdpq_mode_zbuf(true, true);
    rdpq_mode_zoverride(true, 0, 0);
    rdpq_mode_combiner(RDPQ_COMBINER1((PRIM,0,ENV,0), (PRIM,0,ENV,0)));
    rdpq_mode_blender(0);
  rdpq_mode_end();
  rdpq_set_env_color({0xFF, 0xFF, 0xFF, 0xFF});

  tpx_state_from_t3d();
  tpx_state_set_scale(1.0f, 1.0f);
  tpx_state_set_base_size(128);

  int idxStart = layerSetup->layerCount3D;
  for(int i=0; i<layerSetup->layerCountPtx; ++i) {
    draw(idxStart + i);
  }
}

void P64::DrawLayer::draw2D()
{
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

  #ifdef LIBDRAGON_LAYERS
    for(auto &layer : layers) {
      rspq_queue_clear(layer[frameIdx].queue);
    }
  #else
    for(auto &layer : layers) {
      layer[frameIdx].current = layer[frameIdx].pointer;
    }
  #endif
}

void P64::DrawLayer::reset()
{
  #ifdef LIBDRAGON_LAYERS
    for(auto &layer : layers)
    {
      for(auto &l : layer) {
        rspq_queue_destroy(l.queue);
        l.queue = nullptr;
      }
    }
  #else
    if(layerMem)free_uncached((void*)layerMem);
    layerMem = nullptr;
  #endif

  layerSetup = nullptr;
  currLayerIdx = 0;
}
