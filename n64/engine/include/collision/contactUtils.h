#pragma once

#include "contact.h"
#include "rigidBody.h"
#include "meshCollider.h"

namespace P64::Coll {

  inline uint32_t contactTransformVersion(
    const RigidBody *rigidBody,
    const Collider *collider,
    const MeshCollider *meshCollider) {
    if(rigidBody) {
      return rigidBody->transformVersion();
    }

    if(meshCollider) {
      return meshCollider->worldTransformVersion();
    }

    if(collider) {
      return collider->worldStateVersion();
    }

    return 0;
  }

  inline fm_vec3_t contactLocalPointFromWorldPoint(
    const fm_vec3_t &worldPoint,
    const RigidBody *rigidBody,
    const Collider *collider,
    const MeshCollider *meshCollider) {
    if(rigidBody) {
      return rigidBody->toLocalSpace(worldPoint);
    }

    if(meshCollider) {
      return meshCollider->toLocalSpace(worldPoint);
    }

    if(collider) {
      return collider->toLocalSpace(worldPoint);
    }

    return worldPoint;
  }

  inline fm_vec3_t contactWorldPointFromLocalPoint(
    const fm_vec3_t &localPoint,
    const RigidBody *rigidBody,
    const Collider *collider,
    const MeshCollider *meshCollider) {
    if(rigidBody) {
      return rigidBody->toWorldSpace(localPoint);
    }

    if(meshCollider) {
      return meshCollider->toWorldSpace(localPoint);
    }

    if(collider) {
      return collider->toWorldSpace(localPoint);
    }

    return localPoint;
  }

  inline void refreshContactPointWorldState(
    ContactPoint &point,
    const ContactConstraint &constraint) {
    point.contactA = contactWorldPointFromLocalPoint(point.localPointA, constraint.rigidBodyA, constraint.colliderA, constraint.meshColliderA);
    point.contactB = contactWorldPointFromLocalPoint(point.localPointB, constraint.rigidBodyB, constraint.colliderB, constraint.meshColliderB);
    point.point = (point.contactA + point.contactB) * 0.5f;

    const fm_vec3_t diff = point.contactA - point.contactB;
    point.penetration = -fm_vec3_dot(&diff, &constraint.normal);
  }

} // namespace P64::Coll