/**
 * @file mesh_collider.cpp
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Mesh Collider definitions and functions (see meshCollider.h)
 */
#include "collision/meshCollider.h"
#include "collision/colliderShape.h"
#include "scene/object.h"
#include "collision/epa.h"
#include "collision/meshBvhBuilder.h"

namespace P64::Coll {

  namespace {
    static_assert(sizeof(fm_vec3_t) == 3 * sizeof(float));
    static_assert(alignof(fm_vec3_t) <= 4);
    static_assert(sizeof(MeshTriangleIndices) == 3 * sizeof(uint16_t));

    const char *alignPtr(const char *ptr, size_t alignment) {
      return reinterpret_cast<const char *>((reinterpret_cast<uintptr_t>(ptr) + alignment - 1) & ~(alignment - 1));
    }

    AABB nodeBounds(const MeshBvhNode &node) {
      return {fm_vec3_t{{node.min[0], node.min[1], node.min[2]}},
              fm_vec3_t{{node.max[0], node.max[1], node.max[2]}}};
    }
  }

  // ── MeshTriangle ──────────────────────────────────────────────────

  fm_vec3_t MeshTriangle::localVertex(int localIndex) const {
    const uint16_t vertexIndex = tri.indices[localIndex];
    if(vertices) return vertices[vertexIndex];
    return mesh ? mesh->vertex(vertexIndex) : VEC3_ZERO;
  }

  fm_vec3_t MeshTriangle::worldVertex(int localIndex) const {
    const fm_vec3_t v = localVertex(localIndex);
    if(mesh) return mesh->toWorldSpace(v);
    return v;
  }

  fm_vec3_t MeshTriangle::worldNormal() const {
    if(mesh) return mesh->localNormalToWorld(normal);
    return normal;
  }

  void MeshTriangle::gjkSupport(const fm_vec3_t &direction, fm_vec3_t &output) const {
    fm_vec3_t v0 = localVertex(0);
    fm_vec3_t v1 = localVertex(1);
    fm_vec3_t v2 = localVertex(2);

    float d0 = fm_vec3_dot(&v0, &direction);
    float d1 = fm_vec3_dot(&v1, &direction);
    float d2 = fm_vec3_dot(&v2, &direction);

    if(d0 >= d1 && d0 >= d2) {
      output = v0;
    } else if(d1 >= d2) {
      output = v1;
    } else {
      output = v2;
    }
  }

  float MeshTriangle::comparePoint(const fm_vec3_t &point) const {
    fm_vec3_t w0 = worldVertex(0);
    fm_vec3_t wn = worldNormal();
    fm_vec3_t diff = point - w0;
    return fm_vec3_dot(&wn, &diff);
  }

  void meshTriangleGjkSupport(const void *data, const fm_vec3_t &direction, fm_vec3_t &output) {
    auto *tri = static_cast<const MeshTriangle *>(data);
    tri->gjkSupport(direction, output);
  }


  fm_vec3_t MeshCollider::localNormalToWorld(const fm_vec3_t &localNormal) const {
    fm_vec3_t worldNormal = localNormal;
    if(owner_) {
      worldNormal = worldNormal * vec3ReciprocalScaleComponents(owner_->scale);
      if(hasRotation_) {
        worldNormal = owner_->rot * worldNormal;
      }
    }
    return vec3NormalizeOrFallback(worldNormal, VEC3_UP);
  }

  void MeshCollider::localResultToWorld(EpaResult &result) const {
    result.normal = localNormalToWorld(result.normal);
    result.contactA = toWorldSpace(result.contactA);
    result.contactB = toWorldSpace(result.contactB);
    // Recompute penetration from world-space contacts.
    // The raw penetration is in mesh-local space where distances are distorted by the mesh scale
    fm_vec3_t ab = result.contactB - result.contactA;
    result.penetration = fm_vec3_dot(&ab, &result.normal);
  }

  // ── MeshCollider transform ────────────────────────────────────────

  fm_vec3_t MeshCollider::toWorldSpace(const fm_vec3_t &localPoint) const {
    if(!owner_) return localPoint;
    fm_vec3_t p = localPoint * owner_->scale;
    if(hasRotation_) {
      p = owner_->rot * p;
    }
    if(hasPosition_) {
      p = p + owner_->pos;
    }
    return p;
  }

