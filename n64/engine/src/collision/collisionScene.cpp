/**
 * @file collision_scene.cpp
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Defines the Collision Scene which keeps track of physics participants and updates them (see collisionScene.h)
 */
#include "collision/collisionScene.h"
#include "collision/collide.h"
#include "collision/contactUtils.h"
#include "collision/gfxScale.h"
#include "collision/gjk.h"
#include "scene/scene.h"

#include <cmath>
#include <cassert>
#include <cinttypes>
#include <functional>
#include <algorithm>
#include <limits>
#include <utility>

#include "debug/debugDraw.h"

namespace P64::Coll {

  static CollisionScene g_scene;

  static bool canApplyAngularResponse(const RigidBody *body) {
    return body && body->canApplyAngularResponse();
  }

  static float constrainedLinearInvMassAlong(const RigidBody *body, const fm_vec3_t &direction) {
    return body ? body->constrainedLinearInvMassAlong(direction) : 0.0f;
  }

  // object owning the "self" side of a constraint for the given event direction
  // (forward: A is self, mirrored: B is self)
  static Object *constraintSideObject(const ContactConstraint &constraint, bool mirrored) {
    const Collider *selfCollider = mirrored ? constraint.colliderB : constraint.colliderA;
    if(selfCollider) return selfCollider->ownerObject();
    const MeshCollider *selfMeshCollider = mirrored ? constraint.meshColliderB : constraint.meshColliderA;
    if(selfMeshCollider) return selfMeshCollider->ownerObject();
    return nullptr;
  }

  // whether the "self" side of a constraint reads the "hit" side for the given event direction
  static bool constraintMasksOverlap(const ContactConstraint &constraint, bool mirrored) {
    if(constraint.rigidBodyA && !constraint.rigidBodyA->isEnabled()) return false;
    if(constraint.rigidBodyB && !constraint.rigidBodyB->isEnabled()) return false;

    const Collider *selfCollider = mirrored ? constraint.colliderB : constraint.colliderA;
    const Collider *hitCollider = mirrored ? constraint.colliderA : constraint.colliderB;
    const MeshCollider *selfMeshCollider = mirrored ? constraint.meshColliderB : constraint.meshColliderA;
    const MeshCollider *hitMeshCollider = mirrored ? constraint.meshColliderA : constraint.meshColliderB;

    if(selfCollider) {
      if(hitCollider) return selfCollider->readsCollider(hitCollider);
      if(hitMeshCollider) return selfCollider->readsMeshCollider(hitMeshCollider);
      return false;
    }

    if(selfMeshCollider) {
      if(hitCollider) return selfMeshCollider->readsCollider(hitCollider);
    }

    return false;
  }

  static std::pair<const Object *, const Object *> makeObjectPairKey(const Object *objectA, const Object *objectB) {
    if(std::less<const Object *>{}(objectB, objectA)) {
      std::swap(objectA, objectB);
    }
    return {objectA, objectB};
  }

  // build collision event for one direction of a constraint in place, avoiding intermediate event copies
  static void fillCollisionEvent(CollEvent &event, const ContactConstraint &constraint, bool mirrored) {
    if(mirrored) {
      event.selfCollider = constraint.colliderB;
      event.hitCollider = constraint.colliderA;
      event.selfMeshCollider = constraint.meshColliderB;
      event.hitMeshCollider = constraint.meshColliderA;
      event.selfRigidBody = constraint.rigidBodyB;
      event.hitRigidBody = constraint.rigidBodyA;
      event.otherObject = constraintSideObject(constraint, false);
    } else {
      event.selfCollider = constraint.colliderA;
      event.hitCollider = constraint.colliderB;
      event.selfMeshCollider = constraint.meshColliderA;
      event.hitMeshCollider = constraint.meshColliderB;
      event.selfRigidBody = constraint.rigidBodyA;
      event.hitRigidBody = constraint.rigidBodyB;
      event.otherObject = constraint.objectB;
    }
    event.contactCount = static_cast<uint16_t>(std::min(constraint.pointCount, MAX_CONTACT_POINTS_PER_PAIR));

    for(uint16_t i = 0; i < event.contactCount; ++i) {
      event.contacts[i] = constraint.points[i];
      if(mirrored) {
        std::swap(event.contacts[i].contactA, event.contacts[i].contactB);
        std::swap(event.contacts[i].localPointA, event.contacts[i].localPointB);
        std::swap(event.contacts[i].aToContact, event.contacts[i].bToContact);
      }
    }
  }

  bool CollisionScene::shouldTrackSleepState(const RigidBody *rigidBody) {
    return rigidBody && rigidBody->isEnabled_ && !rigidBody->isKinematic_;
  }

  bool CollisionScene::rigidBodyVelocitiesExceededSleepThreshold(const RigidBody *rigidBody) {
    if(!shouldTrackSleepState(rigidBody)) return false;

    const float speedSq = fm_vec3_len2(&rigidBody->linearVelocity_);
    if(speedSq > SPEED_SLEEP_THRESHOLD_SQ) return true;

    const float angSpeedSq = fm_vec3_len2(&rigidBody->angularVelocity_);
    return angSpeedSq > ANGULAR_SLEEP_THRESHOLD_SQ;
  }

