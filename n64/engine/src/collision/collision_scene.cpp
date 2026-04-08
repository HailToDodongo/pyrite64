/**
 * @file collision_scene.cpp
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Defines the Collision Scene which keeps track of physics participants and updates them (see collision_scene.h)
 */
#include "collision/collision_scene.h"
#include "collision/collide.h"
#include "collision/contact_utils.h"
#include "collision/gjk.h"
#include "scene/scene.h"

#include <cmath>
#include <cassert>
#include <cinttypes>
#include <functional>
#include <algorithm>
#include <limits>
#include <utility>
#include <unordered_set>

#include "debug/debugDraw.h"

namespace P64::Coll {

  static CollisionScene g_scene;

  static bool canApplyAngularResponse(const RigidBody *body) {
    return body && body->canApplyAngularResponse();
  }

  static fm_vec3_t constrainLinearWorld(const RigidBody *body, const fm_vec3_t &worldLinear) {
    return body ? body->constrainLinearWorld(worldLinear) : worldLinear;
  }

  static void applyConstrainedLinearVelocityDelta(RigidBody *body, const fm_vec3_t &deltaLinearVelocity) {
    if(!body) return;
    body->applyConstrainedLinearVelocityDelta(deltaLinearVelocity);
  }

  static void applyConstrainedImpulseAtContact(RigidBody *body, const fm_vec3_t &impulse, const fm_vec3_t &toContact) {
    if(!body) return;
    body->applyConstrainedImpulseAtContact(impulse, toContact);
  }

  static float constrainedLinearInvMassAlong(const RigidBody *body, const fm_vec3_t &direction) {
    return body ? body->constrainedLinearInvMassAlong(direction) : 0.0f;
  }

  static Object *collisionEventSelfObject(const CollEvent &event) {
    if(event.selfCollider) return event.selfCollider->ownerObject();
    if(event.selfMeshCollider) return event.selfMeshCollider->ownerObject();
    return nullptr;
  }

  static bool collisionEventMasksOverlap(const CollEvent &event) {
    if(event.selfCollider) {
      if(event.hitCollider) return event.selfCollider->readsCollider(event.hitCollider);
      if(event.hitMeshCollider) return event.selfCollider->readsMeshCollider(event.hitMeshCollider);
      return false;
    }

    if(event.selfMeshCollider) {
      if(event.hitCollider) return event.selfMeshCollider->readsCollider(event.hitCollider);
    }

    return false;
  }

  static std::pair<const Object *, const Object *> makeObjectPairKey(const Object *objectA, const Object *objectB) {
    if(std::less<const Object *>{}(objectB, objectA)) {
      std::swap(objectA, objectB);
    }
    return {objectA, objectB};
  }

  static CollEvent makeMirroredCollisionEvent(const CollEvent &event) {
    CollEvent mirrored{};
    mirrored.selfCollider = event.hitCollider;
    mirrored.hitCollider = event.selfCollider;
    mirrored.selfMeshCollider = event.hitMeshCollider;
    mirrored.hitMeshCollider = event.selfMeshCollider;
    mirrored.selfRigidBody = event.hitRigidBody;
    mirrored.hitRigidBody = event.selfRigidBody;
    mirrored.contactCount = event.contactCount;
    mirrored.otherObject = collisionEventSelfObject(event);

    for(uint16_t i = 0; i < event.contactCount; ++i) {
      mirrored.contacts[i] = event.contacts[i];
      std::swap(mirrored.contacts[i].contactA, mirrored.contacts[i].contactB);
      std::swap(mirrored.contacts[i].localPointA, mirrored.contacts[i].localPointB);
      std::swap(mirrored.contacts[i].aToContact, mirrored.contacts[i].bToContact);
    }

    return mirrored;
  }

  bool CollisionScene::shouldTrackSleepState(const RigidBody *rigidBody) {
    return rigidBody && !rigidBody->isKinematic_ && rigidBody->position_;
  }

  bool CollisionScene::rigidBodyVelocitiesExceededSleepThreshold(const RigidBody *rigidBody) {
    if(!shouldTrackSleepState(rigidBody)) return false;

    const float speedSq = fm_vec3_len2(&rigidBody->linearVelocity_);
    if(speedSq > SPEED_SLEEP_THRESHOLD_SQ * (g_scene.physicsScale_ * g_scene.physicsScale_)) return true;

    const float angSpeedSq = fm_vec3_len2(&rigidBody->angularVelocity_);
    return angSpeedSq > ANGULAR_SLEEP_THRESHOLD_SQ;
  }

  bool CollisionScene::rigidBodyTransformExceededSleepThreshold(const RigidBody *rigidBody) {
    if(!shouldTrackSleepState(rigidBody)) return false;

    const float posDeltaSq = fm_vec3_distance2(rigidBody->position_, &rigidBody->previousStepPosition_);
    if(posDeltaSq > POS_SLEEP_THRESHOLD_SQ * (g_scene.physicsScale_ * g_scene.physicsScale_)) return true;

    if(rigidBody->rotation_) {
      const float rotSim = fabsf(quatDot(*rigidBody->rotation_, rigidBody->previousStepRotation_));
      if(rotSim < ROT_SIMILARITY_SLEEP_THRESHOLD) return true;
    }

    if(rigidBody->owner_) {
      const float scaleDeltaSq = fm_vec3_distance2(&rigidBody->owner_->scale, &rigidBody->previousStepScale_);
      if(scaleDeltaSq > POS_SLEEP_THRESHOLD_SQ * (g_scene.physicsScale_ * g_scene.physicsScale_)) return true;
    }

    return false;
  }

  bool CollisionScene::rigidBodyCompoundPropertiesNeedUpdate(const RigidBody *rigidBody) {
    if(!rigidBody || !rigidBody->owner_) return false;
    if(rigidBody->compoundPropertiesDirty()) return true;
    return fm_vec3_distance2(&rigidBody->getCompoundScale(), &rigidBody->owner_->scale) > FM_EPSILON * FM_EPSILON;
  }

  void CollisionScene::rebuildCachedConstraintLookup() {
    cachedConstraintLookup_.clear();
    for(int i = 0; i < cachedConstraintCount_; ++i) {
      ContactConstraint &cc = cachedConstraints_[i];
      cachedConstraintLookup_[cc.key] = i;
    }
  }

  CollisionScene *collisionSceneGetInstance() {
    return &g_scene;
  }

  // ── Reset / Init ──────────────────────────────────────────────────

  void CollisionScene::reset() {
    colliderAABBTree.destroy();

    rigidBodies_.clear();
    ownerRigidBodies_.clear();
    colliders_.clear();
    ownerColliders_.clear();
    meshColliders_.clear();
    cachedConstraintCount_ = 0;
    cachedConstraints_.clear();
    cachedConstraintLookup_.clear();
    solverConstraints_.clear();
    ticksWakePrep = 0;
    ticksWorldUpdate = 0;
    ticksIntegrateVel = 0;
    ticksDetect = 0;
    ticksDetectBodyPairs = 0;
    ticksDetectMeshPairs = 0;
    ticksRefreshCallbacks = 0;
    ticksPreSolve = 0;
    ticksWarmStart = 0;
    ticksVelocitySolve = 0;
    ticksIntegration = 0;
    ticksPositionSolve = 0;
    ticksFinalize = 0;
    ticksTotal = 0;

    colliderAABBTree.init(32); // Initial capacity (will grow as needed)
  }

  RigidBody *CollisionScene::findRigidBodyByOwner(const Object *owner) const {
    if(!owner) return nullptr;
    auto it = ownerRigidBodies_.find(owner);
    return (it != ownerRigidBodies_.end()) ? it->second : nullptr;
  }

  const std::vector<Collider *> *CollisionScene::findCollidersForOwner(const Object *owner) const {
    if(!owner) return nullptr;
    auto it = ownerColliders_.find(owner);
    if(it == ownerColliders_.end()) return nullptr;
    return &it->second;
  }

  void CollisionScene::updateCompoundProperties(RigidBody *rigidBody) const {
    if(!rigidBody || !rigidBody->owner_ || !rigidBody->position_) return;

    const fm_vec3_t fallbackInertia = rigidBody->getDefaultLocalInertiaTensor();

    const std::vector<Collider *> *ownerColliders = findCollidersForOwner(rigidBody->owner_);
    if(!ownerColliders || ownerColliders->empty()) {
      rigidBody->applyCompoundProperties(VEC3_ZERO, fallbackInertia, rigidBody->owner_->scale);
      return;
    }

    int count = 0;
    fm_vec3_t worldCenterSum = VEC3_ZERO;
    for(Collider *collider : *ownerColliders) {
      if(!collider) continue;
      worldCenterSum = worldCenterSum + collider->worldCenter_;
      ++count;
    }

    if(count <= 0) {
      rigidBody->applyCompoundProperties(VEC3_ZERO, fallbackInertia, rigidBody->owner_->scale);
      return;
    }

    const float invCount = 1.0f / static_cast<float>(count);
    const fm_vec3_t worldCenter = worldCenterSum * invCount;

    fm_vec3_t localCenterOffset = worldCenter - *rigidBody->position_;
    if(rigidBody->rotation_) {
      localCenterOffset = quatConjugate(*rigidBody->rotation_) * localCenterOffset;
    }

    if(rigidBody->getMass() <= FM_EPSILON) {
      rigidBody->applyCompoundProperties(localCenterOffset, fallbackInertia, rigidBody->owner_->scale);
      return;
    }

    const float massPerCollider = rigidBody->getMass() * invCount;
    fm_vec3_t compoundInertia = VEC3_ZERO;

    for(Collider *collider : *ownerColliders) {
      if(!collider) continue;

      fm_vec3_t colliderInertia = collider->inertiaTensor(massPerCollider);
      fm_vec3_t r = collider->worldCenter_ - worldCenter;
      if(rigidBody->rotation_) {
        r = quatConjugate(*rigidBody->rotation_) * r;
      }

      const float x2 = r.x * r.x;
      const float y2 = r.y * r.y;
      const float z2 = r.z * r.z;

      colliderInertia.x += massPerCollider * (y2 + z2);
      colliderInertia.y += massPerCollider * (x2 + z2);
      colliderInertia.z += massPerCollider * (x2 + y2);

      compoundInertia = compoundInertia + colliderInertia;
    }

    rigidBody->applyCompoundProperties(localCenterOffset, compoundInertia, rigidBody->owner_->scale);
  }