  fm_vec3_t MeshCollider::toLocalSpace(const fm_vec3_t &worldPoint) const {
    fm_vec3_t p = worldPoint;
    if(!owner_) return p;
    if(hasPosition_) {
      p = p - owner_->pos;
    }
    if(hasRotation_) {
      p = quatConjugate(owner_->rot) * p;
    }
    if(hasScale_) {
      const fm_vec3_t &scale = owner_->scale;
      if(fabsf(scale.x) > FM_EPSILON) p.x /= scale.x;
      if(fabsf(scale.y) > FM_EPSILON) p.y /= scale.y;
      if(fabsf(scale.z) > FM_EPSILON) p.z /= scale.z;
    }
    return p;
  }

  fm_vec3_t MeshCollider::rotateToWorld(const fm_vec3_t &localDir) const {
    fm_vec3_t worldDirection = localDir;
    if(hasScale_ && owner_)
      worldDirection = worldDirection * owner_->scale;
    if(hasRotation_)
      worldDirection = owner_->rot * worldDirection;
    return worldDirection;
  }

  fm_vec3_t MeshCollider::rotateToLocal(const fm_vec3_t &worldDir) const {
    fm_vec3_t localDirection = worldDir;
    if(hasRotation_)
      localDirection = quatConjugate(owner_->rot) * worldDir;
    if(hasScale_)
      localDirection = localDirection * vec3ReciprocalScaleComponents(owner_->scale);

    return localDirection;
  }

  bool MeshCollider::readsCollider(const Collider *other) const {
    return other && ((readMask_ & other->writeMask()) != 0);
  }

  bool MeshCollider::readsMeshCollider(const MeshCollider *other) const {
    return other && ((readMask_ & other->writeMask_) != 0);
  }

  bool MeshCollider::hasOwnerTransformChanged() const {
    if(!owner_) return false;
    if(!hasCachedOwnerTransform_) return true;

  fm_vec3_t ownerPhysicsPos = owner_->pos;
    if(fm_vec3_distance2(&ownerPhysicsPos, &lastOwnerPosition_) > FM_EPSILON * FM_EPSILON) return true;
    if(fm_vec3_distance2(&owner_->scale, &lastOwnerScale_) > FM_EPSILON * FM_EPSILON) return true;

    const float rotSim = fabsf(quatDot(owner_->rot, lastOwnerRotation_));
    return rotSim < (1.0f - FM_EPSILON);
  }

  void MeshCollider::syncOwnerTransform() {
    if(!owner_) {
      lastOwnerPosition_ = VEC3_ZERO;
      lastOwnerRotation_ = QUAT_IDENTITY;
      lastOwnerScale_ = fm_vec3_t{{1.0f, 1.0f, 1.0f}};
    } else {
      lastOwnerPosition_ = owner_->pos;
      lastOwnerRotation_ = owner_->rot;
      lastOwnerScale_ = owner_->scale;
    }

    // Cached because they are used frequently in the hot loops. They only say
    // whether a transform component is present, so lagging behind the owner potentially transforming by at most
    // one physics step is harmless. Anything that needs the actual transform reads the owner.
    hasRotation_ = owner_ && !quatIsIdentical(&lastOwnerRotation_, &QUAT_IDENTITY);
    hasPosition_ = owner_ && fm_vec3_len2(&lastOwnerPosition_) > FM_EPSILON * FM_EPSILON;
    hasScale_ = owner_ && ((fabsf(lastOwnerScale_.x - 1.0f) > FM_EPSILON) ||
                           (fabsf(lastOwnerScale_.y - 1.0f) > FM_EPSILON) ||
                           (fabsf(lastOwnerScale_.z - 1.0f) > FM_EPSILON));
    hasTransform_ = hasRotation_ || hasPosition_ || hasScale_;

    inverseRotationMatrix_ = quatToMatrix3(quatConjugate(lastOwnerRotation_));
    hasCachedOwnerTransform_ = true;
    ++worldTransformVersion_;
  }

  void MeshCollider::computeLocalRootAabb() {
    if(triangleBvh_) {
      localRootAabb_ = nodeBounds(triangleBvh_[0]);
      return;
    }
    // Fallback: compute from vertices
    if(vertexCount_ == 0) return;
    fm_vec3_t minV = vertices_[0];
    fm_vec3_t maxV = vertices_[0];
    for(int i = 1; i < vertexCount_; ++i) {
      minV = vec3Min(minV, vertices_[i]);
      maxV = vec3Max(maxV, vertices_[i]);
    }
    localRootAabb_ = {minV, maxV};
  }