  bool CollisionScene::rigidBodyTransformExceededSleepThreshold(const RigidBody *rigidBody) {
    if(!shouldTrackSleepState(rigidBody)) return false;

    const float posDeltaSq = fm_vec3_distance2(&rigidBody->position_, &rigidBody->previousStepPosition_);
    if(posDeltaSq > POS_SLEEP_THRESHOLD_SQ) return true;

    const float rotSim = fabsf(quatDot(rigidBody->rotation_, rigidBody->previousStepRotation_));
    if(rotSim < ROT_SIMILARITY_SLEEP_THRESHOLD) return true;

    if(rigidBody->owner_) {
      const float scaleDeltaSq = fm_vec3_distance2(&rigidBody->owner_->scale, &rigidBody->previousStepScale_);
      if(scaleDeltaSq > POS_SLEEP_THRESHOLD_SQ) return true;
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
    meshColliderAABBTree.destroy();

    // clear cached cross links so participants that outlive the scene don't dangle
    for(Collider *collider : colliders_) {
      if(collider) collider->rigidBody_ = nullptr;
    }
    for(RigidBody *body : rigidBodies_) {
      if(body) body->attachedColliders_.clear();
    }

    rigidBodies_.clear();
    ownerRigidBodies_.clear();
    colliders_.clear();
    ownerColliders_.clear();
    meshColliders_.clear();
    cachedConstraintCount_ = 0;
    cachedConstraints_.clear();
    cachedConstraintLookup_.clear();
    solverConstraints_.clear();
    solverBodies_.clear();
    solverHeaders_.clear();
    solverPoints_.clear();
    solverFrictionHeaders_.clear();
    solverFrictionPoints_.clear();
    solverOrder_.clear();
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
    meshColliderAABBTree.init(32);
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
    if(!rigidBody || !rigidBody->owner_) return;

    const fm_vec3_t fallbackInertia = rigidBody->getDefaultLocalInertiaTensor();

    const std::vector<Collider *> *ownerColliders = findCollidersForOwner(rigidBody->owner_);
    if(!ownerColliders || ownerColliders->empty()) {
      rigidBody->applyCompoundProperties(rigidBody->localCenterOfMassOffset_, fallbackInertia, rigidBody->owner_->scale);
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
      rigidBody->applyCompoundProperties(rigidBody->localCenterOfMassOffset_, fallbackInertia, rigidBody->owner_->scale);
      return;
    }

    const float invCount = 1.0f / static_cast<float>(count);
    const fm_vec3_t worldCenter = (worldCenterSum * invCount) + (rigidBody->rotation_ * rigidBody->localCenterOfMassOffset_);

    fm_vec3_t localCenterOfMass = worldCenter - rigidBody->position_;
    localCenterOfMass = quatConjugate(rigidBody->rotation_) * localCenterOfMass;

    if(rigidBody->getMass() <= FM_EPSILON) {
      rigidBody->applyCompoundProperties(localCenterOfMass, fallbackInertia, rigidBody->owner_->scale);
      return;
    }

    const float massPerCollider = rigidBody->getMass() * invCount;
    fm_vec3_t compoundInertia = VEC3_ZERO;

    for(Collider *collider : *ownerColliders) {
      if(!collider) continue;

      fm_vec3_t colliderInertia = collider->inertiaTensor(massPerCollider);
      fm_vec3_t r = collider->worldCenter_ - worldCenter;
      r = quatConjugate(rigidBody->rotation_) * r;

      const float x2 = r.x * r.x;
      const float y2 = r.y * r.y;
      const float z2 = r.z * r.z;

      colliderInertia.x += massPerCollider * (y2 + z2);
      colliderInertia.y += massPerCollider * (x2 + z2);
      colliderInertia.z += massPerCollider * (x2 + y2);

      compoundInertia = compoundInertia + colliderInertia;
    }

    rigidBody->applyCompoundProperties(localCenterOfMass, compoundInertia, rigidBody->owner_->scale);
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

    // Link colliders that were registered for this owner before the body existed
    rigidBody->attachedColliders_.clear();
    auto ownerIt = ownerColliders_.find(rigidBody->owner_);
    if(ownerIt != ownerColliders_.end()) {
      rigidBody->attachedColliders_ = ownerIt->second;
      for(Collider *collider : rigidBody->attachedColliders_) {
        if(collider) collider->rigidBody_ = rigidBody;
      }
    }

    rigidBody->markCompoundPropertiesDirty();
    syncCompoundProperties(rigidBody);
    const fm_vec3_t worldPos = rigidBody->position_;
    rigidBody->worldAabb_ = AABB{worldPos, worldPos};
  }

  void CollisionScene::removeRigidBody(RigidBody *rigidBody) {
    if(!rigidBody) return;

    disableRigidBody(rigidBody);

    for(Collider *collider : rigidBody->attachedColliders_) {
      if(collider && collider->rigidBody_ == rigidBody) collider->rigidBody_ = nullptr;
    }
    rigidBody->attachedColliders_.clear();

    if(rigidBody->owner_) {
      auto ownerIt = ownerRigidBodies_.find(rigidBody->owner_);
      if(ownerIt != ownerRigidBodies_.end() && ownerIt->second == rigidBody) {
        ownerRigidBodies_.erase(ownerIt);
      }
    }

    rigidBodies_.erase(std::remove(rigidBodies_.begin(), rigidBodies_.end(), rigidBody), rigidBodies_.end());
  }

  void CollisionScene::enableRigidBody(RigidBody *rigidBody) {
    if(!rigidBody || rigidBody->isEnabled_) return;
    rigidBody->enable();
  }

  void CollisionScene::disableRigidBody(RigidBody* rigidBody) {
    if(!rigidBody || !rigidBody->isEnabled_) return;

    std::vector<RigidBody *> wakeCandidates;

    removeCachedConstraints([rigidBody](const ContactConstraint &cc) {
      return cc.rigidBodyA == rigidBody || cc.rigidBodyB == rigidBody;
    }, wakeCandidates, rigidBody);

    // Sleeping rigidBodies overlapping the disabled body are likely support-dependent and should re-evaluate.
    const AABB removedBounds = rigidBody->worldAabb_;

    for(RigidBody *body : rigidBodies_) {
      if(!body || body == rigidBody) continue;
      if(aabbOverlap(body->worldAabb_, removedBounds)) {
        addWakeCandidate(wakeCandidates, body, rigidBody);
      }
    }

    wakeCandidateIslands(wakeCandidates);
    rigidBody->disable();
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
    collider->rigidBody_ = rigidBody;
    if (rigidBody)
    {
      rigidBody->attachedColliders_.push_back(collider);
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

    RigidBody *rigidBody = collider->rigidBody_;
    collider->rigidBody_ = nullptr;
    if(rigidBody) {
      auto &attached = rigidBody->attachedColliders_;
      attached.erase(std::remove(attached.begin(), attached.end(), collider), attached.end());
      rigidBody->markCompoundPropertiesDirty();
      syncCompoundProperties(rigidBody);
    }
  }

  void CollisionScene::addMeshCollider(MeshCollider *mesh) {
    if(!mesh) return;

    mesh->computeLocalRootAabb();
    mesh->recalculateWorldAabb();
    mesh->syncOwnerTransform();

    meshColliders_.push_back(mesh);

    mesh->aabbTreeNodeId_ = meshColliderAABBTree.createNode(mesh->worldAabb_, mesh);
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

    if (mesh->aabbTreeNodeId_ != NULL_NODE) {
      meshColliderAABBTree.removeLeaf(mesh->aabbTreeNodeId_, true);
      mesh->aabbTreeNodeId_ = NULL_NODE;
    }
  }

  void CollisionScene::configureSimulation(float fixedDt, const fm_vec3_t &gravity, uint8_t velocityIterations, uint8_t positionIterations, float gfxScale) {
    fixedDt_ = fixedDt > 0.0f ? fixedDt : DEFAULT_FIXED_DT;
    setGfxScale(gfxScale);
    gravity_ = gravity;
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

  uint32_t CollisionScene::nextIslandEpoch() {
    if(++islandVisitEpoch_ == 0) {
      // counter wrapped: clear stale stamps so no body falsely reads as visited
      for(RigidBody *body : rigidBodies_) {
        if(body) body->islandVisitEpoch_ = 0;
      }
      islandVisitEpoch_ = 1;
    }
    return islandVisitEpoch_;
  }

  void CollisionScene::collectConnectedIsland(RigidBody *seed, std::vector<RigidBody *> &island) {
    if(!shouldTrackSleepState(seed)) return;

    const uint32_t epoch = nextIslandEpoch();
    std::vector<RigidBody *> &stack = islandStackScratch_;
    stack.clear();
    stack.push_back(seed);

    while(!stack.empty()) {
      RigidBody *current = stack.back();
      stack.pop_back();

      if(!shouldTrackSleepState(current)) continue;
      if(current->islandVisitEpoch_ == epoch) continue;
      current->islandVisitEpoch_ = epoch;
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
        if(other->islandVisitEpoch_ != epoch) {
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

  void CollisionScene::dispatchCollisionCallbacks() {
    pendingDispatchKeys_.clear();
    pendingDispatchScratch_.clear();

    for(int i = 0; i < cachedConstraintCount_; ++i) {
      const ContactConstraint &constraint = cachedConstraints_[i];
      if(!constraint.isActive || constraint.pointCount <= 0) continue;
      if(!constraint.objectA || !constraint.objectB) continue;
      if(!constraint.objectA->isEnabled() || !constraint.objectB->isEnabled()) continue;

      const auto key = makeObjectPairKey(constraint.objectA, constraint.objectB);

      // number of unique object pairs per step is small, so linear scan over list of pairs
      // likely beats hashmap lookup and allocation
      int32_t dispatchIndex = -1;
      bool seen = false;
      for(const PendingPairKey &pairKey : pendingDispatchKeys_) {
        if(pairKey.first == key.first && pairKey.second == key.second) {
          dispatchIndex = pairKey.dispatchIndex;
          seen = true;
          break;
        }
      }

      if(!seen) {
        // only pairs where at least one object actually has a collision callback get event storage
        // others are remembered as not-interested.
        const bool wantFirst = Scene::objectHasCollisionHandler(*key.first);
        const bool wantSecond = Scene::objectHasCollisionHandler(*key.second);
        if(wantFirst || wantSecond) {
          dispatchIndex = static_cast<int32_t>(pendingDispatchScratch_.size());
          pendingDispatchScratch_.push_back(PendingPairDispatch{});
          PendingPairDispatch &created = pendingDispatchScratch_.back();
          created.wantFirst = wantFirst;
          created.wantSecond = wantSecond;
        }
        pendingDispatchKeys_.push_back(PendingPairKey{key.first, key.second, dispatchIndex});
      }

      if(dispatchIndex < 0) continue; // neither object listens for collisions

      PendingPairDispatch &dispatch = pendingDispatchScratch_[dispatchIndex];
      if((dispatch.hasFirstEvent || !dispatch.wantFirst) &&
         (dispatch.hasSecondEvent || !dispatch.wantSecond)) {
        continue; // both directions already captured by an earlier constraint of this pair
      }

      for(int direction = 0; direction < 2; ++direction) {
        const bool mirrored = direction == 1;
        Object *selfObject = constraintSideObject(constraint, mirrored);
        if(!selfObject) continue;

        bool isFirstSlot;
        if(selfObject == key.first) {
          isFirstSlot = true;
        } else if(selfObject == key.second) {
          isFirstSlot = false;
        } else {
          continue;
        }

        const bool want = isFirstSlot ? dispatch.wantFirst : dispatch.wantSecond;
        bool &hasEvent = isFirstSlot ? dispatch.hasFirstEvent : dispatch.hasSecondEvent;
        if(!want || hasEvent) continue;
        if(!constraintMasksOverlap(constraint, mirrored)) continue;

        CollEvent &event = isFirstSlot ? dispatch.firstEvent : dispatch.secondEvent;
        event = CollEvent{};
        fillCollisionEvent(event, constraint, mirrored);
        hasEvent = true;
      }
    }

    for(const PendingPairDispatch &dispatch : pendingDispatchScratch_) {
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

    std::vector<RigidBody *> &island = islandScratch_;
    island.clear();
    collectConnectedIsland(rigidBody, island);

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
      if(!body || !body->isEnabled_ || !body->isSleeping_) continue;
      if(!rigidBodyTransformExceededSleepThreshold(body)) continue;
      wakeCandidates.push_back(body);
    }

    for(RigidBody *body : wakeCandidates) {
      wakeIsland(body);
    }
  }

  void CollisionScene::updateSleepStates() {
    // One epoch for the whole pass: bodies claimed by an earlier island are skipped as seeds
    const uint32_t epoch = nextIslandEpoch();

    for(RigidBody *body : rigidBodies_) {
      if(!shouldTrackSleepState(body) || body->isSleeping_) continue;
      if(body->islandVisitEpoch_ == epoch) continue;

      // Build the island of AWAKE, connected bodies only.
      // They are only woken explicitly when a new dynamic contact forces it.
      std::vector<RigidBody *> &island = islandScratch_;
      island.clear();
      {
        std::vector<RigidBody *> &stack = islandStackScratch_;
        stack.clear();
        stack.push_back(body);
        while(!stack.empty()) {
          RigidBody *current = stack.back();
          stack.pop_back();

          if(!shouldTrackSleepState(current)) continue;
          if(current->isSleeping_) continue; // skip sleeping bodies
          if(current->islandVisitEpoch_ == epoch) continue;
          current->islandVisitEpoch_ = epoch;
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
            if(other->islandVisitEpoch_ != epoch) {
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

        // Deactivate if too separated; up to the breaking separation the point stays
        // active as a speculative contact (see preSolveContacts)
        if(cp.penetration < -CONTACT_BREAKING_SEPARATION) {
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
    std::vector<NodeProxy> &candidates = colliderCandidateScratch_;
    candidates.resize(colliders_.size());

    std::vector<NodeProxy> &meshCandidates = meshCandidateScratch_;
    meshCandidates.resize(meshColliders_.size());

    for(RigidBody *body : rigidBodies_) {
      if(!body || !body->isEnabled_ || body->isSleeping_ || body->isKinematic_) continue;

      const float dt = fixedDt_ * body->timeScale_;
      if(dt <= 0.0f) continue;

      const fm_vec3_t displacement = body->linearVelocity_ * dt;

      const std::vector<Collider *> *ownerColliders = &body->attachedColliders_;
      if(ownerColliders->empty()) continue;

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

      const fm_vec3_t originalPos = body->position_;
      bool hit = false;

      // Test at intermediate substep positions along the predicted trajectory.
      // k=0 is the current position (handled by normal detection afterwards).
      for(int k = 1; k < substeps; ++k) {
        const float fraction = static_cast<float>(k) / static_cast<float>(substeps);
        body->position_ = originalPos + (displacement * fraction);

        // Sync this body's colliders at the substep position
        for(Collider *collider : *ownerColliders) {
          if(!collider) continue;
          const fm_vec3_t prevCenter = collider->worldCenter_;
          if(!collider->syncFromRigidBody(body->position_, body->rotation_)) continue;
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

            RigidBody *rbB = collB->rigidBody_;
            if(collideDetectObjectToObject(collider, body, collB, rbB, false)) {
              debugf("CCD substep %d/%d: body %u hit body %u", k, substeps, collider->owner_->id, collB->owner_->id);
              hit = true;
              break;
            }
          }
          if (hit) break;
        }
        if (hit) break;

        // Broadphase + narrowphase against mesh colliders
        for(Collider *collider : *ownerColliders) {
          if(!collider || !collider->owner_) continue;
          const int meshCandidateCount = meshColliderAABBTree.queryBounds(
             collider->worldAabb_,
             meshCandidates.data(),
             static_cast<int>(meshCandidates.size()));
          for(int m = 0; m < meshCandidateCount; ++m) {
            void *data = meshColliderAABBTree.getNodeData(meshCandidates[m]);
            if (!data) continue;
            MeshCollider* mesh = static_cast<MeshCollider *>(data);
            if(!mesh || mesh->triangleCount_ <= 0) continue;
            if(!collider->readsMeshCollider(mesh) && !mesh->readsCollider(collider)) continue;
            if(collideDetectObjectToMesh(collider, body, *mesh, false)) {
              debugf("CCD substep %d/%d: body %u hit mesh %u", k, substeps, collider->owner_->id, mesh->owner_ ? static_cast<unsigned>(mesh->owner_->id) : 0u);
              hit = true;
              break;
            }
          }
          if (hit) break;
        }
        if (hit) break;
      }

      if (!hit) {
        // Restore original position and sync colliders back
        body->position_ = originalPos;
        for(Collider *collider : *ownerColliders) {
          if(!collider) continue;
          const fm_vec3_t prevCenter = collider->worldCenter_;
          if(!collider->syncFromRigidBody(body->position_, body->rotation_)) continue;
          const fm_vec3_t disp = collider->worldCenter_ - prevCenter;
          if(collider->aabbTreeNodeId_ != NULL_NODE) {
            colliderAABBTree.moveNode(collider->aabbTreeNodeId_, collider->worldAabb_, disp);
          }
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

    //list of candidate colliders for broad phase query results
    std::vector<NodeProxy> &candidateColliders = colliderCandidateScratch_;
    candidateColliders.resize(colliders_.size());

    const uint64_t bodyDetectStart = get_ticks();
    for (Collider *collider : colliders_)
    {
      if (!collider || !collider->owner_)
        continue;
      if (collider->isTrigger_)
        continue;

      RigidBody *rbA = collider->rigidBody_;

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

        // don't let collider collide with itself or other colliders on the same object
        if (collB == collider)
          continue;
        if (collB->owner_ && collider->owner_ == collB->owner_)
          continue;

        // overlapping pair is always discovered from both directions, because each
        // leafs fattened bounds contain its tight bounds. Ordering by tree node id therefore
        // tests each pair exactly once without needing a tested-pairs set
        if (!collB->isTrigger_ && collider->aabbTreeNodeId_ > collB->aabbTreeNodeId_)
          continue;

        if (!collider->readsCollider(collB) && !collB->readsCollider(collider))
          continue;
        RigidBody *rbB = collB->rigidBody_;
        if((rbA && rbA->isSleeping_) && (rbB && rbB->isSleeping_)) {
          // Allow sleeping objects to generate contacts with triggers, but skip if both are sleeping non-triggers
          if(!collider->isTrigger_ && !collB->isTrigger_) {
            continue;
          }
        }
        collideDetectObjectToObject(collider, rbA, collB, rbB, true);
      }
    }
    ticksDetectBodyPairs = get_ticks() - bodyDetectStart;

    std::vector<NodeProxy> &candidateMeshColliders = meshCandidateScratch_;
    candidateMeshColliders.resize(meshColliders_.size());

    const uint64_t meshDetectStart = get_ticks();
    for (Collider *collider : colliders_) {
      if (!collider || !collider->owner_) continue;

      RigidBody *rigidBodyA = collider->rigidBody_;

      if (!collider->isTrigger_ && rigidBodyA && rigidBodyA->isSleeping_) continue;

      const int candidateCount = meshColliderAABBTree.queryBounds(
          collider->worldAabb_,
          candidateMeshColliders.data(),
          static_cast<int>(candidateMeshColliders.size()));

      for (int candidateIdx = 0; candidateIdx < candidateCount; ++candidateIdx) {
        void *data = meshColliderAABBTree.getNodeData(candidateMeshColliders[candidateIdx]);
        if (!data) continue;

        MeshCollider *meshA = static_cast<MeshCollider *>(data);
        if (!meshA || meshA->triangleCount_ <= 0) continue;

        if (!meshA->readsCollider(collider) && !collider->readsMeshCollider(meshA)) continue;

        collideDetectObjectToMesh(collider, rigidBodyA, *meshA, true);
      }
    }
    ticksDetectMeshPairs = get_ticks() - meshDetectStart;
    //TODO: possibly offer mesh-mesh collision detection in the future, but not needed for current use cases

    removeInactiveContacts();
  }

  // ── Pre-solve: build dense solver data ────────────────────────

  /// @brief Acquires an index for the given rigid body in the solver's body array
  /// @param body 
  /// @return index of the body in the solverBodies_ array, or 0 for a static sentinel body
  uint16_t CollisionScene::acquireSolverBodyIndex(RigidBody *body) {
    if(!body) return 0; // sentinel static body
    if(body->solverIndex_ >= 0) return static_cast<uint16_t>(body->solverIndex_);

    body->solverIndex_ = static_cast<int16_t>(solverBodies_.size());
    solverBodies_.push_back(SolverBody{});
    SolverBody &sb = solverBodies_.back();
    sb.body = body;
    sb.linearVelocity = body->linearVelocity_;
    sb.angularVelocity = body->angularVelocity_;
    return static_cast<uint16_t>(body->solverIndex_);
  }

  void CollisionScene::preSolveContacts() {
    const float restitutionSlop = 0.5f;
    const float invFixedDt = (fixedDt_ > 0.0f) ? 1.0f / fixedDt_ : 0.0f;
    
    // Position correction settings
    const float positionSlop = 0.005f; // leave a small slop (in meters) that objects can penetrate before correction is applied
    const float positionSteering = 0.8f; // push out this fraction of the rest of penetration each step
    const float maxPositionCorrection = 0.2f; // cap per step (in meters)

    solverBodies_.clear();
    solverHeaders_.clear();
    solverPoints_.clear();
    solverFrictionHeaders_.clear();
    solverFrictionPoints_.clear();
    solverOrder_.clear();

    solverBodies_.push_back(SolverBody{}); // index 0: always immovable sentinel

    // Reset solver indices on all bodies
    for(RigidBody *body : rigidBodies_) {
      if(body) body->solverIndex_ = -1; 
    }

    for(ContactConstraint *constraint : solverConstraints_) {
      ContactConstraint &cc = *constraint;

      RigidBody *a = cc.rigidBodyA;
      RigidBody *b = cc.rigidBodyB;
      const bool aHasMotionAngular = canApplyAngularResponse(a);
      const bool bHasMotionAngular = canApplyAngularResponse(b);
      const bool aCanRotate = cc.respondsA && aHasMotionAngular;
      const bool bCanRotate = cc.respondsB && bHasMotionAngular;
      const bool aRespondsLinear = cc.respondsA && a && a->isEnabled_ && !a->isKinematic_;
      const bool bRespondsLinear = cc.respondsB && b && b->isEnabled_ && !b->isKinematic_;

      const float invMassA = cc.respondsA ? constrainedLinearInvMassAlong(a, cc.normal) : 0.0f;
      const float invMassB = cc.respondsB ? constrainedLinearInvMassAlong(b, cc.normal) : 0.0f;
      const float totalInvMass = invMassA + invMassB;
      // constraint only drops out when neither side has a linear nor an angular way to respond
      if(totalInvMass < FM_EPSILON && !aCanRotate && !bCanRotate) continue;

      // Tangent effective masses for friction
      const float linearU = (cc.respondsA ? constrainedLinearInvMassAlong(a, cc.tangentU) : 0.0f) +
                (cc.respondsB ? constrainedLinearInvMassAlong(b, cc.tangentU) : 0.0f);
      const float linearV = (cc.respondsA ? constrainedLinearInvMassAlong(a, cc.tangentV) : 0.0f) +
                (cc.respondsB ? constrainedLinearInvMassAlong(b, cc.tangentV) : 0.0f);

      const uint16_t headerIndex = static_cast<uint16_t>(solverHeaders_.size());
      solverHeaders_.push_back(SolverConstraintHeader{});
      solverFrictionHeaders_.push_back(SolverFrictionHeader{});
      SolverConstraintHeader &h = solverHeaders_[headerIndex];
      SolverFrictionHeader &fh = solverFrictionHeaders_[headerIndex];

      h.normal = cc.normal;
      h.bodyA = acquireSolverBodyIndex(a);
      h.bodyB = acquireSolverBodyIndex(b);
      h.pointStart = static_cast<uint16_t>(solverPoints_.size());

      // Linear response per unit impulse, already constrained and respecting B's sign
      if(aRespondsLinear) h.linearResponseA = a->constrainLinearWorld(cc.normal * a->inverseMass_);
      if(bRespondsLinear) h.linearResponseB = b->constrainLinearWorld(cc.normal * -b->inverseMass_);

      // Building friction header
      fh.tangentU = cc.tangentU;
      fh.tangentV = cc.tangentV;
      fh.friction = cc.combinedFriction;
      // Friction measures only enabled, non-kinematic bodies
      fh.linearMeasureScaleA = (a && a->isEnabled_ && !a->isKinematic_) ? 1.0f : 0.0f;
      fh.linearMeasureScaleB = (b && b->isEnabled_ && !b->isKinematic_) ? 1.0f : 0.0f;
      if(aRespondsLinear) {
        fh.linearResponseUA = a->constrainLinearWorld(cc.tangentU * a->inverseMass_);
        fh.linearResponseVA = a->constrainLinearWorld(cc.tangentV * a->inverseMass_);
      }
      if(bRespondsLinear) {
        fh.linearResponseUB = b->constrainLinearWorld(cc.tangentU * -b->inverseMass_);
        fh.linearResponseVB = b->constrainLinearWorld(cc.tangentV * -b->inverseMass_);
      }

      // Precompute solver points for each contact point in the constraint
      for(int j = 0; j < cc.pointCount; ++j) {
        ContactPoint &cp = cc.points[j];
        if(!cp.active) continue;

        // Relative vectors from centers of mass (also consumed by collision events)
        cp.aToContact = a ? cp.contactA - a->worldCenterOfMass() : VEC3_ZERO;
        cp.bToContact = b ? cp.contactB - b->worldCenterOfMass() : VEC3_ZERO;

        // Normal effective mass: 1 / (invMassA + invMassB + (rA×n)·I_A^-1·(rA×n) + ...)
        fm_vec3_t raCrossN;
        fm_vec3_cross(&raCrossN, &cp.aToContact, &cc.normal);
        fm_vec3_t rbCrossN;
        fm_vec3_cross(&rbCrossN, &cp.bToContact, &cc.normal);

        fm_vec3_t angularResponseA = VEC3_ZERO;
        fm_vec3_t angularResponseB = VEC3_ZERO;
        float angularA = 0.0f;
        if(aCanRotate) {
          angularResponseA = a->applyConstrainedWorldInertia(raCrossN);
          angularA = fm_vec3_dot(&raCrossN, &angularResponseA);
        }
        float angularB = 0.0f;
        if(bCanRotate) {
          fm_vec3_t inertia = b->applyConstrainedWorldInertia(rbCrossN);
          angularB = fm_vec3_dot(&rbCrossN, &inertia);
          angularResponseB = -inertia;
        }

        // Skip points nothing can respond to
        const float denomN = totalInvMass + angularA + angularB;
        if(denomN < FM_EPSILON) continue;

        solverPoints_.push_back(SolverContactPoint{});
        solverFrictionPoints_.push_back(SolverFrictionPoint{});
        SolverContactPoint &sp = solverPoints_.back();
        SolverFrictionPoint &fp = solverFrictionPoints_.back();
        fp.source = &cp;

        sp.angularResponseA = angularResponseA;
        sp.angularResponseB = angularResponseB;
        if(aHasMotionAngular) sp.angularMeasureA = raCrossN;
        if(bHasMotionAngular) sp.angularMeasureB = rbCrossN;

        sp.normalMass = 1.0f / denomN;
        sp.accumulatedImpulse = cp.accumulatedNormalImpulse;

        // Tangent effective masses
        {
          fm_vec3_t raCrossU;
          fm_vec3_cross(&raCrossU, &cp.aToContact, &cc.tangentU);
          fm_vec3_t rbCrossU;
          fm_vec3_cross(&rbCrossU, &cp.bToContact, &cc.tangentU);
          float angU_A = 0.0f;
          if(aCanRotate) {
            fp.angularResponseUA = a->applyConstrainedWorldInertia(raCrossU);
            angU_A = fm_vec3_dot(&raCrossU, &fp.angularResponseUA);
          }
          float angU_B = 0.0f;
          if(bCanRotate) {
            fm_vec3_t inertia = b->applyConstrainedWorldInertia(rbCrossU);
            angU_B = fm_vec3_dot(&rbCrossU, &inertia);
            fp.angularResponseUB = -inertia;
          }
          if(fh.linearMeasureScaleA > 0.0f && aHasMotionAngular) fp.angularMeasureUA = raCrossU;
          if(fh.linearMeasureScaleB > 0.0f && bHasMotionAngular) fp.angularMeasureUB = rbCrossU;
          float denomU = linearU + angU_A + angU_B;
          if(denomU < FM_EPSILON) denomU = FM_EPSILON;
          fp.tangentMassU = 1.0f / denomU;
        }
        {
          fm_vec3_t raCrossV;
          fm_vec3_cross(&raCrossV, &cp.aToContact, &cc.tangentV);
          fm_vec3_t rbCrossV;
          fm_vec3_cross(&rbCrossV, &cp.bToContact, &cc.tangentV);
          float angV_A = 0.0f;
          if(aCanRotate) {
            fp.angularResponseVA = a->applyConstrainedWorldInertia(raCrossV);
            angV_A = fm_vec3_dot(&raCrossV, &fp.angularResponseVA);
          }
          float angV_B = 0.0f;
          if(bCanRotate) {
            fm_vec3_t inertia = b->applyConstrainedWorldInertia(rbCrossV);
            angV_B = fm_vec3_dot(&rbCrossV, &inertia);
            fp.angularResponseVB = -inertia;
          }
          if(fh.linearMeasureScaleA > 0.0f && aHasMotionAngular) fp.angularMeasureVA = raCrossV;
          if(fh.linearMeasureScaleB > 0.0f && bHasMotionAngular) fp.angularMeasureVB = rbCrossV;
          float denomV = linearV + angV_A + angV_B;
          if(denomV < FM_EPSILON) denomV = FM_EPSILON;
          fp.tangentMassV = 1.0f / denomV;
        }
        fp.accumulatedImpulseU = cp.accumulatedTangentImpulseU;
        fp.accumulatedImpulseV = cp.accumulatedTangentImpulseV;

        // Restitution bias
        sp.velocityBias = 0.0f;
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
        const float relVelN = fm_vec3_dot(&relVel, &cc.normal);
        if(relVelN < -restitutionSlop) {
          sp.velocityBias += cc.combinedBounce * relVelN;
        }

        // Speculative contact: a separated manifold point may approach at up to
        // gap/dt before normal impulses fire. This keeps grazing corners of a
        // resting manifold active (stable warm starting) without blocking bodies
        // from settling into full contact.
        if(cp.penetration < 0.0f) {
          sp.velocityBias += -cp.penetration * invFixedDt;
        }

        // Split impulse target: correct most of the remaining penetration this step
        if(cp.penetration > positionSlop) {
          sp.positionBias = fminf(positionSteering * (cp.penetration - positionSlop), maxPositionCorrection);
        }
      }

      h.pointCount = static_cast<uint16_t>(solverPoints_.size() - h.pointStart);
      if(h.pointCount == 0) {
        // every point was skipped, drop the constraint again
        solverHeaders_.pop_back();
        solverFrictionHeaders_.pop_back();
        continue;
      }
      solverOrder_.push_back(headerIndex);
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

    for(std::size_t c = 0; c < solverHeaders_.size(); ++c) {
      const SolverConstraintHeader &h = solverHeaders_[c];
      const SolverFrictionHeader &fh = solverFrictionHeaders_[c];
      SolverBody &bodyA = solverBodies_[h.bodyA];
      SolverBody &bodyB = solverBodies_[h.bodyB];

      const uint16_t end = h.pointStart + h.pointCount;
      for(uint16_t k = h.pointStart; k < end; ++k) {
        SolverContactPoint &sp = solverPoints_[k];
        SolverFrictionPoint &fp = solverFrictionPoints_[k];

        // Scale accumulated impulses by warm starting factor (Bullet's m_warmstartingFactor = 0.85)
        // This prevents overcorrection when constraint configuration changes between frames
        sp.accumulatedImpulse *= WARM_STARTING_FACTOR;
        fp.accumulatedImpulseU *= WARM_STARTING_FACTOR;
        fp.accumulatedImpulseV *= WARM_STARTING_FACTOR;

        // Zero out decayed impulses to prevent persistent micro-impulses that can cause jitter and prevent sleeping
        if(fabsf(sp.accumulatedImpulse) < IMPULSE_ZERO_THRESHOLD) sp.accumulatedImpulse = 0.0f;
        if(fabsf(fp.accumulatedImpulseU) < IMPULSE_ZERO_THRESHOLD) fp.accumulatedImpulseU = 0.0f;
        if(fabsf(fp.accumulatedImpulseV) < IMPULSE_ZERO_THRESHOLD) fp.accumulatedImpulseV = 0.0f;

        bodyA.linearVelocity += h.linearResponseA * sp.accumulatedImpulse
                              + fh.linearResponseUA * fp.accumulatedImpulseU
                              + fh.linearResponseVA * fp.accumulatedImpulseV;
        bodyA.angularVelocity += sp.angularResponseA * sp.accumulatedImpulse
                               + fp.angularResponseUA * fp.accumulatedImpulseU
                               + fp.angularResponseVA * fp.accumulatedImpulseV;
        bodyB.linearVelocity += h.linearResponseB * sp.accumulatedImpulse
                              + fh.linearResponseUB * fp.accumulatedImpulseU
                              + fh.linearResponseVB * fp.accumulatedImpulseV;
        bodyB.angularVelocity += sp.angularResponseB * sp.accumulatedImpulse
                               + fp.angularResponseUB * fp.accumulatedImpulseU
                               + fp.angularResponseVB * fp.accumulatedImpulseV;
      }
    }
  }

  // ── Velocity constraint solver ────────────────────────────────────

  /// Fast xor shift pseudo RNG for constraint randomization
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
    const std::size_t constraintCount = solverOrder_.size();
    if(constraintCount == 0) return;

    constexpr float VELOCITY_SOLVER_EARLY_OUT_THRESHOLD = 1e-4f;
    constexpr float VELOCITY_SOLVER_NORMAL_ERROR_THRESHOLD = 1e-3f;
    constexpr uint8_t MIN_NORMAL_SOLVER_ITERATIONS = 4;

    // Shuffle constraint processing order once per step (Bullet's SOLVER_RANDMIZE_ORDER):
    // prevents bias where one constraint always "wins" in Gauss-Seidel iteration.
    for(std::size_t i = constraintCount; i > 1; --i) {
      const std::size_t j = static_cast<std::size_t>((static_cast<uint64_t>(solverRand()) * i) >> 32);
      std::swap(solverOrder_[i - 1], solverOrder_[j]);
    }

    for(uint8_t iter = 0; iter < velocitySolverIterations_; ++iter) {
      float maxNormalImpulseDelta = 0.0f;
      float maxNormalError = 0.0f;

      for(uint16_t orderIdx : solverOrder_) {
        const SolverConstraintHeader &h = solverHeaders_[orderIdx];
        SolverBody &bodyA = solverBodies_[h.bodyA];
        SolverBody &bodyB = solverBodies_[h.bodyB];

        const uint16_t end = h.pointStart + h.pointCount;
        for(uint16_t k = h.pointStart; k < end; ++k) {
          SolverContactPoint &sp = solverPoints_[k];

          // Relative normal velocity via precomputed r×n vectors
          const fm_vec3_t dv = bodyA.linearVelocity - bodyB.linearVelocity;
          const float relVelN = fm_vec3_dot(&dv, &h.normal)
                              + fm_vec3_dot(&bodyA.angularVelocity, &sp.angularMeasureA)
                              - fm_vec3_dot(&bodyB.angularVelocity, &sp.angularMeasureB);

          maxNormalError = fmaxf(maxNormalError, fmaxf(-(relVelN + sp.velocityBias), 0.0f));

          float dImpulseN = sp.normalMass * (-(relVelN + sp.velocityBias));

          // Clamp accumulated impulse (normal must be non-negative).
          const float oldAccum = sp.accumulatedImpulse;
          sp.accumulatedImpulse = fmaxf(oldAccum + dImpulseN, 0.0f);
          dImpulseN = sp.accumulatedImpulse - oldAccum;
          maxNormalImpulseDelta = fmaxf(maxNormalImpulseDelta, fabsf(dImpulseN));

          if(fabsf(dImpulseN) > FM_EPSILON) {
            bodyA.linearVelocity += h.linearResponseA * dImpulseN;
            bodyA.angularVelocity += sp.angularResponseA * dImpulseN;
            bodyB.linearVelocity += h.linearResponseB * dImpulseN;
            bodyB.angularVelocity += sp.angularResponseB * dImpulseN;
          }
        }
      }

      if(iter + 1 >= MIN_NORMAL_SOLVER_ITERATIONS &&
         maxNormalImpulseDelta < VELOCITY_SOLVER_EARLY_OUT_THRESHOLD &&
         maxNormalError < VELOCITY_SOLVER_NORMAL_ERROR_THRESHOLD) {
        break;
      }
    }

    // Friction: single pass after the normal impulses have converged
    for(uint16_t orderIdx : solverOrder_) {
      const SolverConstraintHeader &h = solverHeaders_[orderIdx];
      const SolverFrictionHeader &fh = solverFrictionHeaders_[orderIdx];
      if(fh.friction <= FM_EPSILON) continue;

      SolverBody &bodyA = solverBodies_[h.bodyA];
      SolverBody &bodyB = solverBodies_[h.bodyB];

      const uint16_t end = h.pointStart + h.pointCount;
      for(uint16_t k = h.pointStart; k < end; ++k) {
        SolverContactPoint &sp = solverPoints_[k];
        SolverFrictionPoint &fp = solverFrictionPoints_[k];

        const float vTangentU =
            fh.linearMeasureScaleA * fm_vec3_dot(&bodyA.linearVelocity, &fh.tangentU)
          - fh.linearMeasureScaleB * fm_vec3_dot(&bodyB.linearVelocity, &fh.tangentU)
          + fm_vec3_dot(&bodyA.angularVelocity, &fp.angularMeasureUA)
          - fm_vec3_dot(&bodyB.angularVelocity, &fp.angularMeasureUB);
        const float vTangentV =
            fh.linearMeasureScaleA * fm_vec3_dot(&bodyA.linearVelocity, &fh.tangentV)
          - fh.linearMeasureScaleB * fm_vec3_dot(&bodyB.linearVelocity, &fh.tangentV)
          + fm_vec3_dot(&bodyA.angularVelocity, &fp.angularMeasureVA)
          - fm_vec3_dot(&bodyB.angularVelocity, &fp.angularMeasureVB);

        float lambdaU = -vTangentU * fp.tangentMassU;
        float lambdaV = -vTangentV * fp.tangentMassV;

        float newAccumU = fp.accumulatedImpulseU + lambdaU;
        float newAccumV = fp.accumulatedImpulseV + lambdaV;

        // Clamp to the friction cone (tangentU ⊥ tangentV, both unit length)
        const float maxFriction = fh.friction * sp.accumulatedImpulse;
        const float tangentMagnitude = sqrtf(newAccumU * newAccumU + newAccumV * newAccumV);
        if(tangentMagnitude > maxFriction && tangentMagnitude > FM_EPSILON) {
          const float scale = maxFriction / tangentMagnitude;
          newAccumU *= scale;
          newAccumV *= scale;
        }

        lambdaU = newAccumU - fp.accumulatedImpulseU;
        lambdaV = newAccumV - fp.accumulatedImpulseV;
        fp.accumulatedImpulseU = newAccumU;
        fp.accumulatedImpulseV = newAccumV;

        if(lambdaU * lambdaU + lambdaV * lambdaV <= FM_EPSILON * FM_EPSILON) continue;

        bodyA.linearVelocity += fh.linearResponseUA * lambdaU + fh.linearResponseVA * lambdaV;
        bodyA.angularVelocity += fp.angularResponseUA * lambdaU + fp.angularResponseVA * lambdaV;
        bodyB.linearVelocity += fh.linearResponseUB * lambdaU + fh.linearResponseVB * lambdaV;
        bodyB.angularVelocity += fp.angularResponseUB * lambdaU + fp.angularResponseVB * lambdaV;
      }
    }

    // Write the results back to the bodies and the contact cache (for warm starting)
    for(std::size_t i = 1; i < solverBodies_.size(); ++i) {
      SolverBody &sb = solverBodies_[i];
      sb.body->linearVelocity_ = sb.linearVelocity;
      sb.body->angularVelocity_ = sb.angularVelocity;
    }
    for(std::size_t k = 0; k < solverPoints_.size(); ++k) {
      ContactPoint *cp = solverFrictionPoints_[k].source;
      cp->accumulatedNormalImpulse = solverPoints_[k].accumulatedImpulse;
      cp->accumulatedTangentImpulseU = solverFrictionPoints_[k].accumulatedImpulseU;
      cp->accumulatedTangentImpulseV = solverFrictionPoints_[k].accumulatedImpulseV;
    }
  }


  // ── Position constraint solver ────────────────────────────────────

  // Split impulse: penetration is pushed out with separate push velocities that reuse the velocity solver's masses and response vectors, 
  // then baked into each body's transform once at the end.
  void CollisionScene::solvePositionConstraints() {
    if(solverOrder_.empty()) return;

    bool anyCorrection = false;

    for(uint8_t iter = 0; iter < positionSolverIterations_; ++iter) {
      bool applied = false;

      for(uint16_t orderIdx : solverOrder_) {
        const SolverConstraintHeader &h = solverHeaders_[orderIdx];
        SolverBody &bodyA = solverBodies_[h.bodyA];
        SolverBody &bodyB = solverBodies_[h.bodyB];

        const uint16_t end = h.pointStart + h.pointCount;
        for(uint16_t k = h.pointStart; k < end; ++k) {
          SolverContactPoint &sp = solverPoints_[k];
          if(sp.positionBias <= 0.0f) continue;

          const fm_vec3_t dv = bodyA.pushLinearVelocity - bodyB.pushLinearVelocity;
          const float pushVelN = fm_vec3_dot(&dv, &h.normal)
                               + fm_vec3_dot(&bodyA.pushAngularVelocity, &sp.angularMeasureA)
                               - fm_vec3_dot(&bodyB.pushAngularVelocity, &sp.angularMeasureB);

          float dImpulse = sp.normalMass * (sp.positionBias - pushVelN);

          // Push impulses only separate (accumulated impulse must be non-negative)
          const float oldAccum = sp.accumulatedPushImpulse;
          sp.accumulatedPushImpulse = fmaxf(oldAccum + dImpulse, 0.0f);
          dImpulse = sp.accumulatedPushImpulse - oldAccum;
          if(fabsf(dImpulse) <= FM_EPSILON) continue;

          applied = true;
          bodyA.pushLinearVelocity += h.linearResponseA * dImpulse;
          bodyA.pushAngularVelocity += sp.angularResponseA * dImpulse;
          bodyB.pushLinearVelocity += h.linearResponseB * dImpulse;
          bodyB.pushAngularVelocity += sp.angularResponseB * dImpulse;
        }
      }

      anyCorrection |= applied;
      if(!applied) break;
    }

    if(!anyCorrection) return;

    // Apply the accumulated correction to position and rotation of each body at the end
    for(std::size_t i = 1; i < solverBodies_.size(); ++i) {
      SolverBody &sb = solverBodies_[i];
      RigidBody *body = sb.body;
      if(!vec3IsZero(sb.pushLinearVelocity)) {
        body->position_ += sb.pushLinearVelocity;
      }
      if(!vec3IsZero(sb.pushAngularVelocity)) {
        body->rotation_ = quatApplyAngularVelocity(body->rotation_, sb.pushAngularVelocity, 1.0f);
      }
    }
  }

  /// @brief Recalculate the world-space AABBs of all Mesh Colliders in the Collision Scene.
  void CollisionScene::updateMeshColliderWorldStates() {
    for(std::size_t i = 0; i < meshColliders_.size(); ++i) {
      MeshCollider *mesh = meshColliders_[i];
      if(!mesh) continue;

      mesh->transformChanged_ = mesh->hasOwnerTransformChanged();
      if(!mesh->transformChanged_ && mesh->hasCachedOwnerTransform_) continue;

      fm_vec3_t prevOwnerPhysicsPos = mesh->owner_ ? mesh->owner_->pos * getInvGfxScale() : VEC3_ZERO;

      mesh->recalculateWorldAabb();
      mesh->syncOwnerTransform();

      if (mesh->aabbTreeNodeId_ != NULL_NODE) {
        if (mesh->owner_) {
          fm_vec3_t ownerPhysicsPos = mesh->owner_->pos * getInvGfxScale();
          const fm_vec3_t disp = ownerPhysicsPos - prevOwnerPhysicsPos;
          meshColliderAABBTree.moveNode(mesh->aabbTreeNodeId_, mesh->worldAabb_, disp);
        } else {
          meshColliderAABBTree.moveNode(mesh->aabbTreeNodeId_, mesh->worldAabb_, VEC3_ZERO);
        }
      }
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
      NodeProxy meshCandidates[RAYCAST_MAX_COLLIDER_TESTS];
      int meshCount = meshColliderAABBTree.queryRay(ray, meshCandidates, RAYCAST_MAX_COLLIDER_TESTS);
      for(int m = 0; m < meshCount; ++m) {
        const MeshCollider* mesh = static_cast<const MeshCollider*>(meshColliderAABBTree.getNodeData(meshCandidates[m]));
        if(!mesh || mesh->triangleCount_ == 0) continue;
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
          currentHit.hitObjectId = mesh->owner_ ? mesh->owner_->id : 0;

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

  struct CapsuleGjkData {
    fm_vec3_t center;
    fm_vec3_t axis;
    float innerHalfHeight;
    float radius;
  };

  static void capsuleGjkSupport(const void *data, const fm_vec3_t &dir, fm_vec3_t &out) {
    const auto &c = *static_cast<const CapsuleGjkData*>(data);
    float proj = fm_vec3_dot(&dir, &c.axis);
    proj = fminf(fmaxf(proj, -c.innerHalfHeight), c.innerHalfHeight);
    fm_vec3_t segPt = c.center + c.axis * proj;
    const float len2 = fm_vec3_len2(&dir);
    if (len2 > FM_EPSILON * FM_EPSILON) {
      out = segPt + dir * (1.0f / sqrtf(len2) * c.radius);
    } else {
      out = segPt;
    }
  }

  struct SphereGjkData {
      fm_vec3_t center;
      float radius;
  };

  static void sphereGjkSupport(const void *data, const fm_vec3_t &dir, fm_vec3_t &out) {
    const auto &s = *static_cast<const SphereGjkData*>(data);
    const float len2 = fm_vec3_len2(&dir);
    if (len2 > FM_EPSILON * FM_EPSILON) {
      // Center + (Normalized Direction * Radius)
      out = s.center + dir * (1.0f / sqrtf(len2) * s.radius);
    } else {
      // Fallback if the direction vector is zero
      out = s.center;
    }
  }

  // World-space support for a Collider. The shape support functions return LOCAL-space points (centered at origin), 
  // so we rotate the query direction into local space, sample, then transform the result back to world space.
  static void colliderWorldGjkSupport(const void *data, const fm_vec3_t &dir, fm_vec3_t &out) {
    const Collider *coll = static_cast<const Collider *>(data);
    const fm_vec3_t localDir = coll->rotateToLocal(dir);
    const fm_vec3_t localSup = coll->support(localDir);
    out = coll->toWorldSpace(localSup);
  }

  bool CollisionScene::capsuleSweep(
    const fm_vec3_t& center,
    const fm_vec3_t& axisUp,
    float radius,
    float innerHalfHeight,
    const fm_vec3_t& displacement,
    RaycastColliderTypeFlags collTypes,
    uint8_t readMask,
    CapsuleSweepHit& hit,
    const Object* ignoreOwner
  ) const {
    hit = CapsuleSweepHit{};

    const bool doMesh    = hasFlag(collTypes, RaycastColliderTypeFlags::MESH_COLLIDERS);
    const bool doBodies  = hasFlag(collTypes, RaycastColliderTypeFlags::COLLIDER_BODIES);
    if (!doMesh && !doBodies) return false;

    // Compute world-space swept AABB for broadphase
    fm_vec3_t extents = {
      radius + fabsf(axisUp.x) * innerHalfHeight,
      radius + fabsf(axisUp.y) * innerHalfHeight,
      radius + fabsf(axisUp.z) * innerHalfHeight
    };
    AABB capsuleBox = { center - extents, center + extents };
    AABB sweptBox;
    aabbExtendDirection(capsuleBox, displacement, sweptBox);

    CapsuleSweepHit candidate{};

    // ── Mesh colliders ──────────────────────────────────────────────────────
    if (doMesh) {
      constexpr int MAX_TRI = 64;
      NodeProxy triCandidates[MAX_TRI];
      constexpr int MAX_MESH_CANDIDATES = 32;
      NodeProxy meshCandidates[MAX_MESH_CANDIDATES];

      int meshCount = meshColliderAABBTree.queryBounds(sweptBox, meshCandidates, MAX_MESH_CANDIDATES);
      for (int m = 0; m < meshCount; ++m) {
        const MeshCollider* mesh = static_cast<const MeshCollider*>(
          meshColliderAABBTree.getNodeData(meshCandidates[m]));
        if (!mesh || mesh->triangleCount() == 0 || !mesh->ownerObject()) continue;

        AABB localSweptBox = mesh->worldAabbToLocal(sweptBox);
        int triCount = mesh->queryTriangleNodes(localSweptBox, triCandidates, MAX_TRI);

        for (int i = 0; i < triCount; ++i) {
          int triIdx = mesh->triangleIndexForNode(triCandidates[i]);
          if (triIdx < 0 || triIdx >= static_cast<int>(mesh->triangleCount())) continue;

          const MeshTriangleIndices& tri = mesh->triangleIndices(triIdx);

          fm_vec3_t wv0 = mesh->hasTransform() ? mesh->toWorldSpace(mesh->vertex(tri.indices[0])) : mesh->vertex(tri.indices[0]);
          fm_vec3_t wv1 = mesh->hasTransform() ? mesh->toWorldSpace(mesh->vertex(tri.indices[1])) : mesh->vertex(tri.indices[1]);
          fm_vec3_t wv2 = mesh->hasTransform() ? mesh->toWorldSpace(mesh->vertex(tri.indices[2])) : mesh->vertex(tri.indices[2]);
          fm_vec3_t wn  = mesh->hasTransform() ? mesh->localNormalToWorld(mesh->triangleNormal(triIdx)) : mesh->triangleNormal(triIdx);

          candidate = CapsuleSweepHit{};
          if (!capsuleSweepTriangle(center, axisUp, radius, innerHalfHeight,
                                    displacement, wv0, wv1, wv2, wn, candidate))
            continue;

          if (!hit.didHit || candidate.t < hit.t ||
              (candidate.t == 0.0f && candidate.depth > hit.depth)) {
            hit = candidate;
          }
        }
      }
    }

    // Collider bodies (GJK + EPA)
    if (doBodies) {
      constexpr int MAX_CB_CANDIDATES = 16;
      NodeProxy cbCandidates[MAX_CB_CANDIDATES];

      const fm_vec3_t endCenter = center + displacement;
      const CapsuleGjkData capsuleStart{ center,    axisUp, innerHalfHeight, radius };
      const CapsuleGjkData capsuleEnd  { endCenter, axisUp, innerHalfHeight, radius };

      int cbCount = colliderAABBTree.queryBounds(sweptBox, cbCandidates, MAX_CB_CANDIDATES);
      for (int ci = 0; ci < cbCount; ++ci) {
        void *cbData = colliderAABBTree.getNodeData(cbCandidates[ci]);
        if (!cbData) continue;
        Collider *coll = static_cast<Collider *>(cbData);
        if (!coll || !coll->ownerObject() || coll->isTrigger()) continue;
        // Skip the character's own other colliders
        if (ignoreOwner && coll->ownerObject() == ignoreOwner) continue;
        if ((coll->writeMask() & readMask) == 0) continue;

        // Stable fallback direction for when EPA degenerates at a near-zero-depth touch.
        // Points from the collider toward the capsule
        // this can happen if .e.g the capsule hangs on the edge of an AABB
        fm_vec3_t fallbackN = center - coll->worldCenter();
        if (fm_vec3_len2(&fallbackN) < FM_EPSILON * FM_EPSILON) fallbackN = axisUp;
        else fm_vec3_norm(&fallbackN, &fallbackN);

        // Below this, an "overlap" is really just a surface touch, not a penetration to push out of. 
        // Use the swept check here, which produces a stable contact normal for blocking/sliding.
        constexpr float MIN_REAL_PENETRATION = 0.001f;

        fm_vec3_t initDir = coll->worldCenter() - center;
        if (fm_vec3_len2(&initDir) < FM_EPSILON * FM_EPSILON) initDir = axisUp;

        Simplex simplex{};
        bool overlapAtStart = gjkCheckForOverlap(simplex,
          &capsuleStart, capsuleGjkSupport,
          coll, colliderWorldGjkSupport,
          initDir);

        if (overlapAtStart) {
          // Already inside: EPA gives the push-out normal and dept
          EpaResult epa{};
          if (epaSolve(simplex, &capsuleStart, capsuleGjkSupport, coll, colliderWorldGjkSupport, epa)
              && epa.penetration > MIN_REAL_PENETRATION) {
            fm_vec3_t n = epa.normal;
            if (fm_vec3_len2(&n) < FM_EPSILON * FM_EPSILON) n = fallbackN;

            if (!hit.didHit || hit.t > 0.0f || epa.penetration > hit.depth) {
              hit.didHit = true;
              hit.t      = 0.0f;
              hit.normal = n;
              hit.depth  = epa.penetration;
              hit.point  = epa.contactB;
            }
            continue;
          }
          // Shallow / degenerate touch -> fall through to the swept check below
        }

        // End-position (swept) overlap check:
        fm_vec3_t endInitDir = coll->worldCenter() - endCenter;
        if (fm_vec3_len2(&endInitDir) < FM_EPSILON * FM_EPSILON) endInitDir = axisUp;

        Simplex endSimplex{};
        bool overlapAtEnd = gjkCheckForOverlap(endSimplex,
          &capsuleEnd, capsuleGjkSupport,
          coll, colliderWorldGjkSupport,
          endInitDir);

        if (!overlapAtEnd) continue;

        EpaResult epa{};
        if (!epaSolve(endSimplex, &capsuleEnd, capsuleGjkSupport, coll, colliderWorldGjkSupport, epa)) continue;

        fm_vec3_t n = epa.normal;
        if (fm_vec3_len2(&n) < FM_EPSILON * FM_EPSILON) n = fallbackN;

        // Estimate the touch fraction by backing the end-penetration out along the normal
        const float dispAlongNormal = fabsf(fm_vec3_dot(&displacement, &n));
        float t = 1.0f;
        if (dispAlongNormal > FM_EPSILON) {
          t = fmaxf(0.0f, fminf(1.0f - epa.penetration / dispAlongNormal, 1.0f));
        }

        if (!hit.didHit || t < hit.t) {
          hit.didHit = true;
          hit.t      = t;
          hit.normal = n;
          // Swept contact: the capsule is NOT penetrating at the start position,
          // it only reaches the surface partway through the move. 
          // `depth` is reserved for pre-existing overlaps, so set 0 here
          hit.depth  = 0.0f;
          hit.point  = epa.contactB;
        }
      }
    }

    return hit.didHit;
  }

  bool CollisionScene::sphereSweep(
          const fm_vec3_t& center,
          float radius,
          const fm_vec3_t& displacement,
          RaycastColliderTypeFlags collTypes,
          uint8_t readMask,
          SphereSweepHit& hit,
          const Object* ignoreOwner
  ) const {
    hit = SphereSweepHit{};

    const bool doMesh    = hasFlag(collTypes, RaycastColliderTypeFlags::MESH_COLLIDERS);
    const bool doBodies  = hasFlag(collTypes, RaycastColliderTypeFlags::COLLIDER_BODIES);
    if (!doMesh && !doBodies) return false;

    // Compute world-space swept AABB for broadphase
    fm_vec3_t extents = {
            radius,
            radius,
            radius
    };
    AABB capsuleBox = { center - extents, center + extents };
    AABB sweptBox;
    aabbExtendDirection(capsuleBox, displacement, sweptBox);

    SphereSweepHit candidate{};

    // ── Mesh colliders ──────────────────────────────────────────────────────
    if (doMesh) {
      constexpr int MAX_TRI = 64;
      NodeProxy triCandidates[MAX_TRI];
      constexpr int MAX_MESH_CANDIDATES = 32;
      NodeProxy meshCandidates[MAX_MESH_CANDIDATES];

      int meshCount = meshColliderAABBTree.queryBounds(sweptBox, meshCandidates, MAX_MESH_CANDIDATES);
      for (int m = 0; m < meshCount; ++m) {
        const MeshCollider* mesh = static_cast<const MeshCollider*>(
                meshColliderAABBTree.getNodeData(meshCandidates[m]));
        if (!mesh || mesh->triangleCount() == 0 || !mesh->ownerObject()) continue;

        AABB localSweptBox = mesh->worldAabbToLocal(sweptBox);
        int triCount = mesh->queryTriangleNodes(localSweptBox, triCandidates, MAX_TRI);

        for (int i = 0; i < triCount; ++i) {
          int triIdx = mesh->triangleIndexForNode(triCandidates[i]);
          if (triIdx < 0 || triIdx >= static_cast<int>(mesh->triangleCount())) continue;

          const MeshTriangleIndices& tri = mesh->triangleIndices(triIdx);

          fm_vec3_t wv0 = mesh->hasTransform() ? mesh->toWorldSpace(mesh->vertex(tri.indices[0])) : mesh->vertex(tri.indices[0]);
          fm_vec3_t wv1 = mesh->hasTransform() ? mesh->toWorldSpace(mesh->vertex(tri.indices[1])) : mesh->vertex(tri.indices[1]);
          fm_vec3_t wv2 = mesh->hasTransform() ? mesh->toWorldSpace(mesh->vertex(tri.indices[2])) : mesh->vertex(tri.indices[2]);
          fm_vec3_t wn  = mesh->hasTransform() ? mesh->localNormalToWorld(mesh->triangleNormal(triIdx)) : mesh->triangleNormal(triIdx);

          candidate = SphereSweepHit{};
          if (!sphereSweepTriangle(center, radius,
                                    displacement, wv0, wv1, wv2, wn, candidate))
            continue;

          if (!hit.didHit || candidate.t < hit.t ||
              (candidate.t == 0.0f && candidate.depth > hit.depth)) {
            hit = candidate;
          }
        }
      }
    }

    // Collider bodies (GJK + EPA)
    if (doBodies) {
      constexpr int MAX_CB_CANDIDATES = 16;
      NodeProxy cbCandidates[MAX_CB_CANDIDATES];

      const fm_vec3_t endCenter = center + displacement;
      const SphereGjkData sphereStart{ center, radius };
      const SphereGjkData sphereEnd  { endCenter, radius };

      int cbCount = colliderAABBTree.queryBounds(sweptBox, cbCandidates, MAX_CB_CANDIDATES);
      for (int ci = 0; ci < cbCount; ++ci) {
        void *cbData = colliderAABBTree.getNodeData(cbCandidates[ci]);
        if (!cbData) continue;
        Collider *coll = static_cast<Collider *>(cbData);
        if (!coll || !coll->ownerObject() || coll->isTrigger()) continue;
        // Skip the sweep's filtered colliders
        if (ignoreOwner && coll->ownerObject() == ignoreOwner) continue;
        if ((coll->writeMask() & readMask) == 0) continue;

        // Stable fallback direction for when EPA degenerates at a near-zero-depth touch.
        // Points from the collider toward the sphere
        // this can happen if .e.g the sphere hangs on the edge of an AABB
        fm_vec3_t fallbackN = center - coll->worldCenter();
        //if (fm_vec3_len2(&fallbackN) < FM_EPSILON * FM_EPSILON) fallbackN = axisUp;
        //else fm_vec3_norm(&fallbackN, &fallbackN);
        fm_vec3_norm(&fallbackN, &fallbackN);

        // Below this, an "overlap" is really just a surface touch, not a penetration to push out of.
        // Use the swept check here, which produces a stable contact normal for blocking/sliding.
        constexpr float MIN_REAL_PENETRATION = 0.001f;

        fm_vec3_t initDir = coll->worldCenter() - center;
        //if (fm_vec3_len2(&initDir) < FM_EPSILON * FM_EPSILON) initDir = axisUp;

        Simplex simplex{};
        bool overlapAtStart = gjkCheckForOverlap(simplex,
                                                 &sphereStart, sphereGjkSupport,
                                                 coll, colliderWorldGjkSupport,
                                                 initDir);

        if (overlapAtStart) {
          // Already inside: EPA gives the push-out normal and dept
          EpaResult epa{};
          if (epaSolve(simplex, &sphereStart, sphereGjkSupport, coll, colliderWorldGjkSupport, epa)
              && epa.penetration > MIN_REAL_PENETRATION) {
            fm_vec3_t n = epa.normal;
            if (fm_vec3_len2(&n) < FM_EPSILON * FM_EPSILON) n = fallbackN;

            if (!hit.didHit || hit.t > 0.0f || epa.penetration > hit.depth) {
              hit.didHit = true;
              hit.t      = 0.0f;
              hit.normal = n;
              hit.depth  = epa.penetration;
              hit.point  = epa.contactB;
            }
            continue;
          }
          // Shallow / degenerate touch -> fall through to the swept check below
        }

        // End-position (swept) overlap check:
        fm_vec3_t endInitDir = coll->worldCenter() - endCenter;
        //if (fm_vec3_len2(&endInitDir) < FM_EPSILON * FM_EPSILON) endInitDir = axisUp;

        Simplex endSimplex{};
        bool overlapAtEnd = gjkCheckForOverlap(endSimplex,
                                               &sphereEnd, sphereGjkSupport,
                                               coll, colliderWorldGjkSupport,
                                               endInitDir);

        if (!overlapAtEnd) continue;

        EpaResult epa{};
        if (!epaSolve(endSimplex, &sphereEnd, sphereGjkSupport, coll, colliderWorldGjkSupport, epa)) continue;

        fm_vec3_t n = epa.normal;
        if (fm_vec3_len2(&n) < FM_EPSILON * FM_EPSILON) n = fallbackN;

        // Estimate the touch fraction by backing the end-penetration out along the normal
        const float dispAlongNormal = fabsf(fm_vec3_dot(&displacement, &n));
        float t = 1.0f;
        if (dispAlongNormal > FM_EPSILON) {
          t = fmaxf(0.0f, fminf(1.0f - epa.penetration / dispAlongNormal, 1.0f));
        }

        if (!hit.didHit || t < hit.t) {
          hit.didHit = true;
          hit.t      = t;
          hit.normal = n;
          // Swept contact: the capsule is NOT penetrating at the start position,
          // it only reaches the surface partway through the move.
          // `depth` is reserved for pre-existing overlaps, so set 0 here
          hit.depth  = 0.0f;
          hit.point  = epa.contactB;
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

      RigidBody *rb = collider->rigidBody_;
      const bool changed = rb ? collider->syncFromRigidBody(rb->position_, rb->rotation_)
                              : collider->syncWorldState();

      if(!changed || collider->aabbTreeNodeId_ == NULL_NODE) continue;

      const fm_vec3_t displacement = collider->worldCenter_ - previousCenter;
      colliderAABBTree.moveNode(collider->aabbTreeNodeId_, collider->worldAabb_, displacement);
    }

    // Update compound CoM/inertia on demand and refresh world inertia tensors.
    for (RigidBody *body : rigidBodies_){
      syncCompoundProperties(body);
      if(!body->isEnabled_ || body->isSleeping_) continue;
      body->updateWorldInertia();
    }
    ticksWorldUpdate = get_ticks() - stageStart;

    stageStart = get_ticks();
    // Integrate velocities (also resets sleeping bodies — see RigidBody::integrateVelocity)
    for(RigidBody *body : rigidBodies_) {
      if (!body->isEnabled_) continue;
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
    warmStart();
    ticksWarmStart = get_ticks() - stageStart;

    // Velocity constraint solver
    stageStart = get_ticks();
    solveVelocityConstraints();

    ticksVelocitySolve = get_ticks() - stageStart;

    // Integrate positions and rotations
    stageStart = get_ticks();
    for(RigidBody *body : rigidBodies_) {
      if(!body->isEnabled_ || body->isSleeping_) continue;

      body->integratePosition(fixedDt_);
      body->integrateRotation(fixedDt_);

    }
    ticksIntegration = get_ticks() - stageStart;

    // Position constraint solver
    stageStart = get_ticks();
    solvePositionConstraints();
    ticksPositionSolve = get_ticks() - stageStart;

    // Apply position constraints, inertia and world state of rigidbodies and colliders
    stageStart = get_ticks();
    for(RigidBody *body : rigidBodies_) {

      if(!body || !body->owner_) continue;

      body->applyPositionConstraints();
      body->updateWorldInertia();

      const std::vector<Collider *> &ownerColliders = body->attachedColliders_;
      if(ownerColliders.empty()) continue;

      body->worldAabb_ = AABB {.min = body->worldCenterOfMass_, .max = body->worldCenterOfMass_};
      for (Collider *collider : ownerColliders)
      {
        if (!collider) continue;

        const fm_vec3_t previousCenter = collider->worldCenter_;

        if (collider->syncFromRigidBody(body->position_, body->rotation_)) {
          const fm_vec3_t displacement = collider->worldCenter_ - previousCenter;
          colliderAABBTree.moveNode(collider->aabbTreeNodeId_, collider->worldAabb_, displacement);
        }

        body->worldAabb_ = aabbUnion(body->worldAabb_, collider->worldAabb_);
      }

      // Sync visual object with physics position
      body->owner_->pos = body->position_ * getGfxScale();
      body->owner_->rot = body->rotation_;
    }

    // Update RigidBody sleep states
    updateSleepStates();

   for(RigidBody *body : rigidBodies_) {
        if(!body || !body->owner_) continue;
        //update Transform snapshot after final positions are applied
        if(!body->isSleeping_) {
            // We only update this while the body is awake, so slow cumulative movement
            // will eventually exceed the sleep threshold
            body->previousStepPosition_ = body->position_;
            body->previousStepRotation_ = body->rotation_;
            body->previousStepScale_ = body->owner_->scale;
          }
      }

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

        for (uint16_t t = 0; t < meshCollider->triangleCount_; ++t)
        {
          int idxA = meshCollider->triangles_[t].indices[0];
          int idxB = meshCollider->triangles_[t].indices[1];
          int idxC = meshCollider->triangles_[t].indices[2];

          fm_vec3_t v0 = meshCollider->toWorldSpace(meshCollider->vertices_[idxA]) * getGfxScale();
          fm_vec3_t v1 = meshCollider->toWorldSpace(meshCollider->vertices_[idxB]) * getGfxScale();
          fm_vec3_t v2 = meshCollider->toWorldSpace(meshCollider->vertices_[idxC]) * getGfxScale();

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
          const RigidBody *rigidBody = collider->rigidBody_;
          const bool isSleepingBody = rigidBody && rigidBody->isSleeping_;

          if (isSleepingBody)
          {
            col = color_t{0x80, 0x80, 0x80, 0xFF};
          }

          switch (collider->type_)
          {
          case ShapeType::Sphere:
            if (!isSleepingBody) col = color_t{0xFF, 0x00, 0x00, 0xFF};
            Debug::drawSphere(collider->worldCenter_ * getGfxScale(), collider->sphere_.radius * getGfxScale(), col);
            break;
          case ShapeType::Box:
            if (!isSleepingBody) col = color_t{0x00, 0xFF, 0xFF, 0xFF};
            Debug::drawOBB(collider->worldCenter_ * getGfxScale(), collider->box_.halfSize * getGfxScale(), collider->owner_->rot, col);
            break;
          case ShapeType::Capsule:
            if (!isSleepingBody) col = color_t{0x00, 0x80, 0xFF, 0xFF};
            Debug::drawCapsule(
                collider->worldCenter_ * getGfxScale(),
                collider->capsule_.radius * getGfxScale(),
                collider->capsule_.innerHalfHeight * getGfxScale(),
                collider->owner_->rot,
                col);
            break;
          case ShapeType::Cylinder:
            if (!isSleepingBody) col = color_t{0xFF, 0x80, 0x00, 0xFF};
            Debug::drawCylinder(
                collider->worldCenter_ * getGfxScale(),
                collider->cylinder_.radius * getGfxScale(),
                collider->cylinder_.halfHeight * getGfxScale(),
                collider->owner_->rot,
                col);
            break;
          case ShapeType::Cone:
            if (!isSleepingBody) col = color_t{0xFF, 0x40, 0xA0, 0xFF};
            Debug::drawCone(
                collider->worldCenter_ * getGfxScale(),
                collider->cone_.radius * getGfxScale(),
                collider->cone_.halfHeight * getGfxScale(),
                collider->owner_->rot,
                col);
            break;
          case ShapeType::Pyramid:
            if (!isSleepingBody) col = color_t{0xB0, 0xFF, 0x40, 0xFF};
            Debug::drawPyramid(
                collider->worldCenter_ * getGfxScale(),
                collider->pyramid_.baseHalfWidthX * getGfxScale(),
                collider->pyramid_.baseHalfWidthZ * getGfxScale(),
                collider->pyramid_.halfHeight * getGfxScale(),
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