  void CollisionScene::syncCompoundProperties(RigidBody *rigidBody) const {
    if(!rigidBody || !rigidBody->owner_) return;
    if(!rigidBodyCompoundPropertiesNeedUpdate(rigidBody)) return;

    updateCompoundProperties(rigidBody);
  }

  // ── Object management ─────────────────────────────────────────────

  void CollisionScene::addRigidBody(RigidBody *rigidBody) {
    if(!rigidBody || !rigidBody->owner_) return;
    rigidBodies_.push_back(rigidBody);
    ownerRigidBodies_[rigidBody->owner_] = rigidBody;
    

    rigidBody->markCompoundPropertiesDirty();
    syncCompoundProperties(rigidBody);
    const fm_vec3_t worldPos = rigidBody->position_ ? *rigidBody->position_ : VEC3_ZERO;
    rigidBody->worldAabb_ = AABB{worldPos, worldPos};
  }

  void CollisionScene::removeRigidBody(RigidBody *rigidBody) {
    if(!rigidBody) return;

    std::vector<RigidBody *> wakeCandidates;

    if(rigidBody->owner_) {
      auto ownerIt = ownerRigidBodies_.find(rigidBody->owner_);
      if(ownerIt != ownerRigidBodies_.end() && ownerIt->second == rigidBody) {
        ownerRigidBodies_.erase(ownerIt);
      }
    }

    removeCachedConstraints([rigidBody](const ContactConstraint &cc) {
      return cc.rigidBodyA == rigidBody || cc.rigidBodyB == rigidBody;
    }, wakeCandidates, rigidBody);

    // Sleeping rigidBodies overlapping the removed body are likely support-dependent and should re-evaluate.
    const AABB removedBounds = rigidBody->worldAabb_;
    
    for(RigidBody *body : rigidBodies_) {
      if(!body || body == rigidBody) continue;
      if(aabbOverlap(body->worldAabb_, removedBounds)) {
        addWakeCandidate(wakeCandidates, body, rigidBody);
      }
    }

    wakeCandidateIslands(wakeCandidates);

    rigidBodies_.erase(std::remove(rigidBodies_.begin(), rigidBodies_.end(), rigidBody), rigidBodies_.end());
  }

  RigidBody *CollisionScene::findRigidBodyByObjectId(uint16_t id) const
  {
    for (RigidBody *body : rigidBodies_)
    {
      if (body && body->owner_ && body->owner_->id == id)
      {
        return body;
      }
    }
    return nullptr;
  }

  void CollisionScene::addCollider(Collider *collider) {
    if(!collider || !collider->owner_) return;
    colliders_.push_back(collider);
    ownerColliders_[collider->owner_].push_back(collider);
    collider->syncWorldState();

    RigidBody *rigidBody = findRigidBodyByOwner(collider->owner_);
    if (rigidBody)
    {
      rigidBody->markCompoundPropertiesDirty();
      syncCompoundProperties(rigidBody);
    }
    collider->aabbTreeNodeId_ = colliderAABBTree.createNode(collider->worldAabb_, collider);
  }

  void CollisionScene::removeCollider(Collider *collider) {
    if(!collider) return;
    Object *owner = collider->owner_;

    std::vector<RigidBody *> wakeCandidates;
    removeCachedConstraints([collider](const ContactConstraint &cc) {
      return cc.colliderA == collider || cc.colliderB == collider;
    }, wakeCandidates);

    wakeCandidateIslands(wakeCandidates);

    if(owner) {
      auto it = ownerColliders_.find(owner);
      if(it != ownerColliders_.end()) {
        std::vector<Collider *> &ownerList = it->second;
        ownerList.erase(std::remove(ownerList.begin(), ownerList.end(), collider), ownerList.end());
        if(ownerList.empty()) {
          ownerColliders_.erase(it);
        }
      }
    }

    colliders_.erase(std::remove(colliders_.begin(), colliders_.end(), collider), colliders_.end());

    if(collider->aabbTreeNodeId_ != NULL_NODE) {
      colliderAABBTree.removeLeaf(collider->aabbTreeNodeId_, true);
      collider->aabbTreeNodeId_ = NULL_NODE;
    }

    if(owner) {
      RigidBody *rigidBody = findRigidBodyByOwner(owner);
      if(rigidBody) {
        rigidBody->markCompoundPropertiesDirty();
        syncCompoundProperties(rigidBody);
      }
    }
  }

  void CollisionScene::addMeshCollider(MeshCollider *mesh) {
    if(!mesh) return;

    mesh->computeLocalRootAabb();
    mesh->recalculateWorldAabb();
    mesh->syncOwnerTransform();

    meshColliders_.push_back(mesh);
  }

  void CollisionScene::removeMeshCollider(MeshCollider *mesh) {
    if(!mesh) return;

    std::vector<RigidBody *> wakeCandidates;
    removeCachedConstraints([mesh](const ContactConstraint &cc) {
      return cc.meshColliderA == mesh || cc.meshColliderB == mesh;
    }, wakeCandidates);

    wakeCandidateIslands(wakeCandidates);

    for(std::size_t i = 0; i < meshColliders_.size(); ++i) {
      if(meshColliders_[i] == mesh) {
        meshColliders_[i] = meshColliders_.back();
        meshColliders_.pop_back();
        break;
      }
    }
  }

  void CollisionScene::configureSimulation(float fixedDt, const fm_vec3_t &gravity, uint8_t velocityIterations, uint8_t positionIterations, float physicsScale) {
    fixedDt_ = fixedDt > 0.0f ? fixedDt : DEFAULT_FIXED_DT;
    physicsScale_ = physicsScale > FM_EPSILON ? physicsScale : DEFAULT_PHYSICS_SCALE;
    gravity_ = gravity * physicsScale;
    velocitySolverIterations_ = std::max<uint8_t>(1, velocityIterations);
    positionSolverIterations_ = std::max<uint8_t>(1, positionIterations);
  }

  void CollisionScene::wakeRigidBodyIsland(RigidBody *rigidBody) {
    wakeIsland(rigidBody);
  }

  int CollisionScene::getCachedConstraintCount() const {
    return cachedConstraintCount_;
  }

  ContactConstraint &CollisionScene::getCachedConstraint(int index) {
    assert(index >= 0 && index < cachedConstraintCount_);
    return cachedConstraints_[index];
  }

  const ContactConstraint &CollisionScene::getCachedConstraint(int index) const {
    assert(index >= 0 && index < cachedConstraintCount_);
    return cachedConstraints_[index];
  }

  ContactConstraint *CollisionScene::createCachedConstraint(
    const ContactConstraintKey &key,
    RigidBody *rigidBodyA, Collider *colliderA, MeshCollider *meshColliderA, Object *objectA,
    RigidBody *rigidBodyB, Collider *colliderB, MeshCollider *meshColliderB, Object *objectB) {
    if(ContactConstraint *existing = findCachedConstraint(key)) {
      return existing;
    }

    cachedConstraints_.push_back(ContactConstraint{});
    cachedConstraintCount_ = static_cast<int>(cachedConstraints_.size());
    ContactConstraint &cc = cachedConstraints_.back();
    cc.key = key;
    cc.rigidBodyA = rigidBodyA;
    cc.colliderA = colliderA;
    cc.meshColliderA = meshColliderA;
    cc.objectA = objectA;
    cc.rigidBodyB = rigidBodyB;
    cc.colliderB = colliderB;
    cc.meshColliderB = meshColliderB;
    cc.objectB = objectB;

    cachedConstraintLookup_[key] = cachedConstraintCount_ - 1;
    return &cc;
  }

  ContactConstraint *CollisionScene::findCachedConstraint(const ContactConstraintKey &key) {
    auto it = cachedConstraintLookup_.find(key);
    if(it == cachedConstraintLookup_.end()) return nullptr;
    return &cachedConstraints_[it->second];
  }

