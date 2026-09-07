/**
 * @file mesh_collider.cpp
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Mesh Collider definitions and functions (see meshCollider.h)
 */
#include "collision/meshCollider.h"
#include "collision/colliderShape.h"
#include "scene/object.h"
#include "collision/epa.h"

namespace P64::Coll {

  namespace {
    struct RawCollisionHeader {
      uint32_t triCount;
      uint32_t vertCount;
      float collScale;
      uint32_t vertexPtr;
      uint32_t normalsPtr;
      uint32_t bvhPtr;
    };

    struct PackedNormal {
      int16_t v[3];
    };

    char *alignPtr(char *ptr, size_t alignment) {
      return reinterpret_cast<char *>((reinterpret_cast<uintptr_t>(ptr) + alignment - 1) & ~(alignment - 1));
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
    if(aabbTree_.root != NULL_NODE) {
      const AABB *rootBounds = aabbTree_.getNodeBounds(aabbTree_.root);
      if(rootBounds) {
        localRootAabb_ = *rootBounds;
        return;
      }
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

  // ── Load Mesh Collider from Raw Data and build AABB Tree ────────────────────────────────────────

  MeshCollider *MeshCollider::createFromRawData(void *rawData, Object *obj) {
    if(!rawData) return nullptr;
    if(!obj) return nullptr;

    auto *header = static_cast<RawCollisionHeader *>(rawData);
    if(header->triCount == 0 || header->vertCount == 0) return nullptr;
    if(header->triCount > 0xFFFFu || header->vertCount > 0xFFFFu) return nullptr;

    char *data = reinterpret_cast<char *>(header + 1);

    auto *indexData = reinterpret_cast<uint16_t *>(data);
    data += header->triCount * sizeof(uint16_t) * 3;

    data = alignPtr(data, 4);
    auto *normalData = reinterpret_cast<PackedNormal *>(data);

    data += header->triCount * sizeof(PackedNormal);
    data = alignPtr(data, 4);
    auto *vertexData = reinterpret_cast<fm_vec3_t *>(data);

    auto *collider = new MeshCollider();

    collider->triangleCount_ = static_cast<uint16_t>(header->triCount);
    collider->vertexCount_ = static_cast<uint16_t>(header->vertCount);

    // Copy vertex data
    collider->vertices_ = new fm_vec3_t[header->vertCount];
    for(uint32_t i = 0; i < header->vertCount; ++i) {
      collider->vertices_[i] = vertexData[i];
    }

    // Copy triangle indices
    collider->triangles_ = new MeshTriangleIndices[header->triCount];
    for(uint32_t t = 0; t < header->triCount; ++t) {
      collider->triangles_[t].indices[0] = indexData[t * 3 + 0];
      collider->triangles_[t].indices[1] = indexData[t * 3 + 1];
      collider->triangles_[t].indices[2] = indexData[t * 3 + 2];
    }

    // Convert packed normals (int16_t scaled by 32767) to fm_vec3_t
    constexpr float NORM_SCALE = 1.0f / 32767.0f;
    collider->normals_ = new fm_vec3_t[header->triCount];
    for(uint32_t t = 0; t < header->triCount; ++t) {
      collider->normals_[t] = fm_vec3_t{{
        static_cast<float>(normalData[t].v[0]) * NORM_SCALE,
        static_cast<float>(normalData[t].v[1]) * NORM_SCALE,
        static_cast<float>(normalData[t].v[2]) * NORM_SCALE
      }};
    }

    // Bind to owner object
    collider->owner_ = obj;

    buildAabbTree(collider);
    collider->syncOwnerTransform();

    return collider;
  }

  // Build AABB tree from triangle bounding boxes
  // Need 2*N-1 internal nodes for N leaves, plus some margin
  void MeshCollider::buildAabbTree(MeshCollider* collider) {
    int treeCapacity = static_cast<int>(collider->triangleCount_) * 2 + 1;
    collider->aabbTree_.init(treeCapacity);

    for(uint32_t t = 0; t < collider->triangleCount_; ++t) {
      const fm_vec3_t &v0 = collider->vertices_[collider->triangles_[t].indices[0]];
      const fm_vec3_t &v1 = collider->vertices_[collider->triangles_[t].indices[1]];
      const fm_vec3_t &v2 = collider->vertices_[collider->triangles_[t].indices[2]];

      AABB triAABB;
      triAABB.min = vec3Min(vec3Min(v0, v1), v2);
      triAABB.max = vec3Max(vec3Max(v0, v1), v2);

      // Store triangle index + 1 as data pointer (index 0 would be nullptr and get skipped)
      collider->aabbTree_.createNode(triAABB, reinterpret_cast<void *>(static_cast<intptr_t>(t + 1)));
    }

    collider->computeLocalRootAabb();
    // syncOwnerTransform() first because recalculateWorldAabb() depends on the cached has*() properties
    collider->syncOwnerTransform();
    collider->recalculateWorldAabb();
  }

  MeshCollider* MeshCollider::create(fm_vec3_t* vertices, uint16_t vertexCount, MeshTriangleIndices* triangleIndices, uint16_t triangleCount, Object *owner) {
    if (!vertices || vertexCount == 0 || !triangleIndices || triangleCount == 0) return nullptr;

    auto *collider = new MeshCollider();
    collider->vertices_ = vertices;
    collider->vertexCount_ = vertexCount;
    collider->triangles_ = triangleIndices;
    collider->triangleCount_ = triangleCount;
    collider->owner_ = owner;

    collider->normals_ = new fm_vec3_t[triangleCount];
    for (uint16_t t = 0; t < triangleCount; ++t) {
      const auto& indices = triangleIndices[t].indices;
      fm_vec3_t v0 = vertices[indices[0]];
      fm_vec3_t v1 = vertices[indices[1]];
      fm_vec3_t v2 = vertices[indices[2]];
      collider->normals_[t] = triangleNormalFromVertices(v0, v1, v2);
    }

    buildAabbTree(collider);
    collider->syncOwnerTransform();

    return collider;
  }

  void MeshCollider::destroyData() {
    aabbTree_.destroy();
    delete[] vertices_;
    delete[] triangles_;
    delete[] normals_;
    vertices_ = nullptr;
    triangles_ = nullptr;
    normals_ = nullptr;
    triangleCount_ = 0;
    vertexCount_ = 0;
  }

} // namespace P64::Coll
