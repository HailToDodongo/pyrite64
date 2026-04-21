/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene/scene.h"

#include <libdragon.h>
#include <rspq_profile.h>
#include <t3d/t3d.h>

#include "scene/scene.h"

#include <malloc.h>

#include "scene/globalState.h"
#include "collision/mesh_collider.h"
#include "vi/swapChain.h"
#include "lib/memory.h"
#include "lib/logger.h"
#include "lib/matrixManager.h"
#include "assets/assetManager.h"
#include "audio/audioManager.h"
#include "../audio/audioManagerPrivate.h"
#include "../debug/overlay.h"
#include "debug/debugMenu.h"

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
  constexpr uint32_t MAX_PHYSICS_STEPS = 5;
  constexpr float SEC_TO_USEC = 1000000.0f;

  P64::Object* collisionEventSelfObject(const P64::Coll::CollEvent &event)
  {
    if(event.selfCollider) return event.selfCollider->ownerObject();
    if(event.selfMeshCollider) return event.selfMeshCollider->ownerObject();
    return nullptr;
  }

  void dispatchObjectCollisionEvent(P64::Object &obj, const P64::Coll::CollEvent &event)
  {
    auto compRefs = obj.getCompRefs();
    for(uint32_t i = 0; i < obj.compCount; ++i)
    {
      const auto &compDef = P64::COMP_TABLE[compRefs[i].type];
      if(!compDef.onColl) continue;

      char *dataPtr = (char *)&obj + compRefs[i].offset;
      compDef.onColl(obj, dataPtr, event);
    }
  }
#if RSPQ_PROFILE
  uint32_t frameCount = 0;
#endif
}