  void CollisionScene::collectConnectedIsland(RigidBody *seed, std::vector<RigidBody *> &island, std::unordered_set<RigidBody *> &visited) const {
    if(!shouldTrackSleepState(seed)) return;

    std::vector<RigidBody *> stack;
    stack.push_back(seed);

    while(!stack.empty()) {
      RigidBody *current = stack.back();
      stack.pop_back();

      if(!shouldTrackSleepState(current)) continue;
      if(visited.find(current) != visited.end()) continue;
      visited.insert(current);
      island.push_back(current);

      for(int i = 0; i < cachedConstraintCount_; ++i) {
        const ContactConstraint &cc = cachedConstraints_[i];
        if(!cc.isActive || cc.isTrigger) continue;

        RigidBody *other = nullptr;
        if(cc.rigidBodyA == current) {
          other = cc.rigidBodyB;
        } else if(cc.rigidBodyB == current) {
          other = cc.rigidBodyA;
        }

        if(!shouldTrackSleepState(other)) continue;
        if(visited.find(other) == visited.end()) {
          stack.push_back(other);
        }
      }
    }
  }

  void CollisionScene::addWakeCandidate(std::vector<RigidBody *> &wakeCandidates, RigidBody *candidate, RigidBody *ignoredCandidate) {
    if(!candidate || candidate == ignoredCandidate) return;
    if(std::find(wakeCandidates.begin(), wakeCandidates.end(), candidate) == wakeCandidates.end()) {
      wakeCandidates.push_back(candidate);
    }
  }

  void CollisionScene::wakeCandidateIslands(const std::vector<RigidBody *> &wakeCandidates) {
    for(RigidBody *candidate : wakeCandidates) {
      wakeIsland(candidate);
    }
  }

  void CollisionScene::removeCachedConstraints(
    const std::function<bool(const ContactConstraint &)> &shouldRemove,
    std::vector<RigidBody *> &wakeCandidates,
    RigidBody *ignoredCandidate) {
    for(int i = 0; i < cachedConstraintCount_;) {
      ContactConstraint &cc = cachedConstraints_[i];
      if(shouldRemove(cc)) {
        addWakeCandidate(wakeCandidates, cc.rigidBodyA, ignoredCandidate);
        addWakeCandidate(wakeCandidates, cc.rigidBodyB, ignoredCandidate);

        removeCachedConstraintAt(i);
      } else {
        ++i;
      }
    }
  }

  void CollisionScene::removeCachedConstraintAt(int index) {
    assert(index >= 0 && index < cachedConstraintCount_);

    const int lastIndex = cachedConstraintCount_ - 1;
    cachedConstraintLookup_.erase(cachedConstraints_[index].key);

    if(index != lastIndex) {
      cachedConstraints_[index] = cachedConstraints_[lastIndex];
      cachedConstraintLookup_[cachedConstraints_[index].key] = index;
    }

    cachedConstraints_.pop_back();
    cachedConstraintCount_ = static_cast<int>(cachedConstraints_.size());
  }

  CollEvent CollisionScene::makeCollisionEvent(const ContactConstraint &constraint) const {
    CollEvent event{};
    event.selfCollider = constraint.colliderA;
    event.hitCollider = constraint.colliderB;
    event.selfMeshCollider = constraint.meshColliderA;
    event.hitMeshCollider = constraint.meshColliderB;
    event.selfRigidBody = constraint.rigidBodyA;
    event.hitRigidBody = constraint.rigidBodyB;
    event.otherObject = constraint.objectB;
    event.contactCount = static_cast<uint16_t>(std::min(constraint.pointCount, MAX_CONTACT_POINTS_PER_PAIR));

    for(uint16_t i = 0; i < event.contactCount; ++i) {
      event.contacts[i] = constraint.points[i];
    }

    return event;
  }



  void CollisionScene::dispatchCollisionCallbacks() const {

    struct ObjectPairHash
    {
      size_t operator()(const std::pair<const Object *, const Object *> &p) const
      {
        std::size_t hash = 0;
        const auto combine = [&hash](std::size_t value) {
          hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
        };

        combine(reinterpret_cast<std::uintptr_t>(p.first));
        combine(reinterpret_cast<std::uintptr_t>(p.second));
        return hash;
      }
    };

    struct PendingPairDispatch {
      std::pair<const Object *, const Object *> key{};
      bool hasFirstEvent{false};
      bool hasSecondEvent{false};
      CollEvent firstEvent{};
      CollEvent secondEvent{};
    };

    std::vector<PendingPairDispatch> pendingDispatches;
    pendingDispatches.reserve(cachedConstraintCount_);
    std::unordered_map<std::pair<const Object *, const Object *>, std::size_t, ObjectPairHash> dispatchLookup;

    auto captureDirectionalEvent = [](PendingPairDispatch &dispatch, const CollEvent &event) {
      if(!collisionEventMasksOverlap(event)) return;

      Object *selfObject = collisionEventSelfObject(event);
      if(!selfObject) return;

      if(selfObject == dispatch.key.first) {
        if(!dispatch.hasFirstEvent) {
          dispatch.firstEvent = event;
          dispatch.hasFirstEvent = true;
        }
        return;
      }

      if(selfObject == dispatch.key.second && !dispatch.hasSecondEvent) {
        dispatch.secondEvent = event;
        dispatch.hasSecondEvent = true;
      }
    };

    for(int i = 0; i < cachedConstraintCount_; ++i) {
      const ContactConstraint &constraint = cachedConstraints_[i];
      if(!constraint.isActive || constraint.pointCount <= 0) continue;
      if(!constraint.objectA || !constraint.objectB) continue;
      if(!constraint.objectA->isEnabled() || !constraint.objectB->isEnabled()) continue;

      const auto key = makeObjectPairKey(constraint.objectA, constraint.objectB);

      auto lookupIt = dispatchLookup.find(key);
      if(lookupIt == dispatchLookup.end()) {
        pendingDispatches.push_back(PendingPairDispatch{});
        pendingDispatches.back().key = key;
        lookupIt = dispatchLookup.emplace(key, pendingDispatches.size() - 1).first;
      }

      PendingPairDispatch &dispatch = pendingDispatches[lookupIt->second];
      const CollEvent event = makeCollisionEvent(constraint);
      captureDirectionalEvent(dispatch, event);
      captureDirectionalEvent(dispatch, makeMirroredCollisionEvent(event));
    }

    for(const PendingPairDispatch &dispatch : pendingDispatches) {
      if(dispatch.hasFirstEvent) {
        SceneManager::getCurrent().onObjectCollision(dispatch.firstEvent);
      }
      if(dispatch.hasSecondEvent) {
        SceneManager::getCurrent().onObjectCollision(dispatch.secondEvent);
      }
    }
  }


  // ── Wake island ───────────────────────────────────────────────────

  void CollisionScene::wakeIsland(RigidBody *rigidBody) {
    if(!rigidBody) return;

    std::vector<RigidBody *> island;
    std::unordered_set<RigidBody *> visited;
    collectConnectedIsland(rigidBody, island, visited);

    if(island.empty() && shouldTrackSleepState(rigidBody)) {
      island.push_back(rigidBody);
    }

    for(RigidBody *body : island) {
      if(!body) continue;
      if(body->isSleeping_) {
        body->wake();
      } else {
        body->sleepCounter_ = 0;
      }
    }
  }

  void CollisionScene::wakeBodiesTransformedExternally() {
    std::vector<RigidBody *> wakeCandidates;

    for(RigidBody *body : rigidBodies_) {
      if(!body || !body->isSleeping_) continue;
      if(!rigidBodyTransformExceededSleepThreshold(body)) continue;
      wakeCandidates.push_back(body);
    }

    for(RigidBody *body : wakeCandidates) {
      wakeIsland(body);
    }
  }

  void CollisionScene::updateSleepStates() {
    std::unordered_set<RigidBody *> visited;

    for(RigidBody *body : rigidBodies_) {
      if(!shouldTrackSleepState(body) || body->isSleeping_) continue;
      if(visited.find(body) != visited.end()) continue;

      // Build the island of AWAKE, connected bodies only.
      // They are only woken explicitly when a new dynamic contact forces it.
      std::vector<RigidBody *> island;
      {
        std::vector<RigidBody *> stack;
        stack.push_back(body);
        while(!stack.empty()) {
          RigidBody *current = stack.back();
          stack.pop_back();

          if(!shouldTrackSleepState(current)) continue;
          if(current->isSleeping_) continue; // skip sleeping bodies
          if(visited.find(current) != visited.end()) continue;
          visited.insert(current);
          island.push_back(current);

          for(int i = 0; i < cachedConstraintCount_; ++i) {
            const ContactConstraint &cc = cachedConstraints_[i];
            if(!cc.isActive || cc.isTrigger) continue;

            RigidBody *other = nullptr;
            if(cc.rigidBodyA == current)      other = cc.rigidBodyB;
            else if(cc.rigidBodyB == current) other = cc.rigidBodyA;

            if(!other) continue;
            if(!shouldTrackSleepState(other)) continue;
            if(other->isSleeping_) continue; // skip sleeping neighbours
            if(visited.find(other) == visited.end()) {
              stack.push_back(other);
            }
          }
        }
      }
      if(island.empty()) continue;

      bool islandCanSleep = true;
      for(RigidBody *islandBody : island) {
        const bool transformChangedTooMuch = rigidBodyTransformExceededSleepThreshold(islandBody);
        const bool velocitiesTooHigh = rigidBodyVelocitiesExceededSleepThreshold(islandBody);
        if(transformChangedTooMuch || velocitiesTooHigh) {
          islandCanSleep = false;
          break; // early-out: one active body prevents the whole island from sleeping
        }
      }

      if(!islandCanSleep) {
        for(RigidBody *islandBody : island) {
          islandBody->sleepCounter_ = 0;
        }
        continue;
      }

      bool shouldSleepIsland = true;
      for(RigidBody *islandBody : island) {
        if(islandBody->sleepCounter_ < std::numeric_limits<uint16_t>::max()) {
          islandBody->sleepCounter_++;
        }
        if(islandBody->sleepCounter_ < SLEEP_STEPS) {
          shouldSleepIsland = false;
        }
      }

      if(shouldSleepIsland) {
        for(RigidBody *islandBody : island) {
          islandBody->sleep();
        }
      }
    }
  }

