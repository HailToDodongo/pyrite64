/**
 * @file collider_shape.cpp
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Defines the Basic (non-mesh) Colliders (see colliderShape.h)
 */
#include "collision/colliderShape.h"
#include "collision/meshCollider.h"
#include "collision/rigidBody.h"
#include "scene/object.h"

using namespace P64::Coll;

void Collider::markGeometryChanged() {
  geometryDirty_ = true;
  // size and offset of a shape feed into the center of mass and inertia tensor of a compound body
  if(rigidBody_) rigidBody_->markCompoundPropertiesDirty();
}

void Collider::refreshWorldShape() {
  const fm_vec3_t scale = owner_ ? owner_->scale : fm_vec3_t{{1.0f, 1.0f, 1.0f}};
  const float x = fabsf(localHalfExtend_.x * scale.x);
  const float y = fabsf(localHalfExtend_.y * scale.y);
  const float z = fabsf(localHalfExtend_.z * scale.z);
  const float radius = fmaxf(x, z);

  switch(type_) {
    case ShapeType::Sphere:
      sphere_.radius = fmaxf(x, fmaxf(y, z));
    break;
    case ShapeType::Box:
      box_.halfSize = fm_vec3_t{{x, y, z}};
    break;
    case ShapeType::Capsule:
      capsule_.radius = radius;
      capsule_.innerHalfHeight = y;
    break;
    case ShapeType::Cylinder:
      cylinder_.radius = radius;
      cylinder_.halfHeight = y;
    break;
    case ShapeType::Cone:
      cone_.radius = radius;
      cone_.halfHeight = y;
    break;
    case ShapeType::Pyramid:
      pyramid_.baseHalfWidthX = x;
      pyramid_.baseHalfWidthZ = z;
      pyramid_.halfHeight = y;
    break;
  }

  markGeometryChanged();
}

fm_vec3_t Collider::support(const fm_vec3_t &dir) const {
  switch(type_) {
    case ShapeType::Sphere:   return sphere_.support(dir);
    case ShapeType::Box:      return box_.support(dir);
    case ShapeType::Capsule:  return capsule_.support(dir);
    case ShapeType::Cylinder: return cylinder_.support(dir);
    case ShapeType::Cone:     return cone_.support(dir);
    case ShapeType::Pyramid:  return pyramid_.support(dir);
  }
  __builtin_unreachable();
}

AABB Collider::boundingBox(const fm_quat_t *rotation) const {
  switch(type_) {
    case ShapeType::Sphere:   return sphere_.boundingBox(rotation);
    case ShapeType::Box:      return box_.boundingBox(rotation);
    case ShapeType::Capsule:  return capsule_.boundingBox(rotation);
    case ShapeType::Cylinder: return cylinder_.boundingBox(rotation);
    case ShapeType::Cone:     return cone_.boundingBox(rotation);
    case ShapeType::Pyramid:  return pyramid_.boundingBox(rotation);
  }
  __builtin_unreachable();
}

fm_vec3_t Collider::inertiaTensor(float mass) const {
  switch(type_) {
    case ShapeType::Sphere:   return sphere_.inertiaTensor(mass);
    case ShapeType::Box:      return box_.inertiaTensor(mass);
    case ShapeType::Capsule:  return capsule_.inertiaTensor(mass);
    case ShapeType::Cylinder: return cylinder_.inertiaTensor(mass);
    case ShapeType::Cone:     return cone_.inertiaTensor(mass);
    case ShapeType::Pyramid:  return pyramid_.inertiaTensor(mass);
  }
  __builtin_unreachable();
}

fm_vec3_t Collider::toWorldSpace(const fm_vec3_t &localPoint) const {
  return worldCenter_ + rotateToWorld(localPoint);
}

fm_vec3_t Collider::toLocalSpace(const fm_vec3_t &worldPoint) const {
  return rotateToLocal(worldPoint - worldCenter_);
}

fm_vec3_t Collider::rotateToWorld(const fm_vec3_t &localDir) const {
  return matrix3Vec3Mul(rotationMatrix_, localDir);
}

fm_vec3_t Collider::rotateToLocal(const fm_vec3_t &worldDir) const {
  return matrix3Vec3Mul(inverseRotationMatrix_, worldDir);
}

