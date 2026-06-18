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

  inline fm_vec3_t contactReferenceOffset(
    const fm_vec3_t &worldPoint,
    const RigidBody *rigidBody,
    const Collider *collider,
    const MeshCollider *meshCollider) {
    if(rigidBody) {
      return worldPoint - rigidBody->worldCenterOfMass();
    }

    if(meshCollider) {
      return worldPoint - (meshCollider->ownerObject() ? meshCollider->ownerObject()->pos : VEC3_ZERO);
    }

    if(collider) {
      return worldPoint - collider->worldCenter();
    }

    return VEC3_ZERO;
  }

  inline void refreshContactPointWorldState(
    ContactPoint &point,
    const ContactConstraint &constraint,
    bool computeRelativeOffsets = false) {
    point.contactA = contactWorldPointFromLocalPoint(point.localPointA, constraint.rigidBodyA, constraint.colliderA, constraint.meshColliderA);
    point.contactB = contactWorldPointFromLocalPoint(point.localPointB, constraint.rigidBodyB, constraint.colliderB, constraint.meshColliderB);
    point.point = (point.contactA + point.contactB) * 0.5f;

    const fm_vec3_t diff = point.contactA - point.contactB;
    point.penetration = -fm_vec3_dot(&diff, &constraint.normal);

    if(computeRelativeOffsets) {
      point.aToContact = contactReferenceOffset(point.contactA, constraint.rigidBodyA, constraint.colliderA, constraint.meshColliderA);
      point.bToContact = contactReferenceOffset(point.contactB, constraint.rigidBodyB, constraint.colliderB, constraint.meshColliderB);
    } else {
      point.aToContact = VEC3_ZERO;
      point.bToContact = VEC3_ZERO;
    }
  }

} // namespace P64::Coll