  void MeshCollider::recalculateWorldAabb() {
    // The AABB of an affine-transformed box:
    // For M = R * diag(scale) this is the min/max over the 8 transformed corners
    // (Ericson, Real-Time Collision Detection 4.2.6)
    const fm_vec3_t localCenter = (localRootAabb_.min + localRootAabb_.max) * 0.5f;
    const fm_vec3_t localHalf   = (localRootAabb_.max - localRootAabb_.min) * 0.5f;

    const fm_vec3_t worldCenter = toWorldSpace(localCenter);
    const fm_vec3_t scale = owner_ ? owner_->scale : fm_vec3_t{{1.0f, 1.0f, 1.0f}};

    fm_vec3_t worldHalf;
    if(hasRotation_) {
      const Matrix3x3 r = quatToMatrix3(owner_->rot);
      worldHalf = fm_vec3_t{{
        fabsf(r.m[0][0] * scale.x) * localHalf.x + fabsf(r.m[0][1] * scale.y) * localHalf.y + fabsf(r.m[0][2] * scale.z) * localHalf.z,
        fabsf(r.m[1][0] * scale.x) * localHalf.x + fabsf(r.m[1][1] * scale.y) * localHalf.y + fabsf(r.m[1][2] * scale.z) * localHalf.z,
        fabsf(r.m[2][0] * scale.x) * localHalf.x + fabsf(r.m[2][1] * scale.y) * localHalf.y + fabsf(r.m[2][2] * scale.z) * localHalf.z
      }};
    } else {
      worldHalf = fm_vec3_t{{
        fabsf(scale.x) * localHalf.x,
        fabsf(scale.y) * localHalf.y,
        fabsf(scale.z) * localHalf.z
      }};
    }

    worldAabb_ = {worldCenter - worldHalf, worldCenter + worldHalf};
  }

  AABB MeshCollider::worldAabbToLocal(const AABB &worldAabb) const {
    // Same |M| construction as recalculateWorldAabb(), on the inverse map M^-1 = diag(1/scale) * R^T.
    // Identical box to transforming all 8 corners through toLocalSpace().
    const fm_vec3_t center = (worldAabb.min + worldAabb.max) * 0.5f;
    const fm_vec3_t half   = (worldAabb.max - worldAabb.min) * 0.5f;

    const fm_vec3_t localCenter = toLocalSpace(center);

    // Degenerate axes stay at 1 so they pass through
    fm_vec3_t invScale{{1.0f, 1.0f, 1.0f}};
    if(hasScale_ && owner_) {
      const fm_vec3_t &scale = owner_->scale;
      if(fabsf(scale.x) > FM_EPSILON) invScale.x = 1.0f / scale.x;
      if(fabsf(scale.y) > FM_EPSILON) invScale.y = 1.0f / scale.y;
      if(fabsf(scale.z) > FM_EPSILON) invScale.z = 1.0f / scale.z;
    }

    fm_vec3_t localHalf;
    if(hasRotation_) {
      const Matrix3x3 ri = quatToMatrix3(quatConjugate(owner_->rot));
      localHalf = fm_vec3_t{{
        fabsf(invScale.x) * (fabsf(ri.m[0][0]) * half.x + fabsf(ri.m[0][1]) * half.y + fabsf(ri.m[0][2]) * half.z),
        fabsf(invScale.y) * (fabsf(ri.m[1][0]) * half.x + fabsf(ri.m[1][1]) * half.y + fabsf(ri.m[1][2]) * half.z),
        fabsf(invScale.z) * (fabsf(ri.m[2][0]) * half.x + fabsf(ri.m[2][1]) * half.y + fabsf(ri.m[2][2]) * half.z)
      }};
    } else {
      localHalf = fm_vec3_t{{
        fabsf(invScale.x) * half.x,
        fabsf(invScale.y) * half.y,
        fabsf(invScale.z) * half.z
      }};
    }

    return {localCenter - localHalf, localCenter + localHalf};
  }

  int MeshCollider::queryTriangles(const AABB &localBounds, uint16_t *outCandidates, int maxCandidates) const {
    return queryMeshBvh(triangleBvh_, meshBvhNodeCount(triangleCount_), outCandidates, maxCandidates,
      [&](const MeshBvhNode &node) { return aabbOverlap(nodeBounds(node), localBounds); });
  }

  int MeshCollider::queryTriangles(const Raycast &localRay, uint16_t *outCandidates, int maxCandidates) const {
    return queryMeshBvh(triangleBvh_, meshBvhNodeCount(triangleCount_), outCandidates, maxCandidates,
      [&](const MeshBvhNode &node) { return aabbIntersectsRay(nodeBounds(node), localRay); });
  }

