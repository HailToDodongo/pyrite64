/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene/scene.h"

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

#include "renderer/pipeline.h"
#include "renderer/pipelineHDRBloom.h"
#include "renderer/pipelineBigTex.h"

#include "debug/debugDraw.h"
#include "renderer/drawLayer.h"
#include "scene/componentTable.h"
#include "script/globalScript.h"

namespace
{
  bool collDebugDraw = false;
}

P64::Scene::Scene(uint16_t sceneId, Scene** ref)
  : id{sceneId}
{
  if(ref)*ref = this;
  Debug::init();

  loadSceneConfig();

  DrawLayer::init(conf.layerSetup);

  switch(conf.pipeline)
  {
    case SceneConf::Pipeline::DEFAULT    : renderPipeline = new RenderPipelineDefault(*this);  break;
    case SceneConf::Pipeline::HDR_BLOOM  : renderPipeline = new RenderPipelineHDRBloom(*this); break;
    case SceneConf::Pipeline::BIG_TEX_256: renderPipeline = new RenderPipelineBigTex(*this);   break;
    default: assertf(false, "Unknown render pipeline %d", (int)conf.pipeline);
  }


  state.screenSize[0] = conf.screenWidth;
  state.screenSize[1] = conf.screenHeight;

  renderPipeline->init();

  VI::SwapChain::start();

  loadScene();

  Log::info("Scene %d Loaded", getId());
}

P64::Scene::~Scene()
{
  rspq_wait();

  for(auto obj : objects) {
    obj->~Object();
    free(obj);
  }

  AudioManager::stopAll();
  MatrixManager::reset();
  AssetManager::freeAll();
  Debug::destroy();

  delete renderPipeline;
}

void P64::Scene::update(float deltaTime)
{
  joypad_poll();
  if(joypad_get_buttons_pressed(JOYPAD_PORT_1).l) {
    collDebugDraw = !collDebugDraw;
  }

  AudioManager::update();

  lighting.reset();

  camMain = cameras.empty() ? nullptr : cameras[0];
  //debugf("cam %p: %d | %f\n", camMain, cameras.size(), (double)camMain->pos.z);

  collScene.update(deltaTime);

  GlobalScript::callHooks(GlobalScript::HookType::SCENE_UPDATE);

  for(auto obj : objects)
  {
    if(!obj->isEnabled())continue;

    auto compRefs = obj->getCompRefs();

    for (uint32_t i=0; i<obj->compCount; ++i) {
      const auto &compDef = COMP_TABLE[compRefs[i].type];
      char* dataPtr = (char*)obj + compRefs[i].offset;
      compDef.update(*obj, dataPtr, deltaTime);
    }
  }

  for(auto &cam : cameras) {
    cam->update(deltaTime);
  }

  for(auto &obj : pendingObjDelete)
  {
    std::erase(objects, obj);
    obj->~Object();
    free(obj);
  }
  pendingObjDelete.clear();

  // events, switch now to prevent infinite loops for objects that push events in response to events
  auto &evQueue = eventQueue[eventQueueIdx];
  eventQueueIdx = (eventQueueIdx + 1) % 2;
  for(uint32_t e=0; e<evQueue.eventCount; ++e)
  {
    auto &entry = evQueue.events[e];
    auto obj = getObjectById(entry.targetId);
    if(obj)
    {
      auto compRefs = obj->getCompRefs();
      for (uint32_t i=0; i<obj->compCount; ++i) {
        const auto &compDef = COMP_TABLE[compRefs[i].type];
        if(compDef.onEvent)
        {
          char* dataPtr = (char*)obj + compRefs[i].offset;
          compDef.onEvent(*obj, dataPtr, entry.event);
        }
      }
    }
  }
  evQueue.clear();

  AudioManager::update();

  VI::SwapChain::nextFrame();
}

void P64::Scene::draw([[maybe_unused]] float deltaTime)
{
  renderPipeline->preDraw();
  DrawLayer::draw(0);

  // 3D Pass, for every active camera
  for(auto &cam : cameras)
  {
    camMain = cam;
    cam->attach();

    lighting.apply();
    t3d_matrix_push_pos(1);

    for(int i=1; i<conf.layerSetup.layerCount3D; ++i) {
      DrawLayer::use3D(i);
        t3d_matrix_push_pos(1);
      DrawLayer::useDefault();
    }

    GlobalScript::callHooks(GlobalScript::HookType::SCENE_PRE_DRAW_3D);

    //debugf("Drawing objects:\n");
    for(auto obj : objects)
    {
      //debugf(" - %d\n", obj->id);
      if(!obj->isEnabled())continue;
      auto compRefs = obj->getCompRefs();

      for (uint32_t i=0; i<obj->compCount; ++i) {
        const auto &compDef = COMP_TABLE[compRefs[i].type];
        if(compDef.draw)
        {
          char* dataPtr = (char*)obj + compRefs[i].offset;
          compDef.draw(*obj, dataPtr, deltaTime);
        }
      }
    }

    GlobalScript::callHooks(GlobalScript::HookType::SCENE_POST_DRAW_3D);

    t3d_matrix_pop(1);
    for(int i=1; i<conf.layerSetup.layerCount3D; ++i) {
      DrawLayer::use3D(i);
        t3d_matrix_pop(1);
      DrawLayer::useDefault();
    }
  }

  DrawLayer::use(conf.layerSetup.layerCount3D + conf.layerSetup.layerCountPtx);
    Debug::printStart();
    Debug::printf(16, 16, "%.2f\n", (double)VI::SwapChain::getFPS());

    heap_stats_t heap{};
    sys_get_heap_stats(&heap);
    Debug::printf(16, 16+9, "%.4f\n", heap.used / 1024.0);

    GlobalScript::callHooks(GlobalScript::HookType::SCENE_DRAW_2D);
  DrawLayer::useDefault();

  renderPipeline->draw();
  collScene.debugDraw(collDebugDraw, collDebugDraw);
}

void P64::Scene::onObjectCollision(const Coll::CollEvent &event)
{
  auto objA = getObjectById(event.self->objectId);
  auto objB = getObjectById(event.other->objectId);
  if(!objA || !objB)return;

  //debugf("Collision: %d <> %d\n", event.self->objectId, event.other->objectId);

  auto compRefsA = objA->getCompRefs();
  for (uint32_t i=0; i<objA->compCount; ++i)
  {
    const auto &compDef = COMP_TABLE[compRefsA[i].type];
    if(compDef.onColl) {
      char* dataPtr = (char*)objA + compRefsA[i].offset;
      compDef.onColl(*objA, dataPtr, event);
    }
  }

  Coll::CollEvent eventOther{
    .self = event.other,
    .other = event.self,
  };

  auto compRefsB = objB->getCompRefs();
  for (uint32_t i=0; i<objB->compCount; ++i)
  {
    const auto &compDef = COMP_TABLE[compRefsB[i].type];
    if(compDef.onColl) {
      char* dataPtr = (char*)objB + compRefsB[i].offset;
      compDef.onColl(*objB, dataPtr, eventOther);
    }
  }
}

void P64::Scene::removeObject(Object &obj)
{
  pendingObjDelete.push_back(&obj);
}

P64::Object* P64::Scene::getObjectById(uint16_t objId) const
{
  // @TODO: optimize!
  for(auto obj : objects) {
    if (objId == obj->id) {
      return obj;
    }
  }
  return nullptr;
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
