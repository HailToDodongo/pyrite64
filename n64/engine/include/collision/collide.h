/**
 * @file collide.h
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Functions to detect collisions between different shapes and objects and record them
 */
#pragma once

#include "rigidBody.h"
#include "meshCollider.h"
#include "epa.h"

namespace P64::Coll {

  struct CollisionScene; // forward declare
  struct ColliderProxy; // forward declare

  bool collideDetectObjectToObject(Collider *colliderA, RigidBody *rigidBodyA, Collider *colliderB, RigidBody *rigidBodyB, bool recordConstraints);
  bool collideDetectObjectToMesh(Collider *collider, RigidBody *rigidBody, const MeshCollider &mesh, bool recordConstraints);
  bool collideDetectObjectToTriangle(ColliderProxy *colliderProxy, RigidBody *rigidBody, const MeshCollider &mesh, int triangleIndex, bool recordConstraints);

  ContactConstraint *collideCacheContactConstraint(
    RigidBody *rigidBodyA, Collider *colliderA, MeshCollider *meshColliderA, Object *objectA,
    RigidBody *rigidBodyB, Collider *colliderB, MeshCollider *meshColliderB, Object *objectB, const EpaResult &result,
    float combinedFriction, float combinedBounce, bool isTrigger, bool respondsA, bool respondsB, int triangleIndex = -1);

} // namespace P64::Coll