  // ── Contact refresh ───────────────────────────────────────────────

  void CollisionScene::refreshContacts() {
    for(int i = 0; i < cachedConstraintCount_; ++i) {
      ContactConstraint &cc = cachedConstraints_[i];
      if(!cc.isActive) continue;

      if(cc.isTrigger) {
        continue;
      }

      const uint32_t versionA = contactTransformVersion(cc.rigidBodyA, cc.colliderA, cc.meshColliderA);
      const uint32_t versionB = contactTransformVersion(cc.rigidBodyB, cc.colliderB, cc.meshColliderB);
      if(cc.transformVersionA == versionA && cc.transformVersionB == versionB) {
        continue;
      }

      for(int j = 0; j < cc.pointCount; ++j) {
        ContactPoint &cp = cc.points[j];
        if(!cp.active) continue;

        refreshContactPointWorldState(cp, cc);

        // Deactivate if too separated
        if(cp.penetration < -(0.1f * physicsScale_)) {
          cp.active = false;
        }
      }

      // Compact: prune inactive points to free slots for new contacts
      int writeIdx = 0;
      for(int j = 0; j < cc.pointCount; ++j) {
        if(cc.points[j].active) {
          if(writeIdx != j) {
            cc.points[writeIdx] = cc.points[j];
          }
          writeIdx++;
        }
      }
      cc.pointCount = writeIdx;
      cc.transformVersionA = versionA;
      cc.transformVersionB = versionB;
    }
  }

  void CollisionScene::removeInactiveContacts() {
    for(int i = 0; i < cachedConstraintCount_;) {
      ContactConstraint &cc = cachedConstraints_[i];
      if(!cc.isActive) {
        removeCachedConstraintAt(i);
      } else {
        ++i;
      }
    }
  }

  void CollisionScene::rebuildSolverConstraints() {
    solverConstraints_.clear();
    solverConstraints_.reserve(cachedConstraintCount_);

    for(int i = 0; i < cachedConstraintCount_; ++i) {
      ContactConstraint &cc = cachedConstraints_[i];
      if(!cc.isActive || cc.isTrigger || cc.pointCount <= 0) continue;
      solverConstraints_.push_back(&cc);
    }
  }

  // ── Swept substep detection ─────────────────────────────────

  void CollisionScene::detectSweptCollisions() {
    std::vector<NodeProxy> candidates;
    candidates.resize(colliders_.size());

    for(RigidBody *body : rigidBodies_) {
      if(!body || body->isSleeping_ || body->isKinematic_ || !body->position_) continue;

      const float dt = fixedDt_ * body->timeScale_;
      if(dt <= 0.0f) continue;

      const fm_vec3_t displacement = body->linearVelocity_ * dt;

      const std::vector<Collider *> *ownerColliders = findCollidersForOwner(body->owner_);
      if(!ownerColliders || ownerColliders->empty()) continue;

      const fm_vec3_t halfExt = (body->worldAabb_.max - body->worldAabb_.min) * 0.5f;
      const float dispAbs[3] = { fabsf(displacement.x), fabsf(displacement.y), fabsf(displacement.z) };
      const float extArr[3] = { halfExt.x, halfExt.y, halfExt.z };

      // Find maximum displacement-to-half-extent ratio across axes
      float maxRatio = 0.0f;
      for(int axis = 0; axis < 3; ++axis) {
        if(extArr[axis] > FM_EPSILON) {
          maxRatio = fmaxf(maxRatio, dispAbs[axis] / extArr[axis]);
        } else if(dispAbs[axis] > FM_EPSILON) {
          maxRatio = static_cast<float>(MAX_CCD_SUBSTEPS);
          break;
        }
      }

      constexpr float CCD_THRESHOLD = 0.5f;
      if(maxRatio <= CCD_THRESHOLD) continue;

      const int substeps = std::min(
        static_cast<int>(ceilf(maxRatio / CCD_THRESHOLD)),
        MAX_CCD_SUBSTEPS);
      if(substeps <= 1) continue;

      const fm_vec3_t originalPos = *body->position_;

      // Test at intermediate substep positions along the predicted trajectory.
      // k=0 is the current position (handled by normal detection afterwards).
      for(int k = 1; k < substeps; ++k) {
        const float fraction = static_cast<float>(k) / static_cast<float>(substeps);
        *body->position_ = originalPos + (displacement * fraction);

        // Sync this body's colliders at the substep position
        for(Collider *collider : *ownerColliders) {
          if(!collider) continue;
          const fm_vec3_t prevCenter = collider->worldCenter_;
          if(!collider->syncWorldState()) continue;
          const fm_vec3_t disp = collider->worldCenter_ - prevCenter;
          if(collider->aabbTreeNodeId_ != NULL_NODE) {
            colliderAABBTree.moveNode(collider->aabbTreeNodeId_, collider->worldAabb_, disp);
          }
        }

        // Broadphase + narrowphase against other colliders
        for(Collider *collider : *ownerColliders) {
          if(!collider || collider->isTrigger_) continue;

          const int candidateCount = colliderAABBTree.queryBounds(
            collider->worldAabb_, candidates.data(),
            static_cast<int>(candidates.size()));

          for(int ci = 0; ci < candidateCount; ++ci) {
            void *data = colliderAABBTree.getNodeData(candidates[ci]);
            if(!data) continue;
            Collider *collB = static_cast<Collider *>(data);
            if(!collB || collB == collider || !collB->owner_) continue;
            if(collider->owner_ == collB->owner_) continue;
            if(!collider->readsCollider(collB) && !collB->readsCollider(collider)) continue;

            RigidBody *rbB = findRigidBodyByOwner(collB->owner_);
            if(collideDetectObjectToObject(collider, body, collB, rbB)) {
              debugf("CCD substep %d/%d: body %u hit body %u", k, substeps, collider->owner_->id, collB->owner_->id);
            }
          }
        }

        // Broadphase + narrowphase against mesh colliders
        for(MeshCollider *mesh : meshColliders_) {
          if(!mesh || mesh->triangleCount_ <= 0) continue;
          for(Collider *collider : *ownerColliders) {
            if(!collider || !collider->owner_) continue;
            if(!collider->readsMeshCollider(mesh) && !mesh->readsCollider(collider)) continue;
            if(!aabbOverlap(collider->worldAabb_, mesh->worldAabb_)) continue;
            if(collideDetectObjectToMesh(collider, body, *mesh)) {
              debugf("CCD substep %d/%d: body %u hit mesh %u", k, substeps, collider->owner_->id, mesh->owner_ ? static_cast<unsigned>(mesh->owner_->id) : 0u);
            }
          }
        }
      }

      // Restore original position and sync colliders back
      *body->position_ = originalPos;
      for(Collider *collider : *ownerColliders) {
        if(!collider) continue;
        const fm_vec3_t prevCenter = collider->worldCenter_;
        if(!collider->syncWorldState()) continue;
        const fm_vec3_t disp = collider->worldCenter_ - prevCenter;
        if(collider->aabbTreeNodeId_ != NULL_NODE) {
          colliderAABBTree.moveNode(collider->aabbTreeNodeId_, collider->worldAabb_, disp);
        }
      }
    }
  }

  // ── Contact detection ─────────────────────────────────────────────

