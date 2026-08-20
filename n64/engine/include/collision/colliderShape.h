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

namespace P64
{
  class Object;
}

namespace P64::Coll {

  class CollisionScene;
  struct MeshCollider;
  struct RigidBody;

  struct Collider {
    // Everything that can be set is in the owner object's local space, exactly like the values in
    // the editor. The object scale is applied on top of it and stays applied when the object
    // is scaled later on. The world-scale result is derived and read-only.

    /// Changes the shape type, the size is kept and folded into the new shape.
    void setShapeType(ShapeType newType) { setShape(newType, localHalfExtend_); }
    ShapeType shapeType() const { return type_; }

    /// Resizes the current shape from a half-extend box, keeping the shape type.
    /// Axes a shape has no use for are folded in, e.g. a cylinder takes its radius from
    /// max(x, z) and its half height from y. Negative values are mirrored.
    void setHalfExtend(const fm_vec3_t &newHalfExtend) { setShape(type_, newHalfExtend); }
    /// Size of the collider in the owner's local space, as set by any of the setters.
    const fm_vec3_t &halfExtend() const { return localHalfExtend_; }

    /// @brief Makes this a sphere collider of the given size.
    /// @param radius The radius of the sphere
    void setSphereShape(float radius) {
      setShape(ShapeType::Sphere, fm_vec3_t{{radius, radius, radius}});
    }

    /// @brief Makes this a box collider of the given size.
    /// @param halfSize The half size vector of the box
    void setBoxShape(const fm_vec3_t &halfSize) {
      setShape(ShapeType::Box, halfSize);
    }

    /// @brief Makes this a capsule collider of the given size ('innerHalfHeight' excludes the round caps).
    /// @param radius The radius of the capsule
    /// @param innerHalfHeight The half height of the capsule's cylindrical part
    void setCapsuleShape(float radius, float innerHalfHeight) {
      setShape(ShapeType::Capsule, fm_vec3_t{{radius, innerHalfHeight, radius}});
    }

    /// @brief Makes this a cylinder collider of the given size.
    /// @param radius The radius of the cylinder
    /// @param halfHeight The half height of the cylinder
    void setCylinderShape(float radius, float halfHeight) {
      setShape(ShapeType::Cylinder, fm_vec3_t{{radius, halfHeight, radius}});
    }
    /// @brief Makes this a cone collider of the given size.
    /// @param radius The radius of the cone's base
    /// @param halfHeight The half height of the cone
    void setConeShape(float radius, float halfHeight) {
      setShape(ShapeType::Cone, fm_vec3_t{{radius, halfHeight, radius}});
    }
    /// @brief Makes this a pyramid collider of the given size.
    /// @param baseHalfWidthX Half the width of the base along the X axis
    /// @param baseHalfWidthZ Half the width of the base along the Z axis
    /// @param halfHeight Half the height of the pyramid along the Y axis
    void setPyramidShape(float baseHalfWidthX, float baseHalfWidthZ, float halfHeight) {
      setShape(ShapeType::Pyramid, fm_vec3_t{{baseHalfWidthX, halfHeight, baseHalfWidthZ}});
    }

    /// @brief Moves the shape's center relative to the owner's origin, in the owner's local space.
    void setParentOffset(const fm_vec3_t &newParentOffset) {
      if(parentOffset_ == newParentOffset) return;
      parentOffset_ = newParentOffset;
      markGeometryChanged();
    }
    const fm_vec3_t &parentOffset() const { return parentOffset_; }

    // Object-scaled dimensions the collision detection runs on, derived from the setters above.
    const SphereShape &worldSphereShape() const { return sphere_; }
    const BoxShape &worldBoxShape() const { return box_; }
    const CapsuleShape &worldCapsuleShape() const { return capsule_; }
    const CylinderShape &worldCylinderShape() const { return cylinder_; }
    const ConeShape &worldConeShape() const { return cone_; }
    const PyramidShape &worldPyramidShape() const { return pyramid_; }

    void setOwner(P64::Object *newOwner) {
      owner_ = newOwner;
      hasCachedOwnerTransform_ = false;
    }
    P64::Object *ownerObject() const { return owner_; }

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
    
    /// @brief Applies a new shape type and/or local size, and derives the world-scale shape from it.
    /// @param newType The new shape type
    /// @param newLocalHalfExtend The new local half extend encoded in a vector, axes unused by the shape are folded in
    void setShape(ShapeType newType, const fm_vec3_t &newLocalHalfExtend) {
      if(type_ == newType && localHalfExtend_ == newLocalHalfExtend) return;
      type_ = newType;
      localHalfExtend_ = newLocalHalfExtend;
      refreshWorldShape();
    }


    /// @brief Whether the geometry of the collider changed since this was last called, resets the flag.
    /// Used by the collision scene to wake up what the geometry change may affect.
    /// @return true if the geometry changed since the last call, false otherwise
    bool consumeGeometryChanged() {
      const bool changed = geometryDirty_;
      geometryDirty_ = false;
      return changed;
    }

    /// @brief Rebuilds the object-scaled shape from the local half extend, called whenever either changes.
    void refreshWorldShape();

    /// @brief Flags the size or local placement as changed. On the next collision step the world AABB
    /// (and the mass properties of an attached rigid body) are rebuilt and sleeping bodies the
    /// change may touch are woken. Called by refreshWorldShape() and the offset setter.
    void markGeometryChanged();

    union {
      SphereShape sphere_;
      BoxShape box_;
      CapsuleShape capsule_;
      CylinderShape cylinder_;
      ConeShape cone_;
      PyramidShape pyramid_;
    };

    P64::Object *owner_{nullptr};
    // RigidBody registered for the same owner, maintained by the CollisionScene on add/remove
    RigidBody *rigidBody_{nullptr};
    Matrix3x3 rotationMatrix_{Matrix3x3::identity()};
    Matrix3x3 inverseRotationMatrix_{Matrix3x3::identity()};
    AABB worldAabb_{};
    fm_vec3_t worldCenter_{};
    // authored size in the owner's local space, the shape union above is this * the object scale
    fm_vec3_t localHalfExtend_{};
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
    // forces a world AABB rebuild even if the transform didn't change, consumed by the collision scene
    bool geometryDirty_{false};
  };

  /// @brief GJK-compatible support wrapper
  /// @param data The collider data
  /// @param direction The direction to query
  /// @param output The resulting support point
  void colliderGjkSupport(const void *data, const fm_vec3_t &direction, fm_vec3_t &output);

} // namespace P64::Coll
