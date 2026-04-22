/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "renderer/drawLayer.h"
#include <array>
#include <vector>
#include <t3d/t3d.h>
#include <t3d/tpx.h>

#include "lib/logger.h"
#include "scene/scene.h"

namespace
{
  struct Layer
  {
    rspq_queue_t *queue{};
  };

  constexpr uint32_t LAYER_BUFFER_COUNT = 3;
  std::vector<std::array<Layer, LAYER_BUFFER_COUNT>> layers{};

  constinit P64::DrawLayer::Setup *layerSetup{};
  constinit uint8_t frameIdx{0};
  constinit uint8_t currLayerIdx{0};
  constinit uint8_t lastLightMode{T3D_LIGHTING_MODE_MUL};
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

  Log::info("DrawLayer count: %d", layers.size());
  for(auto &layer : layers)
  {
    for(auto &l : layer) {
      l.queue = rspq_queue_create();
    }
  }
}

void P64::DrawLayer::use(uint32_t idx)
{
  if(idx == currLayerIdx)return;

  rspq_queue_switch(idx == 0 ? nullptr : layers[idx-1][frameIdx].queue);
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

  if(setup.lightMode != lastLightMode) {
    t3d_state_set_lighting_mode((T3DLightingMode)setup.lightMode);
    lastLightMode = setup.lightMode;
  }

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

  //debugf("Draw-Layer: %lu, frame: %lu\n", layerIdx-1, frameIdx);
  //rspq_queue_debug(layer[frameIdx].queue);
  rspq_queue_run(layer[frameIdx].queue);
  //rspq_wait();
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

  for(auto &layer : layers) {
    rspq_queue_clear(layer[frameIdx].queue);
  }
}

void P64::DrawLayer::reset()
{
  for(auto &layer : layers)
  {
    for(auto &l : layer) {
      rspq_queue_destroy(l.queue);
      l.queue = nullptr;
    }
  }

  layerSetup = nullptr;
  currLayerIdx = 0;
}
