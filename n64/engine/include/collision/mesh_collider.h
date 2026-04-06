/**
 * @file mesh_collider.h
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Mesh Collider definitions and functions
 */
#pragma once

#include "vec_math.h"
#include "matrix3x3.h"
#include "aabb_tree.h"
#include <cstdint>

namespace P64 { class Object; }

namespace P64::Coll {

  class CollisionScene;
  struct Raycast;
  struct EpaResult;
  struct Collider;

  struct MeshCollider; // forward declare

  struct MeshTriangleIndices {
    uint16_t indices[3];
  };

  struct MeshTriangle {
    const fm_vec3_t *vertices;
    fm_vec3_t normal;
    MeshTriangleIndices tri;
    const MeshCollider *mesh{nullptr}; ///< Parent mesh for world-space transforms

    fm_vec3_t localVertex(int localIndex) const;

    void gjkSupport(const fm_vec3_t &direction, fm_vec3_t &output) const;
    float comparePoint(const fm_vec3_t &point) const;

    /// Get triangle vertex in world space (applies mesh transform if present)
    fm_vec3_t worldVertex(int localIndex) const;
    /// Get triangle normal in world space
    fm_vec3_t worldNormal() const;
  };

  /// GJK-compatible wrapper for MeshTriangle
  void meshTriangleGjkSupport(const void *data, const fm_vec3_t &direction, fm_vec3_t &output);

  struct MeshCollider {
    P64::Object *ownerObject() const { return owner_; }

    void setCollisionMask(uint8_t newReadMask, uint8_t newWriteMask) {
      readMask_ = newReadMask;
      writeMask_ = newWriteMask;
    }
    uint8_t readMask() const { return readMask_; }
    uint8_t writeMask() const { return writeMask_; }

    void setFriction(float newFriction) { friction_ = newFriction; }
    float friction() const { return friction_; }
    void setBounce(float newBounce) { bounce_ = newBounce; }
    float bounce() const { return bounce_; }

    uint16_t triangleCount() const { return triangleCount_; }
    uint16_t vertexCount() const { return vertexCount_; }
    const fm_vec3_t &vertex(uint16_t index) const { return vertices_[index]; }
    const MeshTriangleIndices &triangleIndices(uint16_t index) const { return triangles_[index]; }
    const fm_vec3_t &triangleNormal(uint16_t index) const { return normals_[index]; }
    int queryTriangleNodes(const AABB &localBounds, NodeProxy *outCandidates, int maxCandidates) const { return aabbTree_.queryBounds(localBounds, outCandidates, maxCandidates); }
    int queryTriangleNodes(const Raycast &localRay, NodeProxy *outCandidates, int maxCandidates) const { return aabbTree_.queryRay(localRay, outCandidates, maxCandidates); }
    int triangleIndexForNode(NodeProxy node) const {
      void *data = aabbTree_.getNodeData(node);
      return data ? static_cast<int>(reinterpret_cast<intptr_t>(data)) - 1 : -1;
    }

    const AABB &localRootAabb() const { return localRootAabb_; }
    const AABB &worldAabb() const { return worldAabb_; }
    uint32_t worldTransformVersion() const { return worldTransformVersion_; }
    bool hasCachedOwnerTransform() const { return hasCachedOwnerTransform_; }
    bool transformChanged() const { return transformChanged_; }
    const Matrix3x3 &inverseRotationMatrix() const { return inverseRotationMatrix_; }

    /// Transform a local-space point to world space
    fm_vec3_t toWorldSpace(const fm_vec3_t &localPoint) const;
    /// Transform a world-space point to local space
    fm_vec3_t toLocalSpace(const fm_vec3_t &worldPoint) const;
    /// Rotate a local-space direction/normal to world space (no translation)
    fm_vec3_t rotateToWorld(const fm_vec3_t &localDir) const;
    /// Rotate a world-space direction/normal to local space (no translation)
    fm_vec3_t rotateToLocal(const fm_vec3_t &worldDir) const;
    fm_vec3_t localNormalToWorld(const fm_vec3_t &localNormal) const;
    void localResultToWorld(EpaResult &result) const;

    /// Recompute worldAabb from localRootAabb + current transform
    void recalculateWorldAabb();
    /// Returns true if the owner's transform differs from the cached transform snapshot
    bool hasOwnerTransformChanged() const;
    /// Updates the cached owner transform snapshot to the current owner transform
    void syncOwnerTransform();
    /// Compute localRootAabb from the internal AABB tree root node
    void computeLocalRootAabb();
    /// Transform a world-space AABB into a conservative local-space AABB for tree queries
    AABB worldAabbToLocal(const AABB &worldAabb) const;

    /// Returns true if the mesh has a non-identity transform
    bool hasTransform() const;

    /// Returns true if the mesh has a non-identity rotation
    bool hasRotation() const;
    /// Returns true if the mesh has a non-zero position
    bool hasPosition() const;
    /// Returns true if the mesh has a non-uniform (1,1,1) scale
    bool hasScale() const;

    bool readsCollider(const Collider *other) const;
    bool readsMeshCollider(const MeshCollider *other) const;

    /// Create a MeshCollider directly from collision asset raw data, binding to the given Object's transform.
    /// The returned collider owns newly allocated arrays (vertices, triangles, normals).
    /// Call destroyData() to free them.
    static MeshCollider *createFromRawData(void *rawData, Object *obj);

    static inline fm_vec3_t triangleNormalFromVertices(const fm_vec3_t &v0, const fm_vec3_t &v1, const fm_vec3_t &v2)
    {
      const fm_vec3_t edge0 = v1 - v0;
      const fm_vec3_t edge1 = v2 - v0;
      fm_vec3_t normal;
      fm_vec3_cross(&normal, &edge0, &edge1);
      return vec3NormalizeOrFallback(normal, VEC3_UP);
    }

    /// Free owned vertex/triangle/normal arrays and destroy the AABB tree.
    void destroyData();

  private:
    friend class CollisionScene;

    AABBTree aabbTree_{};
    fm_vec3_t *vertices_{nullptr};
    MeshTriangleIndices *triangles_{nullptr};
    fm_vec3_t *normals_{nullptr};
    P64::Object *owner_{nullptr};
    AABB localRootAabb_{};
    AABB worldAabb_{};
    fm_vec3_t lastOwnerPosition_{};
    fm_quat_t lastOwnerRotation_{QUAT_IDENTITY};
    fm_vec3_t lastOwnerScale_{1.0f, 1.0f, 1.0f};
    Matrix3x3 inverseRotationMatrix_{};
    float friction_{1.0f};
    float bounce_{0.0f};
    uint32_t worldTransformVersion_{0};
    NodeProxy aabbTreeNodeId_{NULL_NODE};
    uint16_t triangleCount_{0};
    uint16_t vertexCount_{0};
    uint8_t readMask_{0x00};
    uint8_t writeMask_{0x00};
    bool hasCachedOwnerTransform_{false};
    bool transformChanged_{false};
  };

} // namespace P64::Coll
