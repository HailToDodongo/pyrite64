/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene/scene.h"

#include <libdragon.h>
#include <rspq_profile.h>
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
#include "../debug/overlay.h"

#include "renderer/pipeline.h"
#include "renderer/pipelineHDRBloom.h"
#include "renderer/pipelineBigTex.h"

#include "debug/debugDraw.h"
#include "renderer/drawLayer.h"
#include "scene/componentTable.h"
#include "script/globalScript.h"

namespace
{
  uint16_t nextId = 0xFF;
#if RSPQ_PROFILE
  uint32_t frameCount = 0;
#endif
}

P64::Scene::Scene(uint16_t sceneId, Scene** ref)
  : id{sceneId}
{
  if(ref)*ref = this;
  Debug::init();

  loadSceneConfig();
  P64::AudioManager::init(conf.audioFreq);

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

  switch(conf.filter)
  {
    case FILTERS_DISABLED: default:
      vi_set_dedither(false);
      vi_set_aa_mode(VI_AA_MODE_NONE);
    break;
    case FILTERS_RESAMPLE:
      vi_set_dedither(false);
      vi_set_aa_mode(VI_AA_MODE_RESAMPLE);
    break;
    case FILTERS_DEDITHER:
      vi_set_dedither(true);
      vi_set_aa_mode(VI_AA_MODE_NONE);
    break;
    case FILTERS_RESAMPLE_ANTIALIAS:
      vi_set_dedither(false);
      vi_set_aa_mode(VI_AA_MODE_RESAMPLE_FETCH_ALWAYS);
    break;
    case FILTERS_RESAMPLE_ANTIALIAS_DEDITHER:
      vi_set_dedither(true);
      vi_set_aa_mode(VI_AA_MODE_RESAMPLE_FETCH_ALWAYS);
    break;
  }

  VI::SwapChain::setFrameSkip(conf.frameSkip);
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
  auto pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
  auto held = joypad_get_buttons_held(JOYPAD_PORT_1);
  if(held.l && pressed.d_up) {
    Debug::Overlay::toggle();
  }

  // reset metrics
  ticksActorUpdate = 0;
  ticksDraw = 0;
  ticksGlobalDraw = 0;
  collScene.ticks = 0;
  collScene.ticksBVH = 0;
  collScene.raycastCount = 0;
  AudioManager::ticksUpdate = 0;

  AudioManager::update();

  lighting.reset();

  camMain = cameras.empty() ? nullptr : cameras[0];
  //debugf("cam %p: %d | %f\n", camMain, cameras.size(), (double)camMain->pos.z);

  for(auto data : objectsToAdd) {
    loadObject((uint8_t*&)data.prefabData, [&](Object &obj)
    {
      obj.id = data.objectId;
      obj.pos = data.pos;
      obj.scale = data.scale;
      obj.rot = data.rot;
      obj.flags = ObjectFlags::ACTIVE;
    });
  }
  objectsToAdd.clear();

  ticksGlobalUpdate = get_user_ticks();
  GlobalScript::callHooks(GlobalScript::HookType::SCENE_UPDATE);
  ticksGlobalUpdate = get_user_ticks() - ticksGlobalUpdate;

  ticksActorUpdate = get_ticks();
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

  ticksActorUpdate = get_ticks() - ticksActorUpdate;
  collScene.update(deltaTime);

  for(auto &obj : pendingObjDelete)
  {
    idLookup[obj->id] = nullptr;
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
  ticksDraw = get_ticks();

  GlobalScript::callHooks(GlobalScript::HookType::SCENE_PRE_DRAW);
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
        cam->reApplyScissor();
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

      for (uint32_t i=0; i<obj->compCount; ++i)
      {
        if(obj->flags & ObjectFlags::IS_CULLED)break;
        const auto &compDef = COMP_TABLE[compRefs[i].type];
        if(compDef.draw)
        {
          char* dataPtr = (char*)obj + compRefs[i].offset;
          compDef.draw(*obj, dataPtr, deltaTime);
        }
      }

      // culling resets directly after a draw, otherwise objects can get stuck culled.
      // this is also needed to handle multiple cameras correctly.
      obj->setFlag(ObjectFlags::IS_CULLED, false);
    }

    auto t = get_user_ticks();
    GlobalScript::callHooks(GlobalScript::HookType::SCENE_POST_DRAW_3D);
    ticksGlobalDraw += get_user_ticks() - t;

    t3d_matrix_pop(1);
    for(int i=1; i<conf.layerSetup.layerCount3D; ++i) {
      DrawLayer::use3D(i);
        t3d_matrix_pop(1);
      DrawLayer::useDefault();
    }
  }

  auto t = get_user_ticks();
  DrawLayer::use2D();
    GlobalScript::callHooks(GlobalScript::HookType::SCENE_DRAW_2D);
  DrawLayer::useDefault();
  ticksGlobalDraw += get_user_ticks() - t;

  renderPipeline->draw();
  ticksDraw = get_ticks() - ticksDraw;

#if RSPQ_PROFILE
  rspq_profile_next_frame();
  if(++frameCount == 30) {
    rspq_profile_dump();
    rspq_profile_reset();
    frameCount = 0;
  }
#endif
}

void P64::Scene::onObjectCollision(const Coll::CollEvent &event)
{
  auto objA = event.selfBCS ? event.selfBCS->obj : event.selfMesh->object;
  auto objB = event.otherBCS ? event.otherBCS->obj : event.otherMesh->object;
  if(!objA || !objB)return;

  auto compRefsA = objA->getCompRefs();
  for (uint32_t i=0; i<objA->compCount; ++i)
  {
    const auto &compDef = COMP_TABLE[compRefsA[i].type];
    if(compDef.onColl) {
      char* dataPtr = (char*)objA + compRefsA[i].offset;
      compDef.onColl(*objA, dataPtr, event);
    }
  }

  //if(!event.otherBCS)return;

  Coll::CollEvent eventOther{
    .selfBCS = event.otherBCS,
    .otherBCS = event.selfBCS,
    .selfMesh = event.otherMesh,
    .otherMesh = event.selfMesh,
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

uint16_t P64::Scene::addObject(
  uint32_t prefabIdx,
  const fm_vec3_t &pos,
  const fm_vec3_t &scale,
  const fm_quat_t &rot
) {
  auto *prefabData = AssetManager::getByIndex(prefabIdx);
  objectsToAdd.push_back({
    .prefabData = prefabData,
    .pos = pos,
    .scale = scale,
    .rot = rot,
    .objectId = ++nextId,
  });
  return nextId;
}

void P64::Scene::removeObject(Object &obj)
{
  pendingObjDelete.push_back(&obj);
}

P64::Object* P64::Scene::getObjectById(uint16_t objId) const
{
  // the first IDs get a direct lookup, under the assumption most
  // scenes keep object count in a reasonable amount
  if(objId < idLookup.size()) {
    return idLookup[objId];
  }

  // otherwise fallback to a linear scan
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

P64::Lighting & P64::Scene::startLightingOverride(bool copyExisting)
{
  lightingTemp = copyExisting ? lighting : Lighting{};
  return lightingTemp;
}

void P64::Scene::endLightingOverride()
{
  lighting.apply();
}