P64::Scene::Scene(uint16_t sceneId, Scene** ref)
  : id{sceneId}
{
  if(ref)*ref = this;
  Debug::init();
  Debug::Overlay::init();

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

  auto *collisionScene = Coll::collisionSceneGetInstance();
  collisionScene->reset();
  collisionScene->configureSimulation(
    conf.physicsTickRate > 0 ? (1.0f / static_cast<float>(conf.physicsTickRate)) : Coll::DEFAULT_FIXED_DT,
    conf.gravity,
    conf.velocitySolverIterations,
    conf.positionSolverIterations,
    conf.physicsScale
  );
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
  accumulator_ticks += TICKS_FROM_US((uint32_t)(deltaTime * 1000000.0f));
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
  AudioManager::ticksUpdate = 0;

  AudioManager::update();

  lighting.reset();

  camMain = cameras.empty() ? nullptr : cameras[0];
  //debugf("cam %p: %d | %f\n", camMain, cameras.size(), (double)camMain->pos.z);

  ticksGlobalUpdate = get_user_ticks();
  GlobalScript::callHooks(GlobalScript::HookType::SCENE_UPDATE);
  ticksGlobalUpdate = get_user_ticks() - ticksGlobalUpdate;

  for(auto data : objectsToAdd) {
    loadObject((uint8_t*&)data.prefabData, [&](Object &obj)
    {
      obj.id = data.objectId;
      obj.pos = data.pos;
      obj.scale = data.scale;
      obj.rot = data.rot;
      obj.flags = ObjectFlags::ACTIVE;
    }, true);
  }

  runPendingComponentInit();
  objectsToAdd.clear();

  // transition active/inactive state of objects
  //auto t = get_ticks();
  if(needsObjStateUpdate)
  {
    for(const auto obj : objects) {
      updateChildObjectStates(nullptr, *obj);
    }
    needsObjStateUpdate = false;
  }
  //t = get_ticks() - t;
  //debugf("State Change Time: %llu us\n", TICKS_TO_US(t));

  runPendingEvents();

  // ======== Run the Physics and fixed Update Callbacks in a fixed Deltatime Loop ======== //
  uint16_t physicsTickRate = conf.physicsTickRate > 0 ? conf.physicsTickRate : 50;
  float fixedDeltaTime = 1.0f / static_cast<float>(physicsTickRate);
  uint32_t fixedDeltaTimeTicks = TICKS_FROM_US((uint32_t)(fixedDeltaTime * SEC_TO_USEC));
  // Safety Clamp
  if (accumulator_ticks > fixedDeltaTimeTicks * MAX_PHYSICS_STEPS)
  {
    accumulator_ticks = fixedDeltaTimeTicks * MAX_PHYSICS_STEPS;
  }
  while (accumulator_ticks >= fixedDeltaTimeTicks)
  {
    for(auto obj : objects)
    {
      if(!obj->isEnabled()) continue;

      auto compRefs = obj->getCompRefs();
      for(uint32_t i = 0; i < obj->compCount; ++i) {
        const auto &compDef = COMP_TABLE[compRefs[i].type];
        if(!compDef.fixedUpdate) continue;

        char* dataPtr = (char*)obj + compRefs[i].offset;
        compDef.fixedUpdate(*obj, dataPtr, fixedDeltaTime);
      }
    }

    Coll::collisionSceneGetInstance()->step();
    accumulator_ticks -= fixedDeltaTimeTicks;
  }

  // Extrapolate rigid body transforms for visual smoothness
  if(conf.interpolatePhysicsTransforms){
    float remainderSec = static_cast<float>(accumulator_ticks) / static_cast<float>(TICKS_FROM_US(SEC_TO_USEC));
    applyRenderInterpolation(remainderSec);
  }

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

  for(auto &obj : pendingObjDelete)
  {
    if(obj->id < idLookup.size()) {
      idLookup[obj->id] = nullptr;
    }
    std::erase_if(savedTransforms_, [&](const SavedTransform &st) { return st.obj == obj; });
    std::erase(objects, obj);
    obj->~Object();

    memObjects -= malloc_usable_size(obj);
    free(obj);
  }
  pendingObjDelete.clear();

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

  restoreInterpolatedTransforms();

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

void P64::Scene::runPendingEvents()
{
  // events, switch now to prevent infinite loops for objects that push events in response to events
  auto &evQueue = eventQueue[eventQueueIdx];
  eventQueueIdx = (eventQueueIdx + 1) % 2;
  for(const auto &entry : evQueue.events)
  {
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
}

void P64::Scene::applyRenderInterpolation(float dt)
{
  auto &rigidBodies = Coll::collisionSceneGetInstance()->getRigidBodies();
  savedTransforms_.clear();

  for(auto *body : rigidBodies) {
    if(!body || body->isSleeping() || body->isKinematic()) continue;

    Object *obj = body->ownerObject();
    if(!obj) continue;

    savedTransforms_.push_back({obj, obj->pos, obj->rot});

    // Extrapolate position forward by remaining time
    const fm_vec3_t &vel = body->linearVelocity();
    obj->pos = obj->pos + vel * dt;

    // Extrapolate rotation forward by remaining time
    const fm_vec3_t &angVel = body->angularVelocity();
    if(!Coll::vec3IsZero(angVel)) {
      obj->rot = Coll::quatApplyAngularVelocity(obj->rot, angVel, dt);
    }
  }
}

void P64::Scene::restoreInterpolatedTransforms()
{
  for(auto &saved : savedTransforms_) {
    saved.obj->pos = saved.pos;
    saved.obj->rot = saved.rot;
  }
  savedTransforms_.clear();
}

void P64::Scene::onObjectCollision(const Coll::CollEvent &event)
{
  auto *selfObject = collisionEventSelfObject(event);
  auto *otherObject = event.otherObject;
  if(!selfObject || !otherObject) return;
  if(!selfObject->isEnabled() || !otherObject->isEnabled()) return;

  dispatchObjectCollisionEvent(*selfObject, event);
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

void P64::Scene::updateChildObjectStates(const Object* parent, Object& obj)
{
  if(!parent && obj.group) {
    parent = getObjectById(obj.group);
  }

  const auto wasEnabledBefore = obj.isEnabled();
  obj.setFlag(ObjectFlags::PARENTS_ACTIVE, parent ? parent->isEnabled() : true);
  if(!obj.performStateChange() && !parent) {
    return; // without a parent, no cascading change can happen if the own state didn't change
  }

  if(wasEnabledBefore == obj.isEnabled()) {
    return;
  }

  sendEvent(obj.id, 0, obj.isEnabled() ? EVENT_TYPE_ENABLE : EVENT_TYPE_DISABLE, 0);
  iterObjectChildren(obj.id, [&](Object* child) {
    updateChildObjectStates(&obj, *child);
  });
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