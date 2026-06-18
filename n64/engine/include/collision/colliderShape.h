/**
 * @file colliderShape.h
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Defines the Basic (non-mesh) Colliders 
 */
#pragma once

#include "gjk.h"
#include "types.h"
#include "shapes.h"
#include "matrix3x3.h"
#include "aabbTree.h"
#include "gfxScale.h"

namespace P64
{
  class Object;
}

namespace P64::Coll {

  class CollisionScene;
  struct MeshCollider;

  struct Collider {
    void setShapeType(ShapeType newType) {
      type_ = newType;
      switch(type_) {
        case ShapeType::Sphere:   sphere_ = {}; break;
        case ShapeType::Box:      box_ = {}; break;
        case ShapeType::Capsule:  capsule_ = {}; break;
        case ShapeType::Cylinder: cylinder_ = {}; break;
        case ShapeType::Cone:     cone_ = {}; break;
        case ShapeType::Pyramid:  pyramid_ = {}; break;
      }
    }
    ShapeType shapeType() const { return type_; }

    SphereShape &sphereShape() { return sphere_; }
    const SphereShape &sphereShape() const { return sphere_; }
    BoxShape &boxShape() { return box_; }
    const BoxShape &boxShape() const { return box_; }
    CapsuleShape &capsuleShape() { return capsule_; }
    const CapsuleShape &capsuleShape() const { return capsule_; }
    CylinderShape &cylinderShape() { return cylinder_; }
    const CylinderShape &cylinderShape() const { return cylinder_; }
    ConeShape &coneShape() { return cone_; }
    const ConeShape &coneShape() const { return cone_; }
    PyramidShape &pyramidShape() { return pyramid_; }
    const PyramidShape &pyramidShape() const { return pyramid_; }

    void setOwner(P64::Object *newOwner) {
      owner_ = newOwner;
      hasCachedOwnerTransform_ = false;
    }
    P64::Object *ownerObject() const { return owner_; }

    void setParentOffset(const fm_vec3_t &newParentOffset) { parentOffset_ = newParentOffset * getInvGfxScale(); }
    const fm_vec3_t &parentOffset() const { return parentOffset_; }

    void setBounce(float newBounce) { bounce_ = newBounce; }
    float bounce() const { return bounce_; }
    void setFriction(float newFriction) { friction_ = newFriction; }
    float friction() const { return friction_; }

    void setTrigger(bool newIsTrigger) { isTrigger_ = newIsTrigger; }
    bool isTrigger() const { return isTrigger_; }

    void setCollisionMask(uint8_t newReadMask, uint8_t newWriteMask) {
      readMask_ = newReadMask;
      writeMask_ = newWriteMask;
    }
    uint8_t readMask() const { return readMask_; }
    uint8_t writeMask() const { return writeMask_; }

    const fm_vec3_t &worldCenter() const { return worldCenter_; }
    const AABB &worldAabb() const { return worldAabb_; }
    const Matrix3x3 &rotationMatrix() const { return rotationMatrix_; }
    const Matrix3x3 &inverseRotationMatrix() const { return inverseRotationMatrix_; }
    uint32_t worldStateVersion() const { return worldStateVersion_; }

    fm_vec3_t support(const fm_vec3_t &dir) const;
    AABB boundingBox(const fm_quat_t *rotation) const;
    fm_vec3_t inertiaTensor(float mass) const;
    fm_vec3_t toWorldSpace(const fm_vec3_t &localPoint) const;
    fm_vec3_t toLocalSpace(const fm_vec3_t &worldPoint) const;
    fm_vec3_t rotateToWorld(const fm_vec3_t &localDir) const;
    fm_vec3_t rotateToLocal(const fm_vec3_t &worldDir) const;
    bool hasOwnerTransformChanged() const;
    void syncOwnerTransform();
    bool syncFromRigidBody(const fm_vec3_t& rbPosition, const fm_quat_t& rbRotation);
    bool syncWorldState();
    bool readsCollider(const Collider *other) const;
    bool readsMeshCollider(const MeshCollider *other) const;

  private:
    friend class CollisionScene;

    union {
      SphereShape sphere_;
      BoxShape box_;
      CapsuleShape capsule_;
      CylinderShape cylinder_;
      ConeShape cone_;
      PyramidShape pyramid_;
    };

    P64::Object *owner_{nullptr};
    Matrix3x3 rotationMatrix_{Matrix3x3::identity()};
    Matrix3x3 inverseRotationMatrix_{Matrix3x3::identity()};
    AABB worldAabb_{};
    fm_vec3_t worldCenter_{};
    fm_vec3_t parentOffset_{};
    fm_vec3_t lastOwnerPosition_{};
    fm_quat_t lastOwnerRotation_{QUAT_IDENTITY};
    fm_vec3_t lastOwnerScale_{1.0f, 1.0f, 1.0f};
    float bounce_{0.0f};
    float friction_{0.8f};
    uint32_t worldStateVersion_{0};
    NodeProxy aabbTreeNodeId_{NULL_NODE};
    ShapeType type_{ShapeType::Sphere};
    uint8_t readMask_{0x00};
    uint8_t writeMask_{0x00};
    bool hasCachedOwnerTransform_{false};
    bool isTrigger_{false};
  };

  /// GJK-compatible support wrapper
  void colliderGjkSupport(const void *data, const fm_vec3_t &direction, fm_vec3_t &output);

} // namespace P64::Coll
