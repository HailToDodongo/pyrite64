/**
 * @file contact.h
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Defines Contact Points as well as Contact Constraints
 */
#pragma once

#include "vec_math.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

namespace P64 { class Object; }

namespace P64::Coll {

  constexpr int MAX_CONTACT_POINTS_PER_PAIR = 3;

  struct RigidBody; // forward declare
  struct Collider;  // forward declare
  struct MeshCollider;  // forward declare

  enum class ContactConstraintKeyType : uint8_t {
    None = 0,
    ColliderPair,
    ColliderMesh,
    ColliderMeshTriangle,
  };

  struct ContactConstraintKey {
    ContactConstraintKeyType type{ContactConstraintKeyType::None};
    Collider *colliderA{nullptr};
    Collider *colliderB{nullptr};
    MeshCollider *meshCollider{nullptr};
    uint16_t triangleIndex{0};

    bool operator==(const ContactConstraintKey &other) const {
      return type == other.type &&
             colliderA == other.colliderA &&
             colliderB == other.colliderB &&
             meshCollider == other.meshCollider &&
             triangleIndex == other.triangleIndex;
    }
  };

  struct ContactConstraintKeyHash {
    std::size_t operator()(const ContactConstraintKey &key) const {
      std::size_t hash = static_cast<std::size_t>(key.type);
      const auto combine = [&hash](std::size_t value) {
        hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
      };

      combine(reinterpret_cast<std::uintptr_t>(key.colliderA));
      combine(reinterpret_cast<std::uintptr_t>(key.colliderB));
      combine(reinterpret_cast<std::uintptr_t>(key.meshCollider));
      combine(static_cast<std::size_t>(key.triangleIndex));
      return hash;
    }
  };

  inline bool shouldSwapColliderPairOrder(Collider *colliderA, Collider *colliderB) {
    return std::less<const Collider *>{}(colliderB, colliderA);
  }

  inline ContactConstraintKey makeColliderPairConstraintKey(Collider *colliderA, Collider *colliderB) {
    if(shouldSwapColliderPairOrder(colliderA, colliderB)) {
      std::swap(colliderA, colliderB);
    }

    ContactConstraintKey key;
    key.type = ContactConstraintKeyType::ColliderPair;
    key.colliderA = colliderA;
    key.colliderB = colliderB;
    return key;
  }

  inline ContactConstraintKey makeColliderMeshConstraintKey(Collider *collider, MeshCollider *meshCollider, uint16_t triangleIndex) {
    ContactConstraintKey key;
    key.type = ContactConstraintKeyType::ColliderMeshTriangle;
    key.colliderA = collider;
    key.meshCollider = meshCollider;
    key.triangleIndex = triangleIndex;
    return key;
  }

  inline ContactConstraintKey makeColliderMeshConstraintKey(Collider *collider, MeshCollider *meshCollider) {
    ContactConstraintKey key;
    key.type = ContactConstraintKeyType::ColliderMesh;
    key.colliderA = collider;
    key.meshCollider = meshCollider;
    return key;
  }

  /// Linked-list node for tracking contacts on a physics object
  struct Contact {
    struct ContactConstraint *constraint{nullptr};
    RigidBody *otherBody{nullptr};
  };

  /// Single contact point within a contact constraint
  struct ContactPoint {
    fm_vec3_t point{};
    fm_vec3_t contactA{};
    fm_vec3_t contactB{};
    fm_vec3_t localPointA{};
    fm_vec3_t localPointB{};
    fm_vec3_t aToContact{};
    fm_vec3_t bToContact{};
    float penetration{0.0f};

    float accumulatedNormalImpulse{0.0f};
    float accumulatedTangentImpulseU{0.0f};
    float accumulatedTangentImpulseV{0.0f};
    float normalMass{0.0f};
    float tangentMassU{0.0f};
    float tangentMassV{0.0f};
    float velocityBias{0.0f};

    bool active{false};
  };

  /// Contact constraint representing a pair of colliding rigid bodies
  struct ContactConstraint {
    ContactConstraintKey key{};
    RigidBody *rigidBodyA{nullptr};
    Collider *colliderA{nullptr};
    MeshCollider *meshColliderA{nullptr};
    Object *objectA{nullptr};
    RigidBody *rigidBodyB{nullptr};
    Collider *colliderB{nullptr};
    MeshCollider *meshColliderB{nullptr};
    Object *objectB{nullptr};

    fm_vec3_t normal{};
    fm_vec3_t tangentU{};
    fm_vec3_t tangentV{};
    fm_vec3_t cachedSeparatingAxis{}; // Cached GJK separating axis for faster convergence

    float combinedFriction{0.0f};
    float combinedBounce{0.0f};

    int pointCount{0};
    uint32_t transformVersionA{0};
    uint32_t transformVersionB{0};

    bool isActive{false};
    bool isTrigger{false};
    bool respondsA{true};
    bool respondsB{true};

    ContactPoint points[MAX_CONTACT_POINTS_PER_PAIR]{};
  };

} // namespace P64::Coll
