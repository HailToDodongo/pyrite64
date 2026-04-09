/**
 * @file mesh_collider.cpp
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Mesh Collider definitions and functions (see mesh_collider.h)
 */
#include "collision/mesh_collider.h"
#include "collision/collider_shape.h"
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
      if(hasRotation()) {
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
    fm_vec3_t position = owner_ ? owner_->pos : VEC3_ZERO;
    fm_quat_t rotation = owner_ ? owner_->rot : QUAT_IDENTITY;
    fm_vec3_t scale = owner_ ? owner_->scale : fm_vec3_t{{1.0f, 1.0f, 1.0f}};
    fm_vec3_t scaled = localPoint * scale;
    if(!quatIsIdentical(&rotation, &QUAT_IDENTITY)) {
      scaled = rotation * scaled;
    }
    if(fm_vec3_len2(&position) > FM_EPSILON * FM_EPSILON) {
      scaled = scaled + position;
    }
    return scaled;
  }

  fm_vec3_t MeshCollider::toLocalSpace(const fm_vec3_t &worldPoint) const {
    fm_vec3_t p = worldPoint;
    fm_vec3_t scale = owner_ ? owner_->scale : fm_vec3_t{{1.0f, 1.0f, 1.0f}};
    if(hasPosition()) {
      p = p - owner_->pos;
    }
    if(hasRotation()) {
      p = quatConjugate(owner_->rot) * p;
    }
    if(hasScale()) {
      if(fabsf(scale.x) > FM_EPSILON) p.x /= scale.x;
      if(fabsf(scale.y) > FM_EPSILON) p.y /= scale.y;
      if(fabsf(scale.z) > FM_EPSILON) p.z /= scale.z;
    }
    return p;
  }

  fm_vec3_t MeshCollider::rotateToWorld(const fm_vec3_t &localDir) const {
    fm_vec3_t worldDirection = localDir;
    if(hasScale() && owner_) 
      worldDirection = worldDirection * owner_->scale;
    if(hasRotation()) 
      worldDirection = owner_->rot * worldDirection;
    return worldDirection;
  }

  fm_vec3_t MeshCollider::rotateToLocal(const fm_vec3_t &worldDir) const {
    fm_vec3_t localDirection = worldDir;
    if(hasRotation()) 
      localDirection = quatConjugate(owner_->rot) * worldDir;
    if (hasScale())
      localDirection = localDirection * vec3ReciprocalScaleComponents(owner_->scale);

    return localDirection;
  }

  bool MeshCollider::hasTransform() const {
    return (hasRotation() || hasPosition() || hasScale());
  }

  bool MeshCollider::hasRotation() const {
    if(!owner_) return false;
    return !quatIsIdentical(&owner_->rot, &QUAT_IDENTITY);
  }

  bool MeshCollider::hasPosition() const {
    if(!owner_) return false;
    return fm_vec3_len2(&owner_->pos) > FM_EPSILON * FM_EPSILON;
  }

  bool MeshCollider::hasScale() const {
    if(!owner_) return false;
    return (fabsf(owner_->scale.x - 1.0f) > FM_EPSILON) || (fabsf(owner_->scale.y - 1.0f) > FM_EPSILON) || (fabsf(owner_->scale.z - 1.0f) > FM_EPSILON);
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

    if(fm_vec3_distance2(&owner_->pos, &lastOwnerPosition_) > FM_EPSILON * FM_EPSILON) return true;
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
    // Transform all 8 corners of the local AABB to world space and take the enclosing AABB
    fm_vec3_t corners[8] = {
      fm_vec3_t{{localRootAabb_.min.x, localRootAabb_.min.y, localRootAabb_.min.z}},
      fm_vec3_t{{localRootAabb_.max.x, localRootAabb_.min.y, localRootAabb_.min.z}},
      fm_vec3_t{{localRootAabb_.min.x, localRootAabb_.max.y, localRootAabb_.min.z}},
      fm_vec3_t{{localRootAabb_.max.x, localRootAabb_.max.y, localRootAabb_.min.z}},
      fm_vec3_t{{localRootAabb_.min.x, localRootAabb_.min.y, localRootAabb_.max.z}},
      fm_vec3_t{{localRootAabb_.max.x, localRootAabb_.min.y, localRootAabb_.max.z}},
      fm_vec3_t{{localRootAabb_.min.x, localRootAabb_.max.y, localRootAabb_.max.z}},
      fm_vec3_t{{localRootAabb_.max.x, localRootAabb_.max.y, localRootAabb_.max.z}},
    };

    fm_vec3_t worldMin = toWorldSpace(corners[0]);
    fm_vec3_t worldMax = worldMin;
    for(int i = 1; i < 8; ++i) {
      fm_vec3_t w = toWorldSpace(corners[i]);
      worldMin = vec3Min(worldMin, w);
      worldMax = vec3Max(worldMax, w);
    }
    worldAabb_ = {worldMin, worldMax};
  }

  AABB MeshCollider::worldAabbToLocal(const AABB &worldAabb) const {
    // Transform all 8 corners of the world AABB into local space
    fm_vec3_t corners[8] = {
      fm_vec3_t{{worldAabb.min.x, worldAabb.min.y, worldAabb.min.z}},
      fm_vec3_t{{worldAabb.max.x, worldAabb.min.y, worldAabb.min.z}},
      fm_vec3_t{{worldAabb.min.x, worldAabb.max.y, worldAabb.min.z}},
      fm_vec3_t{{worldAabb.max.x, worldAabb.max.y, worldAabb.min.z}},
      fm_vec3_t{{worldAabb.min.x, worldAabb.min.y, worldAabb.max.z}},
      fm_vec3_t{{worldAabb.max.x, worldAabb.min.y, worldAabb.max.z}},
      fm_vec3_t{{worldAabb.min.x, worldAabb.max.y, worldAabb.max.z}},
      fm_vec3_t{{worldAabb.max.x, worldAabb.max.y, worldAabb.max.z}},
    };

    fm_vec3_t localMin = toLocalSpace(corners[0]);
    fm_vec3_t localMax = localMin;
    for(int i = 1; i < 8; ++i) {
      fm_vec3_t l = toLocalSpace(corners[i]);
      localMin = vec3Min(localMin, l);
      localMax = vec3Max(localMax, l);
    }
    return {localMin, localMax};
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

    // Build AABB tree from triangle bounding boxes
    // Need 2*N-1 internal nodes for N leaves, plus some margin
    int treeCapacity = static_cast<int>(header->triCount) * 2 + 1;
    collider->aabbTree_.init(treeCapacity);

    for(uint32_t t = 0; t < header->triCount; ++t) {
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
    collider->recalculateWorldAabb();
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