  void CollisionScene::detectAllContacts() {

    // Mark all constraints as inactive; detection will re-activate them
    for(int i = 0; i < cachedConstraintCount_; ++i) {
      cachedConstraints_[i].isActive = false;
    }

    // Swept substep detection for fast-moving bodies that could tunnel through geometry
    detectSweptCollisions();

    //map of unique collider pairs that have already been tested this step to avoid duplication
    std::unordered_set<int32_t> tested_pairs;

    //list of candidate colliders for broad phase query results
    std::vector<NodeProxy> candidateColliders;

    
    
    candidateColliders.resize(colliders_.size());
    const uint64_t bodyDetectStart = get_ticks();
    for (Collider *collider : colliders_)
    {
      if (!collider || !collider->owner_)
        continue;
      if (collider->isTrigger_)
        continue;

      RigidBody *rbA = findRigidBodyByOwner(collider->owner_);

      const int candidateCount = colliderAABBTree.queryBounds(
          collider->worldAabb_,
          candidateColliders.data(),
          static_cast<int>(candidateColliders.size()));
      for (int candidateIdx = 0; candidateIdx < candidateCount; ++candidateIdx)
      {
        void *data = colliderAABBTree.getNodeData(candidateColliders[candidateIdx]);
        if (!data)
          continue;
        
        Collider *collB = static_cast<Collider *>(data);

        // When you get a candidate pair:
        auto key = AABBTree::makeNodePairKey(collider->aabbTreeNodeId_, collB->aabbTreeNodeId_);
        if (tested_pairs.insert(key).second)
        {
          // Was not present -> test this pair
          // don't let collider collide with itself or colliders of the same object
          if (!collB || collB == collider || !collB->owner_)
            continue;
          if (collider->owner_ == collB->owner_)
            continue;

          if (!collider->readsCollider(collB) && !collB->readsCollider(collider))
            continue;
          RigidBody *rbB = findRigidBodyByOwner(collB->owner_);
          if((rbA && rbA->isSleeping_) && (rbB && rbB->isSleeping_)) {
            // Allow sleeping objects to generate contacts with triggers, but skip if both are sleeping non-triggers to save performance
            if(!collider->isTrigger_ && !collB->isTrigger_) {
              continue;
            }
          }
          collideDetectObjectToObject(collider, rbA, collB, rbB);
        }
      }
    }
    ticksDetectBodyPairs = get_ticks() - bodyDetectStart;

    const uint64_t meshDetectStart = get_ticks();
    for (std::size_t m = 0; m < meshColliders_.size(); ++m)
    {

      MeshCollider *mesh = meshColliders_[m];

      if (!mesh || mesh->triangleCount_ <= 0)
        continue;

      const int candidateCount = colliderAABBTree.queryBounds(
          mesh->worldAabb_,
          candidateColliders.data(),
          static_cast<int>(candidateColliders.size()));

      for (int candidateIdx = 0; candidateIdx < candidateCount; ++candidateIdx)
      {
        void *data = colliderAABBTree.getNodeData(candidateColliders[candidateIdx]);
        if (!data)
          continue;

        Collider *collA = static_cast<Collider *>(data);
        if(!collA || !collA->owner_)
          continue;

        if (!collA->readsMeshCollider(mesh) && !mesh->readsCollider(collA))
          continue;

        RigidBody *rigidBodyA = findRigidBodyByOwner(collA->owner_);
        //prevent perpetural collision checks of sleeping objects with meshes
        if(!mesh->transformChanged_ && rigidBodyA && rigidBodyA->isSleeping_ && !collA->isTrigger_)
          continue;
        collideDetectObjectToMesh(collA, rigidBodyA, *mesh);
      }

    }
    ticksDetectMeshPairs = get_ticks() - meshDetectStart;
    //TODO: possibly offer mesh-mesh collision detection in the future, but not needed for current use cases

    removeInactiveContacts();
  }

  // ── Pre-solve ─────────────────────────────────────────────────────

  void CollisionScene::preSolveContacts() {
    const float restitutionSlop = 0.5f * physicsScale_;

    for(ContactConstraint *constraint : solverConstraints_) {
      ContactConstraint &cc = *constraint;

      RigidBody *a = cc.rigidBodyA;
      RigidBody *b = cc.rigidBodyB;
      const bool aHasMotionAngular = canApplyAngularResponse(a);
      const bool bHasMotionAngular = canApplyAngularResponse(b);
      const bool aCanRotate = cc.respondsA && aHasMotionAngular;
      const bool bCanRotate = cc.respondsB && bHasMotionAngular;

      float invMassA = cc.respondsA ? constrainedLinearInvMassAlong(a, cc.normal) : 0.0f;
      float invMassB = cc.respondsB ? constrainedLinearInvMassAlong(b, cc.normal) : 0.0f;
      float totalInvMass = invMassA + invMassB;
      const float linearU = (cc.respondsA ? constrainedLinearInvMassAlong(a, cc.tangentU) : 0.0f) +
                (cc.respondsB ? constrainedLinearInvMassAlong(b, cc.tangentU) : 0.0f);
      const float linearV = (cc.respondsA ? constrainedLinearInvMassAlong(a, cc.tangentV) : 0.0f) +
                (cc.respondsB ? constrainedLinearInvMassAlong(b, cc.tangentV) : 0.0f);
      if(totalInvMass < FM_EPSILON) continue;

      for(int j = 0; j < cc.pointCount; ++j) {
        ContactPoint &cp = cc.points[j];
        if(!cp.active) continue;

        // Relative vectors from centers of mass
        cp.aToContact = a ? cp.contactA - a->worldCenterOfMass() : VEC3_ZERO;
        cp.bToContact = b ? cp.contactB - b->worldCenterOfMass() : VEC3_ZERO;

        // Normal effective mass: 1 / (invMassA + invMassB + (rA×n)·I_A^-1·(rA×n) + ...)
        fm_vec3_t raCrossN;
        fm_vec3_cross(&raCrossN, &cp.aToContact, &cc.normal);
        fm_vec3_t rbCrossN;
        fm_vec3_cross(&rbCrossN, &cp.bToContact, &cc.normal);

        float angularA = 0.0f;
        if(aCanRotate) {
          fm_vec3_t inertia = a->applyConstrainedWorldInertia(raCrossN);
          angularA = fm_vec3_dot(&raCrossN, &inertia);
        }

        float angularB = 0.0f;
        if(bCanRotate) {
          fm_vec3_t inertia = b->applyConstrainedWorldInertia(rbCrossN);
          angularB = fm_vec3_dot(&rbCrossN, &inertia);
        }

        float denomN = totalInvMass + angularA + angularB;
        if(denomN < FM_EPSILON) denomN = FM_EPSILON;
        cp.normalMass = 1.0f / denomN;

        // Tangent effective masses
        {
          fm_vec3_t raCrossU;
          fm_vec3_cross(&raCrossU, &cp.aToContact, &cc.tangentU);
          fm_vec3_t rbCrossU;
          fm_vec3_cross(&rbCrossU, &cp.bToContact, &cc.tangentU);
          float angU_A = 0.0f;
          if(aCanRotate) {
            fm_vec3_t inertia = a->applyConstrainedWorldInertia(raCrossU);
            angU_A = fm_vec3_dot(&raCrossU, &inertia);
          }
          float angU_B = 0.0f;
          if(bCanRotate) {
            fm_vec3_t inertia = b->applyConstrainedWorldInertia(rbCrossU);
            angU_B = fm_vec3_dot(&rbCrossU, &inertia);
          }
          float denomU = linearU + angU_A + angU_B;
          if(denomU < FM_EPSILON) denomU = FM_EPSILON;
          cp.tangentMassU = 1.0f / denomU;
        }
        {
          fm_vec3_t raCrossV;
          fm_vec3_cross(&raCrossV, &cp.aToContact, &cc.tangentV);
          fm_vec3_t rbCrossV;
          fm_vec3_cross(&rbCrossV, &cp.bToContact, &cc.tangentV);
          float angV_A = 0.0f;
          if(aCanRotate) {
            fm_vec3_t inertia = a->applyConstrainedWorldInertia(raCrossV);
            angV_A = fm_vec3_dot(&raCrossV, &inertia);
          }
          float angV_B = 0.0f;
          if(bCanRotate) {
            fm_vec3_t inertia = b->applyConstrainedWorldInertia(rbCrossV);
            angV_B = fm_vec3_dot(&rbCrossV, &inertia);
          }
          float denomV = linearV + angV_A + angV_B;
          if(denomV < FM_EPSILON) denomV = FM_EPSILON;
          cp.tangentMassV = 1.0f / denomV;
        }

        // Velocity bias (restitution only; Baumgarte is handled in position solver)
        cp.velocityBias = 0.0f;

        // Restitution bias
        fm_vec3_t relVel = VEC3_ZERO;
        if(a) {
          fm_vec3_t aCross;
          fm_vec3_cross(&aCross, &a->angularVelocity_, &cp.aToContact);
          relVel = a->linearVelocity_ + aCross;
        }
        if(b) {
          fm_vec3_t bCross;
          fm_vec3_cross(&bCross, &b->angularVelocity_, &cp.bToContact);
          relVel -= (b->linearVelocity_ + bCross);
        }
        float relVelN = fm_vec3_dot(&relVel, &cc.normal);
        if(relVelN < -restitutionSlop) {
          cp.velocityBias += cc.combinedBounce * relVelN;
        }
      }
    }
  }

  // ── Warm start ────────────────────────────────────────────────────