bool Collider::hasOwnerTransformChanged() const {
  if(!owner_) return false;
  if(!hasCachedOwnerTransform_) return true;

  fm_vec3_t ownerPhysicsPos = owner_->pos;
  if(fm_vec3_distance2(&ownerPhysicsPos, &lastOwnerPosition_) > FM_EPSILON * FM_EPSILON ) return true;
  if(fm_vec3_distance2(&owner_->scale, &lastOwnerScale_) > FM_EPSILON * FM_EPSILON ) return true;

  const float rotSim = fabsf(quatDot(owner_->rot, lastOwnerRotation_));
  return rotSim < (1.0f - FM_EPSILON);
}

void Collider::syncOwnerTransform() {
  if(!owner_) {
    lastOwnerPosition_ = VEC3_ZERO;
    lastOwnerRotation_ = QUAT_IDENTITY;
    lastOwnerScale_ = fm_vec3_t{{1.0f, 1.0f, 1.0f}};
  } else {
    lastOwnerPosition_ = owner_->pos;
    lastOwnerRotation_ = owner_->rot;
    lastOwnerScale_ = owner_->scale;
  }

  rotationMatrix_ = quatToMatrix3(lastOwnerRotation_);
  inverseRotationMatrix_ = quatToMatrix3(quatConjugate(lastOwnerRotation_));
  hasCachedOwnerTransform_ = true;
}

bool Collider::syncFromRigidBody(const fm_vec3_t& rbPosition, const fm_quat_t& rbRotation) {
  const bool scaleChanged = owner_ && owner_->scale != lastOwnerScale_;

  if(hasCachedOwnerTransform_ && !geometryDirty_ && !scaleChanged) {
    const float posDeltaSq = fm_vec3_distance2(&rbPosition, &lastOwnerPosition_);
    const float rotSim = fabsf(quatDot(rbRotation, lastOwnerRotation_));
    if(posDeltaSq <= FM_EPSILON * FM_EPSILON && rotSim >= 1.0f - FM_EPSILON) {
      return false;
    }
  }

  lastOwnerPosition_ = rbPosition;
  lastOwnerRotation_ = rbRotation;
  lastOwnerScale_ = owner_ ? owner_->scale : fm_vec3_t{{1,1,1}};
  if(scaleChanged) refreshWorldShape();
  rotationMatrix_ = quatToMatrix3(lastOwnerRotation_);
  inverseRotationMatrix_ = quatToMatrix3(quatConjugate(lastOwnerRotation_));
  hasCachedOwnerTransform_ = true;

  worldCenter_ = lastOwnerPosition_ + matrix3Vec3Mul(rotationMatrix_, parentOffset_ * lastOwnerScale_);

  const AABB local = boundingBox(&lastOwnerRotation_);
  worldAabb_.min = local.min + worldCenter_;
  worldAabb_.max = local.max + worldCenter_;
  ++worldStateVersion_;

  return true;
}

bool Collider::syncWorldState() {
  if(!owner_) return false;

  const bool transformChanged = !hasCachedOwnerTransform_ || hasOwnerTransformChanged();
  // a resized or moved shape needs a new AABB too, even if the object itself didn't move
  if(!transformChanged && !geometryDirty_) return false;

  const bool scaleChanged = owner_->scale != lastOwnerScale_;

  if(transformChanged) syncOwnerTransform();
  // the shape is object-scaled, so a scaled object needs it rebuilt
  if(scaleChanged) refreshWorldShape();
  worldCenter_ = lastOwnerPosition_ + matrix3Vec3Mul(rotationMatrix_, parentOffset_ * lastOwnerScale_);

  const AABB local = boundingBox(&lastOwnerRotation_);
  worldAabb_.min = local.min + worldCenter_;
  worldAabb_.max = local.max + worldCenter_;
  ++worldStateVersion_;
  return true;
}

bool Collider::readsCollider(const Collider *other) const {
  return other && ((readMask_ & other->writeMask_) != 0);
}

bool Collider::readsMeshCollider(const MeshCollider *other) const {
  return other && ((readMask_ & other->writeMask()) != 0);
}

void P64::Coll::colliderGjkSupport(const void *data, const fm_vec3_t &direction, fm_vec3_t &output) {
  auto *collider = static_cast<const Collider *>(data);
  output = collider->support(direction);
}
