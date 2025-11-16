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
#include "assets/assetManager.h"
#include "audio/audioManager.h"
#include "../audio/audioManagerPrivate.h"
#include "debug/debugDraw.h"
#include "scene/componentTable.h"
#include "script/scriptTable.h"

namespace
{
  sprite_t *sprite{nullptr};
}

P64::Scene::Scene(uint16_t sceneId, Scene** ref)
  : id{sceneId}
{
  if(ref)*ref = this;
  Debug::init();
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
    Debug::draw(static_cast<uint16_t*>(surf->buffer));
    rdpq_detach_cb((void(*)(void*))((void*)done), (void*)fbIndex);
  });

  VI::SwapChain::start();

  Log::info("Scene %d Loaded", getId());
}

P64::Scene::~Scene()
{
  rspq_wait();
  sprite_free(sprite);

  if (dplObjects)rspq_block_free(dplObjects);

  if(objStaticMats)free(objStaticMats);
  free(stringTable);

  for(auto &fb : surfFbColor) {
    surface_free(&fb);
  }

  for(auto obj : objects) {
    obj->~Object();
    free(obj);
  }

  AudioManager::stopAll();
  MatrixManager::reset();
  AssetManager::freeAll();
  Debug::destroy();
}

void P64::Scene::update(float deltaTime) {
  joypad_poll();
  AudioManager::update();

  lighting.reset();

  camMain = cameras[0];

  collScene.update(deltaTime);

  for(auto obj : objects)
  {
    if(!obj->isEnabled())continue;

    auto compRefs = obj->getCompRefs();

    for (uint32_t i=0; i<obj->compCount; ++i) {
      const auto &compDef = COMP_TABLE[compRefs[i].type];
      char* dataPtr = (char*)obj + compRefs[i].offset;
      compDef.update(*obj, dataPtr);
    }
  }

  for(auto &cam : cameras) {
    cam->update(deltaTime);
  }

  for(auto &obj : pendingObjDelete)
  {
    auto compRefs = obj->getCompRefs();
    for (uint32_t i=0; i<obj->compCount; ++i) {
      const auto &compDef = COMP_TABLE[compRefs[i].type];
      char* dataPtr = (char*)obj + compRefs[i].offset;
      compDef.initDel(*obj, dataPtr, nullptr);
    }
    std::erase(objects, obj);
    obj->~Object();
    free(obj);
  }
  pendingObjDelete.clear();

  AudioManager::update();

  VI::SwapChain::nextFrame();
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

  // 3D Pass, for every active camera
  for(auto &cam : cameras)
  {
    camMain = cam;
    cam->attach();

    lighting.apply();

    t3d_matrix_push_pos(1);
    //rspq_block_run(dplObjects);

    for(auto obj : objects)
    {
      if(!obj->isEnabled())continue;
      auto compRefs = obj->getCompRefs();

      for (uint32_t i=0; i<obj->compCount; ++i) {
        const auto &compDef = COMP_TABLE[compRefs[i].type];
        char* dataPtr = (char*)obj + compRefs[i].offset;
        compDef.draw(*obj, dataPtr);
      }
    }

    t3d_matrix_pop(1);
  }

  // 2D Pass, once for screen

  rdpq_sync_pipe();
  rdpq_set_mode_standard();

  rdpq_sync_load();
  rdpq_sync_tile();
  rdpq_sprite_blit(sprite, 16, 16, nullptr);

  Debug::printStart();
  Debug::printf(16, 16, "FPS: %.2f\n", (double)VI::SwapChain::getFPS());
}

void P64::Scene::removeObject(Object &obj)
{
  pendingObjDelete.push_back(&obj);
}

void P64::Scene::setGroupEnabled(uint16_t groupId, bool enabled) const
{
  if(groupId == 0)return;

  for(auto obj : objects)
  {
    if (groupId == obj->id) {
      obj->setFlag(ObjectFlags::SELF_ACTIVE, enabled);
    }
    if (groupId == obj->group) {
      //debugf("-> obj %d active = %d\n", obj->id, enabled);
      obj->setFlag(ObjectFlags::PARENTS_ACTIVE, enabled);
    }
  }
}