  void CollisionScene::warmStart() {
    // Minimum impulse threshold: below this, accumulated impulses are zeroed to
    // prevent small residual forces from keeping bodies awake.
    // Bullet's btSequentialImpulseConstraintSolver similarly discards negligible
    // cached impulses. The warm starting factor (0.85) decays
    // impulses each frame
    constexpr float IMPULSE_ZERO_THRESHOLD = FM_EPSILON;

    for(ContactConstraint *constraint : solverConstraints_) {
      ContactConstraint &cc = *constraint;

      RigidBody *a = cc.rigidBodyA;
      RigidBody *b = cc.rigidBodyB;

      for(int j = 0; j < cc.pointCount; ++j) {
        ContactPoint &cp = cc.points[j];
        if(!cp.active) continue;

        // Scale accumulated impulses by warm starting factor (Bullet's m_warmstartingFactor = 0.85)
        // This prevents overcorrection when constraint configuration changes between frames
        cp.accumulatedNormalImpulse *= WARM_STARTING_FACTOR;
        cp.accumulatedTangentImpulseU *= WARM_STARTING_FACTOR;
        cp.accumulatedTangentImpulseV *= WARM_STARTING_FACTOR;

        // Zero out decayed impulses to prevent persistent micro-impulses that can cause jitter and prevent sleeping
        if(fabsf(cp.accumulatedNormalImpulse) < IMPULSE_ZERO_THRESHOLD) cp.accumulatedNormalImpulse = 0.0f;
        if(fabsf(cp.accumulatedTangentImpulseU) < IMPULSE_ZERO_THRESHOLD) cp.accumulatedTangentImpulseU = 0.0f;
        if(fabsf(cp.accumulatedTangentImpulseV) < IMPULSE_ZERO_THRESHOLD) cp.accumulatedTangentImpulseV = 0.0f;

        fm_vec3_t impulse = cc.normal * cp.accumulatedNormalImpulse;
        impulse += cc.tangentU * cp.accumulatedTangentImpulseU;
        impulse += cc.tangentV * cp.accumulatedTangentImpulseV;

        if(cc.respondsA) applyConstrainedImpulseAtContact(a, impulse, cp.aToContact);
        if(cc.respondsB) applyConstrainedImpulseAtContact(b, -impulse, cp.bToContact);
      }
    }
  }

  // ── Velocity constraint solver ────────────────────────────────────

  /// Fast xorshift32 PRNG for constraint randomization (Bullet's SOLVER_RANDMIZE_ORDER).
  static uint32_t s_solverRngState = 0x12345678u;
  static uint32_t solverRand() {
    uint32_t x = s_solverRngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_solverRngState = x;
    return x;
  }

  void CollisionScene::solveVelocityConstraints() {
    const auto constraintCount = solverConstraints_.size();
    if(constraintCount == 0) return;

    constexpr float VELOCITY_SOLVER_EARLY_OUT_THRESHOLD = 1e-4f;
    constexpr float VELOCITY_SOLVER_NORMAL_ERROR_THRESHOLD_PER_SCALE = 1e-3f;
    constexpr uint8_t MIN_NORMAL_SOLVER_ITERATIONS = 6;
    const float velocitySolverNormalErrorThreshold = VELOCITY_SOLVER_NORMAL_ERROR_THRESHOLD_PER_SCALE * physicsScale_;

    const auto solveFrictionPass = [&]() {
      for(ContactConstraint *constraint : solverConstraints_) {
        ContactConstraint &cc = *constraint;

        if(cc.combinedFriction <= FM_EPSILON) continue;

        RigidBody *a = cc.rigidBodyA;
        RigidBody *b = cc.rigidBodyB;
        const bool aHasMotionAngular = canApplyAngularResponse(a);
        const bool bHasMotionAngular = canApplyAngularResponse(b);

        for(int j = 0; j < cc.pointCount; ++j) {
          ContactPoint &cp = cc.points[j];
          if(!cp.active) continue;

          fm_vec3_t contactVelA = VEC3_ZERO;
          fm_vec3_t contactVelB = VEC3_ZERO;
          if(a && !a->isKinematic_) {
            contactVelA = a->linearVelocity_;
            if(aHasMotionAngular) {
              fm_vec3_t aCross;
              fm_vec3_cross(&aCross, &a->angularVelocity_, &cp.aToContact);
              contactVelA += aCross;
            }
          }
          if(b && !b->isKinematic_) {
            contactVelB = b->linearVelocity_;
            if(bHasMotionAngular) {
              fm_vec3_t bCross;
              fm_vec3_cross(&bCross, &b->angularVelocity_, &cp.bToContact);
              contactVelB += bCross;
            }
          }

          fm_vec3_t relVel = contactVelA - contactVelB;
          float vTangentU = fm_vec3_dot(&relVel, &cc.tangentU);
          float vTangentV = fm_vec3_dot(&relVel, &cc.tangentV);

          float lambdaU = -vTangentU * cp.tangentMassU;
          float lambdaV = -vTangentV * cp.tangentMassV;

          float newAccumU = cp.accumulatedTangentImpulseU + lambdaU;
          float newAccumV = cp.accumulatedTangentImpulseV + lambdaV;

          float maxFriction = cc.combinedFriction * cp.accumulatedNormalImpulse;
          float tangentMagnitude = sqrtf(newAccumU * newAccumU + newAccumV * newAccumV);
          if(tangentMagnitude > maxFriction && tangentMagnitude > FM_EPSILON) {
            float scale = maxFriction / tangentMagnitude;
            newAccumU *= scale;
            newAccumV *= scale;
          }

          lambdaU = newAccumU - cp.accumulatedTangentImpulseU;
          lambdaV = newAccumV - cp.accumulatedTangentImpulseV;

          cp.accumulatedTangentImpulseU = newAccumU;
          cp.accumulatedTangentImpulseV = newAccumV;

          fm_vec3_t tangentImpulse = cc.tangentU * lambdaU + cc.tangentV * lambdaV;
          if(fm_vec3_len2(&tangentImpulse) <= FM_EPSILON * FM_EPSILON) continue;

          if(cc.respondsA) applyConstrainedImpulseAtContact(a, tangentImpulse, cp.aToContact);
          if(cc.respondsB) applyConstrainedImpulseAtContact(b, -tangentImpulse, cp.bToContact);
        }
      }
    };

    for(uint8_t iter = 0; iter < velocitySolverIterations_; ++iter) {
      float maxNormalImpulseDelta = 0.0f;
      float maxNormalError = 0.0f;

      // Shuffle constraint processing order each iteration (Bullet's SOLVER_RANDMIZE_ORDER)
      // Prevents systematic bias where one constraint always "wins" in Gauss-Seidel iteration
      for(std::size_t i = constraintCount; i > 1; --i) {
        std::size_t j = solverRand() % i;
        std::swap(solverConstraints_[i - 1], solverConstraints_[j]);
      }

      for(ContactConstraint *constraint : solverConstraints_) {
        ContactConstraint &cc = *constraint;

        RigidBody *a = cc.rigidBodyA;
        RigidBody *b = cc.rigidBodyB;
        const bool aHasMotionAngular = canApplyAngularResponse(a);
        const bool bHasMotionAngular = canApplyAngularResponse(b);

        for(int j = 0; j < cc.pointCount; ++j) {
          ContactPoint &cp = cc.points[j];
          if(!cp.active) continue;

          // Compute relative velocity at contact.
          fm_vec3_t relVel = VEC3_ZERO;
          if(a) {
            relVel = a->linearVelocity_;
            if(aHasMotionAngular) {
              fm_vec3_t aCross;
              fm_vec3_cross(&aCross, &a->angularVelocity_, &cp.aToContact);
              relVel += aCross;
            }
          }
          if(b) {
            fm_vec3_t velB = b->linearVelocity_;
            if(bHasMotionAngular) {
              fm_vec3_t bCross;
              fm_vec3_cross(&bCross, &b->angularVelocity_, &cp.bToContact);
              velB += bCross;
            }
            relVel -= velB;
          }

          const float relVelN = fm_vec3_dot(&relVel, &cc.normal);
          maxNormalError = fmaxf(maxNormalError, fmaxf(-(relVelN + cp.velocityBias), 0.0f));

          float dImpulseN = cp.normalMass * (-(relVelN + cp.velocityBias));

          // Clamp accumulated impulse (normal must be non-negative).
          const float oldAccum = cp.accumulatedNormalImpulse;
          cp.accumulatedNormalImpulse = fmaxf(oldAccum + dImpulseN, 0.0f);
          dImpulseN = cp.accumulatedNormalImpulse - oldAccum;
          maxNormalImpulseDelta = fmaxf(maxNormalImpulseDelta, fabsf(dImpulseN));

          const fm_vec3_t impulseN = cc.normal * dImpulseN;
          if(fabsf(dImpulseN) > FM_EPSILON) {
            if(cc.respondsA) applyConstrainedImpulseAtContact(a, impulseN, cp.aToContact);
            if(cc.respondsB) applyConstrainedImpulseAtContact(b, -impulseN, cp.bToContact);
          }
        }
      }

      if(iter + 1 >= MIN_NORMAL_SOLVER_ITERATIONS &&
         maxNormalImpulseDelta < VELOCITY_SOLVER_EARLY_OUT_THRESHOLD &&
        maxNormalError < velocitySolverNormalErrorThreshold) {
        break;
      }
    }

    solveFrictionPass();
  }


  // ── Position constraint solver ────────────────────────────────────

