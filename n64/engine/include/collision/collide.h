/**
 * @file collide.h
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Functions to detect collisions between different shapes and objects and record them
 */
#pragma once

#include "rigid_body.h"
#include "mesh_collider.h"
#include "epa.h"

namespace P64::Coll {

  struct CollisionScene; // forward declare
  struct ColliderProxy; // forward declare

  bool collideDetectObjectToObject(Collider *colliderA, RigidBody *rigidBodyA, Collider *colliderB, RigidBody *rigidBodyB);
  bool collideDetectObjectToMesh(Collider *collider, RigidBody *rigidBody, const MeshCollider &mesh);
  bool collideDetectObjectToTriangle(ColliderProxy *colliderProxy, RigidBody *rigidBody, const MeshCollider &mesh, int triangleIndex);

  ContactConstraint *collideCacheContactConstraint(
    RigidBody *rigidBodyA, Collider *colliderA, MeshCollider *meshColliderA, Object *objectA,
    RigidBody *rigidBodyB, Collider *colliderB, MeshCollider *meshColliderB, Object *objectB, const EpaResult &result,
    float combinedFriction, float combinedBounce, bool isTrigger, bool respondsA, bool respondsB, int triangleIndex = -1);

} // namespace P64::Coll