  // ── Load Mesh Collider with the BVH built by the Pyrite Editor ──────────────────────────────────

  MeshCollider *MeshCollider::createFromRawData(const void *rawData, Object *obj) {
    if(!rawData) return nullptr;
    if(!obj) return nullptr;

    auto *header = static_cast<const RawCollisionHeader *>(rawData);
    if(header->triCount == 0 || header->vertCount == 0) return nullptr;
    if(header->triCount > 0xFFFFu || header->vertCount > 0xFFFFu) return nullptr;

    const char *data = reinterpret_cast<const char *>(header + 1);

    auto *indexData = reinterpret_cast<const MeshTriangleIndices *>(data);
    data += header->triCount * sizeof(MeshTriangleIndices);

    data = alignPtr(data, 4);
    auto *normalData = reinterpret_cast<const fm_vec3_t *>(data);

    data += header->triCount * sizeof(fm_vec3_t);
    data = alignPtr(data, 4);
    auto *vertexData = reinterpret_cast<const fm_vec3_t *>(data);

    data += header->vertCount * sizeof(fm_vec3_t);
    data = alignPtr(data, 4);
    // Reject pre-BVH and packed-normal assets; rebuilding the ROM generates the current layout.
    if(header->bvhOffset != static_cast<uint32_t>(data - static_cast<const char *>(rawData))) return nullptr;

    auto *collider = new MeshCollider();

    collider->triangleCount_ = static_cast<uint16_t>(header->triCount);
    collider->vertexCount_ = static_cast<uint16_t>(header->vertCount);

    collider->vertices_ = vertexData;
    collider->triangles_ = indexData;
    collider->normals_ = normalData;

    // Bind to owner object
    collider->owner_ = obj;

    collider->triangleBvh_ = reinterpret_cast<const MeshBvhNode *>(data);
    collider->computeLocalRootAabb();
    collider->syncOwnerTransform();
    collider->recalculateWorldAabb();

    return collider;
  }

  MeshCollider* MeshCollider::create(fm_vec3_t* vertices, uint16_t vertexCount, MeshTriangleIndices* triangleIndices, uint16_t triangleCount, Object *owner) {
    if (!vertices || vertexCount == 0 || !triangleIndices || triangleCount == 0) return nullptr;

    auto *collider = new MeshCollider();
    collider->vertices_ = vertices;
    collider->vertexCount_ = vertexCount;
    collider->triangles_ = triangleIndices;
    collider->triangleCount_ = triangleCount;
    collider->owner_ = owner;
    collider->ownsGeometry_ = true;

    auto *normals = new fm_vec3_t[triangleCount];
    collider->normals_ = normals;
    for (uint16_t t = 0; t < triangleCount; ++t) {
      const auto& indices = triangleIndices[t].indices;
      fm_vec3_t v0 = vertices[indices[0]];
      fm_vec3_t v1 = vertices[indices[1]];
      fm_vec3_t v2 = vertices[indices[2]];
      normals[t] = triangleNormalFromVertices(v0, v1, v2);
    }

    collider->ownedTriangleBvh_ = buildMeshBvh(triangleCount, [&](uint16_t t) {
      const auto &indices = triangleIndices[t].indices;
      const auto minV = vec3Min(vec3Min(vertices[indices[0]], vertices[indices[1]]), vertices[indices[2]]);
      const auto maxV = vec3Max(vec3Max(vertices[indices[0]], vertices[indices[1]]), vertices[indices[2]]);
      return MeshBvhNode{{minV.x, minV.y, minV.z}, {maxV.x, maxV.y, maxV.z}, 0};
    });
    collider->triangleBvh_ = collider->ownedTriangleBvh_.get();
    collider->computeLocalRootAabb();
    collider->syncOwnerTransform();
    collider->recalculateWorldAabb();

    return collider;
  }

  void MeshCollider::destroyData() {
    ownedTriangleBvh_.reset();
    triangleBvh_ = nullptr;
    if(ownsGeometry_) {
      delete[] vertices_;
      delete[] triangles_;
      delete[] normals_;
    }
    ownsGeometry_ = false;
    vertices_ = nullptr;
    triangles_ = nullptr;
    normals_ = nullptr;
    triangleCount_ = 0;
    vertexCount_ = 0;
  }

} // namespace P64::Coll