  bool CollisionScene::solvePositionConstraints() {
    const float slop = 0.01f * physicsScale_;
    const float steering = 0.3f;
    const float maxCorrection = 0.04f * physicsScale_;
    bool appliedCorrection = false;

    for(ContactConstraint *constraint : solverConstraints_) {
      ContactConstraint &cc = *constraint;

      RigidBody *a = cc.rigidBodyA;
      RigidBody *b = cc.rigidBodyB;
      const bool aCanRotate = cc.respondsA && canApplyAngularResponse(a);
      const bool bCanRotate = cc.respondsB && canApplyAngularResponse(b);
      const float invMassA = cc.respondsA ? constrainedLinearInvMassAlong(a, cc.normal) : 0.0f;
      const float invMassB = cc.respondsB ? constrainedLinearInvMassAlong(b, cc.normal) : 0.0f;

      for(int j = 0; j < cc.pointCount; ++j) {
        ContactPoint &cp = cc.points[j];
        if(!cp.active) continue;

        refreshContactPointWorldState(cp, cc, true);

        if(cp.penetration < slop) continue;

        float steeringForce = fminf(steering * (cp.penetration - slop), maxCorrection);
        if(steeringForce <= 0.0f) continue;

        float invMassSum = invMassA + invMassB;

        // Add rotational inertia terms
        if(aCanRotate) {
          fm_vec3_t rCrossN;
          fm_vec3_cross(&rCrossN, &cp.aToContact, &cc.normal);
          fm_vec3_t inertia = a->applyConstrainedWorldInertia(rCrossN);
          invMassSum += fm_vec3_dot(&rCrossN, &inertia);
        }
        if(bCanRotate) {
          fm_vec3_t rCrossN;
          fm_vec3_cross(&rCrossN, &cp.bToContact, &cc.normal);
          fm_vec3_t inertia = b->applyConstrainedWorldInertia(rCrossN);
          invMassSum += fm_vec3_dot(&rCrossN, &inertia);
        }

        if(invMassSum < FM_EPSILON) continue;

        float correctionMag = steeringForce / invMassSum;
        fm_vec3_t impulse = cc.normal * correctionMag;
        appliedCorrection = true;

        // Apply linear + angular corrections to A
        if(a && !a->isKinematic_ && a->position_) {
          if(invMassA > 0.0f) {
            fm_vec3_t corrA = constrainLinearWorld(a, cc.normal * (correctionMag * invMassA));
            *a->position_ = *a->position_ + corrA;
          }
          if(aCanRotate) {
            fm_vec3_t angImpulse;
            fm_vec3_cross(&angImpulse, &cp.aToContact, &impulse);
            fm_vec3_t rotChange = a->applyConstrainedWorldInertia(angImpulse);
            float angle = fm_vec3_len(&rotChange);
            if(angle > FM_EPSILON) {
              fm_vec3_t axis = rotChange / angle;
              fm_quat_t dq;
              fm_quat_from_axis_angle(&dq, &axis, angle);
              *a->rotation_ = dq * *a->rotation_;
              fm_quat_norm(a->rotation_, a->rotation_);
            }
          }
        }

        // Apply linear + angular corrections to B
        if(b && !b->isKinematic_ && b->position_) {
          if(invMassB > 0.0f) {
            fm_vec3_t corrB = constrainLinearWorld(b, cc.normal * (correctionMag * invMassB));
            *b->position_ = *b->position_ - corrB;
          }
          if(bCanRotate) {
            fm_vec3_t angImpulse;
            fm_vec3_cross(&angImpulse, &cp.bToContact, &impulse);
            angImpulse = -angImpulse;
            fm_vec3_t rotChange = b->applyConstrainedWorldInertia(angImpulse);
            float angle = fm_vec3_len(&rotChange);
            if(angle > FM_EPSILON) {
              fm_vec3_t axis = rotChange / angle;
              fm_quat_t dq;
              fm_quat_from_axis_angle(&dq, &axis, angle);
              *b->rotation_ = dq * *b->rotation_;
              fm_quat_norm(b->rotation_, b->rotation_);
            }
          }
        }
      }
    }

    return appliedCorrection;
  }

  /// @brief Recalculate the world-space AABBs of all Mesh Colliders in the Collision Scene.
  void CollisionScene::updateMeshColliderWorldStates() {
    for(std::size_t i = 0; i < meshColliders_.size(); ++i) {
      MeshCollider *mesh = meshColliders_[i];
      if(!mesh) continue;

      mesh->transformChanged_ = mesh->hasOwnerTransformChanged();
      if(!mesh->transformChanged_ && mesh->hasCachedOwnerTransform_) continue;

      mesh->recalculateWorldAabb();
      mesh->syncOwnerTransform();
    }
  }

  // ── Raycast ───────────────────────────────────────────────────────


  bool CollisionScene::raycast(Raycast &ray, RaycastHit &hit) const {
    RaycastHit currentHit = {};
    hit.didHit = false;
    hit.distance = std::numeric_limits<float>::max();
    currentHit.distance = std::numeric_limits<float>::max();

    // Test mesh colliders
    if(hasFlag(ray.collTypes, RaycastColliderTypeFlags::MESH_COLLIDERS)) {
      for(std::size_t m = 0; m < meshColliders_.size(); ++m) {
        const MeshCollider *mesh = meshColliders_[m];
        if(!mesh || mesh->triangleCount_ == 0 || !mesh->owner_) continue;
        Raycast localRay = ray;
        if(mesh->hasScale()) {
          const fm_vec3_t &scale = mesh->owner_->scale;
          if(fabsf(scale.x) <= FM_EPSILON || fabsf(scale.y) <= FM_EPSILON || fabsf(scale.z) <= FM_EPSILON) {
            continue;
          }
        }
        localRay.origin = mesh->hasTransform() ? mesh->toLocalSpace(ray.origin) : ray.origin;
        localRay.dir = mesh->hasTransform() ? mesh->rotateToLocal(ray.dir) : ray.dir;
        localRay.invDir = fm_vec3_t{{
          fabsf(localRay.dir.x) > FM_EPSILON ? 1.0f / localRay.dir.x : copysignf(1.0f / FM_EPSILON, localRay.dir.x),
          fabsf(localRay.dir.y) > FM_EPSILON ? 1.0f / localRay.dir.y : copysignf(1.0f / FM_EPSILON, localRay.dir.y),
          fabsf(localRay.dir.z) > FM_EPSILON ? 1.0f / localRay.dir.z : copysignf(1.0f / FM_EPSILON, localRay.dir.z)
        }};


        NodeProxy triCandidates[RAYCAST_MAX_TRIANGLE_TESTS];
        int triCount = mesh->aabbTree_.queryRay(localRay, triCandidates, RAYCAST_MAX_TRIANGLE_TESTS);
        for(int i = 0; i < triCount; ++i) {
          void *data = mesh->aabbTree_.getNodeData(triCandidates[i]);
          if(!data) continue;
          int triIdx = static_cast<int>(reinterpret_cast<intptr_t>(data)) - 1; // stored as index+1
          if(triIdx < 0 || triIdx >= mesh->triangleCount_) continue;

          const MeshTriangleIndices &tri = mesh->triangles_[triIdx];

          // Get vertices in local space
          fm_vec3_t v0 = mesh->vertices_[tri.indices[0]];
          fm_vec3_t v1 = mesh->vertices_[tri.indices[1]];
          fm_vec3_t v2 = mesh->vertices_[tri.indices[2]];

          currentHit.distance = std::numeric_limits<float>::max();
          currentHit.didHit = false;
          if(!ray_triangle_intersection(localRay, v0, v1, v2, mesh->normals_[triIdx], currentHit)) continue;

          currentHit.point = mesh->hasTransform() ? mesh->toWorldSpace(currentHit.point) : currentHit.point;
          currentHit.normal = mesh->hasTransform() ? mesh->localNormalToWorld(currentHit.normal) : currentHit.normal;
          const fm_vec3_t hitDelta = currentHit.point - ray.origin;
          currentHit.distance = fm_vec3_len(&hitDelta);
          currentHit.hitObjectId = mesh->owner_->id;

          hit.didHit = true;
          if(currentHit.didHit && currentHit.distance < hit.distance && currentHit.distance <= ray.maxDistance) {
            hit = currentHit;
          }
        }
      }
    }

    // Test physics objects
    if(hasFlag(ray.collTypes, RaycastColliderTypeFlags::COLLIDER_BODIES)) {
      NodeProxy collCandidates[RAYCAST_MAX_COLLIDER_TESTS];
      int candidate_count = colliderAABBTree.queryRay(ray, collCandidates, RAYCAST_MAX_COLLIDER_TESTS);

      for(int i = 0; i < candidate_count; ++i) {
        void *data = colliderAABBTree.getNodeData(collCandidates[i]);
        if(!data) continue;
        auto *coll = static_cast<Collider *>(data);
        
        if(!coll->owner_) continue;
        if(!ray.interactTrigger && coll->isTrigger_) continue;
        if((coll->writeMask_ & ray.readMask) == 0) continue;
        currentHit.didHit = false;
        currentHit.distance = std::numeric_limits<float>::max();
        hit.didHit = hit.didHit | ray_collider_intersection(ray, coll, currentHit);
        if(currentHit.didHit && currentHit.distance < hit.distance && currentHit.distance <= ray.maxDistance) {
          hit = currentHit;
        }
      }
    }

    return hit.didHit;
  }

  // ── Main step ─────────────────────────────────────────────────────

