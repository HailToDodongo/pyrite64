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
#include "renderer/renderScale.h"
#include "scene/scene.h"

namespace
{
  struct Layer
  {
    // A queue can only be run once per frame (its end marker gets overwritten by further recording),
    // so every run (e.g. one per camera) gets its own queue, created on demand and reused across frames.
    std::vector<rspq_queue_t*> queues{};
    uint32_t runIdx{0}; // queue currently recorded into
    bool used{false};   // commands were recorded since the last run
  };

  constexpr uint32_t LAYER_BUFFER_COUNT = 3;
  std::vector<std::array<Layer, LAYER_BUFFER_COUNT>> layers{};

  constinit P64::DrawLayer::Setup *layerSetup{};
  constinit uint8_t frameIdx{0};
  constinit uint8_t currLayerIdx{0};
  constinit uint8_t lastLightMode{T3D_LIGHTING_MODE_MUL};

  Layer& layerAt(uint32_t idx)
  {
    assertf(idx > 0 && idx <= layers.size(), "Invalid layer index %lu", idx);
    return layers[idx-1][frameIdx];
  }
}

void P64::DrawLayer::init(Setup &setup)
{
  reset();
  layerSetup = &setup;
  uint32_t layerCount = setup.layerCount3D + setup.layerCountPtx + setup.layerCount2D;
  assert(setup.layerCount3D > 0 && layerCount <= 16);

  currLayerIdx = 0;
  frameIdx = 0;
  layers.resize(layerCount-1);
  Log::info("DrawLayer count: %d", layers.size());
}

void P64::DrawLayer::use(uint32_t idx)
{
  if(idx == currLayerIdx)return;
  currLayerIdx = idx;

  if(idx == 0) {
    rspq_queue_switch(nullptr);
    return;
  }

  auto &layer = layerAt(idx);
  if(layer.runIdx == layer.queues.size())layer.queues.push_back(rspq_queue_create());
  layer.used = true;
  rspq_queue_switch(layer.queues[layer.runIdx]);
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
  if(layerIdx != 0 && !layerAt(layerIdx).used)return;

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
    // fog distances are in meters, view space is in render units
    t3d_fog_set_range(setup.fogMin * Renderer::getRenderScale(), setup.fogMax * Renderer::getRenderScale());

    if(setup.fogMode == Conf::FogMode::CLEAR_COLOR) {
      rdpq_set_fog_color(SceneManager::getCurrent().getConf().clearColor);
    } else if(setup.fogMode == Conf::FogMode::CUSTOM_COLOR) {
      rdpq_set_fog_color(setup.fogColor);
    }
  } else {
    t3d_fog_set_enabled(false);
  }

  if(layerIdx == 0)return;
  auto &layer = layerAt(layerIdx);
  auto queue = layer.queues[layer.runIdx++];
  layer.used = false;

  // 3D layers set model matrices relative to the current stack position, give them their own slot
  if(layerIdx < layerSetup->layerCount3D) {
    t3d_matrix_push_pos(1);
      rspq_queue_run(queue);
      t3d_tri_sync();
    t3d_matrix_pop(1);
  } else {
    rspq_queue_run(queue);
  }
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
  bool hasParticles = false;
  for(int i=0; i<layerSetup->layerCountPtx; ++i) {
    hasParticles |= layerAt(idxStart + i).used;
  }
  if(!hasParticles)return;

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
    auto &l = layer[frameIdx];
    for(auto queue : l.queues)rspq_queue_clear(queue);
    l.runIdx = 0;
    l.used = false;
  }
}

void P64::DrawLayer::reset()
{
  rspq_queue_switch(nullptr);
  for(auto &layer : layers)
  {
    for(auto &l : layer) {
      for(auto queue : l.queues)rspq_queue_destroy(queue);
    }
  }
  layers.clear();

  layerSetup = nullptr;
  currLayerIdx = 0;
}
