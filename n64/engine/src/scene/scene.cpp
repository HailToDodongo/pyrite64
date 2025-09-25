/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include <libdragon.h>
#include <t3d/t3d.h>

#include "scene/scene.h"
#include "scene/globalState.h"
#include "vi/swapChain.h"
#include "lib/memory.h"
#include "lib/logger.h"
#include "lib/matrixManager.h"
#include "lib/assetManager.h"
#include "actor/actorTable.h"
#include "audio/audioManager.h"
#include "../audio/audioManagerPrivate.h"

namespace
{
  sprite_t *sprite{nullptr};
}

P64::Scene::Scene(uint16_t sceneId)
  : actors{}, id{sceneId}
{
  loadScene();

  state.screenSize[0] = conf.screenWidth;
  state.screenSize[1] = conf.screenHeight;

  sprite = sprite_load("rom:/unit1m.i8.sprite");

  tex_format_t fmt = (conf.flags & SceneConf::FLAG_SCR_32BIT) ? FMT_RGBA32 : FMT_RGBA16;
  for(auto &fb : surfFbColor) {
    fb = surface_alloc(fmt, state.screenSize[0], state.screenSize[1]);
  }

  VI::SwapChain::setFrameBuffers(surfFbColor);
  VI::SwapChain::setDrawPass([this](surface_t *surf, uint32_t fbIndex, auto done) {
    rdpq_attach(surf, &P64::Mem::allocDepthBuffer(P64::state.screenSize[0], P64::state.screenSize[1]));
    draw(1.0f / 60.0f);
    rdpq_detach_cb((void(*)(void*))((void*)done), (void*)fbIndex);
  });

  VI::SwapChain::start();

  Log::info("Scene %d Loaded", getId());
}

P64::Scene::~Scene()
{
  rspq_wait();
  sprite_free(sprite);
  rspq_block_free(dplObjects);

  if(objStaticMats)free(objStaticMats);
  free(stringTable);

  for(auto &fb : surfFbColor) {
    surface_free(&fb);
  }

  for(auto actor : actors) {
    delete actor;
  }

  AudioManager::stopAll();
  MatrixManager::reset();
  AssetManager::reset();
}

void P64::Scene::addActor(uint16_t type, P64::Actor::Base::Args *args) {
  auto newActor = Actor::createActor(type, args);
  if(newActor) {
    actors.push_back(newActor);
  }
}

void P64::Scene::update(float deltaTime) {
  joypad_poll();
  AudioManager::update();

  for(auto &cam : cameras) {
    cam.update(deltaTime);
  }

  for(auto actor : actors) {
    actor->update(deltaTime);
  }

  AudioManager::update();

  P64::VI::SwapChain::nextFrame();
}

void P64::Scene::draw(float deltaTime)
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

  if(conf.flags & SceneConf::FLAG_CLR_DEPTH) {
    t3d_screen_clear_depth();
  }
  if(conf.flags & SceneConf::FLAG_CLR_COLOR) {
    t3d_screen_clear_color(conf.clearColor);
  }

  t3d_light_set_ambient({0xFF, 0xFF, 0xFF, 0xFF});

  // 3D Pass, for every active camera
  for(auto &cam : cameras)
  {
    camMain = &cam;
    cam.attach();

    t3d_matrix_push_pos(1);
    rspq_block_run(dplObjects);

    for(auto actor : actors) {
      actor->draw(deltaTime);
    }

    t3d_matrix_pop(1);
  }

  // 2D Pass, once for screen

  rdpq_sync_pipe();
  rdpq_set_mode_standard();

  rdpq_sync_load();
  rdpq_sync_tile();
  rdpq_sprite_blit(sprite, 16, 16, nullptr);
}