  void CollisionScene::step() {
    const uint64_t totalStart = get_ticks();

    uint64_t stageStart = get_ticks();

    // Update mesh collider world states (in case transforms changed)
    // recalculates world AABBs and marks if transform changed for potential broadphase optimization
    updateMeshColliderWorldStates();

    // Wake sleeping rigid bodies that were moved or rotated externally.
    wakeBodiesTransformedExternally();
    ticksWakePrep = get_ticks() - stageStart;

    stageStart = get_ticks();
    // Refresh collider world state
    for(Collider *collider : colliders_) {
      if(!collider) continue;

      const fm_vec3_t previousCenter = collider->worldCenter_;
      if(!collider->syncWorldState() || collider->aabbTreeNodeId_ == NULL_NODE) continue;

      const fm_vec3_t displacement = collider->worldCenter_ - previousCenter;
      colliderAABBTree.moveNode(collider->aabbTreeNodeId_, collider->worldAabb_, displacement);
    }

    // Update compound CoM/inertia on demand and refresh world inertia tensors.
    for (RigidBody *body : rigidBodies_){
      syncCompoundProperties(body);
      if(body->isSleeping_) continue;
      body->updateWorldInertia();
    }
    ticksWorldUpdate = get_ticks() - stageStart;

    stageStart = get_ticks();
    // Integrate velocities (also resets sleeping bodies — see RigidBody::integrateVelocity)
    for(RigidBody *body : rigidBodies_) {
      body->integrateVelocity(fixedDt_, gravity_);
      body->integrateAngularVelocity(fixedDt_);
    }
    ticksIntegrateVel = get_ticks() - stageStart;

    // Detect all contacts (broad + narrow phase)
    const uint64_t detectStart = get_ticks();
    detectAllContacts();
    ticksDetect = get_ticks() - detectStart;

    stageStart = get_ticks();
    // Refresh anchors from local-space points before solving.
    refreshContacts();
    rebuildSolverConstraints();

    dispatchCollisionCallbacks();
    ticksRefreshCallbacks = get_ticks() - stageStart;

    // Pre-solve contacts (compute effective masses)
    stageStart = get_ticks();
    preSolveContacts();
    ticksPreSolve = get_ticks() - stageStart;

    // Warm start
    stageStart = get_ticks();
    // Reset push velocities for split impulse (Bullet-style)
    for(RigidBody *body : rigidBodies_) {
      if(!body->isSleeping_) body->resetPushVelocities();
    }
    warmStart();
    ticksWarmStart = get_ticks() - stageStart;

    // Velocity constraint solver
    stageStart = get_ticks();
    solveVelocityConstraints();

    ticksVelocitySolve = get_ticks() - stageStart;

    // Integrate positions and rotations (including split impulse push velocities)
    stageStart = get_ticks();
    for(RigidBody *body : rigidBodies_) {
      if(body->isSleeping_) continue;

      body->integratePosition(fixedDt_);
      body->integrateRotation(fixedDt_);

    }
    ticksIntegration = get_ticks() - stageStart;

    // Position constraint solver
    stageStart = get_ticks();
    for(uint8_t iter = 0; iter < positionSolverIterations_; ++iter) {
      if(!solvePositionConstraints()) {
        break;
      }
    }
    ticksPositionSolve = get_ticks() - stageStart;

    // Apply position constraints, inertia and world state of rigidbodies and colliders
    stageStart = get_ticks();
    for(RigidBody *body : rigidBodies_) {

      if(!body || !body->owner_) continue;

      body->applyPositionConstraints();
      body->updateWorldInertia();

      const std::vector<Collider *> *ownerColliders = findCollidersForOwner(body->owner_);
      if(!ownerColliders || ownerColliders->empty()) continue;

      body->worldAabb_ = AABB {.min = body->worldCenterOfMass_, .max = body->worldCenterOfMass_};
      fm_vec3_t displacement = *body->position_ - body->previousStepPosition_;
      for (Collider *collider : *ownerColliders)
      {
        if (!collider) continue;
        body->worldAabb_ = aabbUnion(body->worldAabb_, collider->worldAabb_);
        colliderAABBTree.moveNode(collider->aabbTreeNodeId_, collider->worldAabb_, displacement);
      }

      // Snapshot the fully-corrected transform for sleep evaluation.
      // This must happen AFTER the position solver and split impulse push so that
      // solver corrections (penetration resolution) do not register as "movement"
      // in the sleep threshold check.
      if(body->position_) body->previousStepPosition_ = *body->position_;
      if(body->rotation_) body->previousStepRotation_ = *body->rotation_;
      if(body->owner_)    body->previousStepScale_ = body->owner_->scale;
    }

    // Update RigidBody sleep states
    updateSleepStates();
    ticksFinalize = get_ticks() - stageStart;

    ticksTotal = get_ticks() - totalStart;
  }

  /// @brief Draws debug visuals for the collision scene.
  /// Draws on the CPU which may cause significant slowdown
  /// @param showMeshColliders Whether to draw mesh colliders.
  /// @param showRigidBodies Whether to draw rigid bodies.
  void P64::Coll::CollisionScene::debugDraw(bool showMeshColliders, bool showRigidBodies)
  {
    if (showMeshColliders)
    {
      for (std::size_t meshIdx = 0; meshIdx < meshColliders_.size(); ++meshIdx)
      {
        const auto *meshCollider = meshColliders_[meshIdx];
        if (!meshCollider || !meshCollider->vertices_ || !meshCollider->triangles_ || meshCollider->triangleCount_ == 0)
        {
          continue;
        }

        color_t color = Debug::paletteColor(static_cast<uint32_t>(meshIdx));

        const bool useRotation = meshCollider->hasRotation();

  const fm_quat_t rot = meshCollider->owner_->rot;
  const fm_vec3_t pos = meshCollider->owner_->pos;
  const fm_vec3_t scale = meshCollider->owner_->scale;

        auto toWorldMesh = [&](const fm_vec3_t &local)
        {
          fm_vec3_t scaled = fm_vec3_t{{local.x * scale.x, local.y * scale.y, local.z * scale.z}};
          if (useRotation)
          {
            scaled = rot * scaled;
          }
          return scaled + pos;
        };

        for (uint16_t t = 0; t < meshCollider->triangleCount_; ++t)
        {
          int idxA = meshCollider->triangles_[t].indices[0];
          int idxB = meshCollider->triangles_[t].indices[1];
          int idxC = meshCollider->triangles_[t].indices[2];

          fm_vec3_t v0 = toWorldMesh(meshCollider->vertices_[idxA]);
          fm_vec3_t v1 = toWorldMesh(meshCollider->vertices_[idxB]);
          fm_vec3_t v2 = toWorldMesh(meshCollider->vertices_[idxC]);

          Debug::drawLine(v0, v1, color);
          Debug::drawLine(v1, v2, color);
          Debug::drawLine(v2, v0, color);
        }
      }
    }

    if (showRigidBodies)
    {
      for (const auto &collider : colliders_)
      {

        color_t col{0xFF, 0xFF, 0x00, 0xFF};
        if (collider)
        {
          const RigidBody *rigidBody = findRigidBodyByOwner(collider->owner_);
          const bool isSleepingBody = rigidBody && rigidBody->isSleeping_;

          if (isSleepingBody)
          {
            col = color_t{0x80, 0x80, 0x80, 0xFF};
          }

          switch (collider->type_)
          {
          case ShapeType::Sphere:
            if (!isSleepingBody) col = color_t{0xFF, 0x00, 0x00, 0xFF};
            Debug::drawSphere(collider->worldCenter_, collider->sphere_.radius, col);
            break;
          case ShapeType::Box:
            if (!isSleepingBody) col = color_t{0x00, 0xFF, 0xFF, 0xFF};
            Debug::drawOBB(collider->worldCenter_, collider->box_.halfSize, collider->owner_->rot, col);
            break;
          case ShapeType::Capsule:
            if (!isSleepingBody) col = color_t{0x00, 0x80, 0xFF, 0xFF};
            Debug::drawCapsule(
                collider->worldCenter_,
                collider->capsule_.radius,
                collider->capsule_.innerHalfHeight,
                collider->owner_->rot,
                col);
            break;
          case ShapeType::Cylinder:
            if (!isSleepingBody) col = color_t{0xFF, 0x80, 0x00, 0xFF};
            Debug::drawCylinder(
                collider->worldCenter_,
                collider->cylinder_.radius,
                collider->cylinder_.halfHeight,
                collider->owner_->rot,
                col);
            break;
          case ShapeType::Cone:
            if (!isSleepingBody) col = color_t{0xFF, 0x40, 0xA0, 0xFF};
            Debug::drawCone(
                collider->worldCenter_,
                collider->cone_.radius,
                collider->cone_.halfHeight,
                collider->owner_->rot,
                col);
            break;
          case ShapeType::Pyramid:
            if (!isSleepingBody) col = color_t{0xB0, 0xFF, 0x40, 0xFF};
            Debug::drawPyramid(
                collider->worldCenter_,
                collider->pyramid_.baseHalfWidthX,
                collider->pyramid_.baseHalfWidthZ,
                collider->pyramid_.halfHeight,
                collider->owner_->rot,
                col);
            break;
          default:
            break;
          }
        }
      }
    }
  }

} // namespace P64::Coll
