/**
 * @file collision_scene.h
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Defines the Collision Scene which keeps track of physics participants and updates them
 */
#pragma once

#include "rigid_body.h"
#include "collider_shape.h"
#include "mesh_collider.h"
#include "contact.h"
#include "aabb_tree.h"
#include "raycast.h"
#include <array>
#include <deque>
#include <functional>
#include <cstddef>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace P64::Coll {
  constexpr int MAX_OBJ_COLLISION_CANDIDATES = 15;
  constexpr float DEFAULT_FIXED_DT = 1.0f / 50.0f;
  constexpr float DEFAULT_PHYSICS_SCALE = 16.0f;
  constexpr fm_vec3_t DEFAULT_GRAVITY = {0.0f, -9.8f, 0.0f}; //scaled with Pyrites default scale for assets
  constexpr uint8_t DEFAULT_VELOCITY_SOLVER_ITERATIONS = 8;
  constexpr uint8_t DEFAULT_POSITION_SOLVER_ITERATIONS = 7;
  constexpr float WARM_STARTING_FACTOR = 0.85f; // Bullet-style warm starting scale to prevent overcorrection from stale impulses
  constexpr int MAX_CCD_SUBSTEPS = 4; // Maximum substep passes for swept detection of fast-moving bodies

  struct CollEvent
  {
    Collider *selfCollider{};
    Collider *hitCollider{};
    MeshCollider *selfMeshCollider{};
    MeshCollider *hitMeshCollider{};
    RigidBody *selfRigidBody{};
    RigidBody *hitRigidBody{};
    uint16_t contactCount{0};
    std::array<ContactPoint, MAX_CONTACT_POINTS_PER_PAIR> contacts{};
    Object *otherObject{};
  };

  class CollisionScene {
  public:
    uint64_t ticksWakePrep{0};
    uint64_t ticksWorldUpdate{0};
    uint64_t ticksIntegrateVel{0};
    uint64_t ticksDetect{0};
    uint64_t ticksDetectBodyPairs{0};
    uint64_t ticksDetectMeshPairs{0};
    uint64_t ticksRefreshCallbacks{0};
    uint64_t ticksPreSolve{0};
    uint64_t ticksWarmStart{0};
    uint64_t ticksVelocitySolve{0};
    uint64_t ticksIntegration{0};
    uint64_t ticksPositionSolve{0};
    uint64_t ticksFinalize{0};
    uint64_t ticksTotal{0};
    void debugDraw(bool showMeshColliders, bool showRigidBodies);
    void reset();

    void addRigidBody(RigidBody *rigidBody);
    void removeRigidBody(RigidBody *rigidBody);
    RigidBody *findRigidBodyByObjectId(uint16_t id) const;
    const std::vector<RigidBody *> &getRigidBodies() const { return rigidBodies_; }

    void addCollider(Collider *collider);
    void removeCollider(Collider *collider);
    const std::vector<Collider *> &getColliders() const { return colliders_; }

    void addMeshCollider(MeshCollider *mesh);
    void removeMeshCollider(MeshCollider *mesh);

    void configureSimulation(float fixedDt, const fm_vec3_t &gravity, uint8_t velocityIterations, uint8_t positionIterations, float physicsScale);
    void wakeRigidBodyIsland(RigidBody *rigidBody);

    void step();

    float getPhysicsScale() const { return physicsScale_; }

    int getCachedConstraintCount() const;
    ContactConstraint &getCachedConstraint(int index);
    const ContactConstraint &getCachedConstraint(int index) const;
    ContactConstraint *createCachedConstraint(
      const ContactConstraintKey &key,
      RigidBody *rigidBodyA, Collider *colliderA, MeshCollider *meshColliderA, Object *objectA,
      RigidBody *rigidBodyB, Collider *colliderB, MeshCollider *meshColliderB, Object *objectB);
    ContactConstraint *findCachedConstraint(const ContactConstraintKey &key);

    bool raycast(Raycast &ray, RaycastHit &hit) const;

  private:

    std::vector<RigidBody *> rigidBodies_{};
    std::unordered_map<const Object *, RigidBody *> ownerRigidBodies_{};
    std::vector<Collider *> colliders_{};
    std::unordered_map<const Object *, std::vector<Collider *>> ownerColliders_{};
    std::deque<ContactConstraint> cachedConstraints_{};
    std::unordered_map<ContactConstraintKey, int, ContactConstraintKeyHash> cachedConstraintLookup_{};
    std::vector<ContactConstraint *> solverConstraints_{};

    AABBTree colliderAABBTree;

    // Multiple mesh colliders
    std::vector<MeshCollider *> meshColliders_{};

    float fixedDt_{DEFAULT_FIXED_DT};
    float physicsScale_{DEFAULT_PHYSICS_SCALE};
    fm_vec3_t gravity_{DEFAULT_GRAVITY};
    uint8_t velocitySolverIterations_{DEFAULT_VELOCITY_SOLVER_ITERATIONS};
    uint8_t positionSolverIterations_{DEFAULT_POSITION_SOLVER_ITERATIONS};

    int cachedConstraintCount_{0};

    static bool shouldTrackSleepState(const RigidBody *rigidBody);
    static bool rigidBodyTransformExceededSleepThreshold(const RigidBody *rigidBody);
    static bool rigidBodyVelocitiesExceededSleepThreshold(const RigidBody *rigidBody);
    static bool rigidBodyCompoundPropertiesNeedUpdate(const RigidBody *rigidBody);
    RigidBody *findRigidBodyByOwner(const Object *owner) const;
    const std::vector<Collider *> *findCollidersForOwner(const Object *owner) const;
    void updateCompoundProperties(RigidBody *rigidBody) const;
    void syncCompoundProperties(RigidBody *rigidBody) const;
    void collectConnectedIsland(RigidBody *seed, std::vector<RigidBody *> &island, std::unordered_set<RigidBody *> &visited) const;
    static void addWakeCandidate(std::vector<RigidBody *> &wakeCandidates, RigidBody *candidate, RigidBody *ignoredCandidate = nullptr);
    void wakeCandidateIslands(const std::vector<RigidBody *> &wakeCandidates);
    void removeCachedConstraints(
      const std::function<bool(const ContactConstraint &)> &shouldRemove,
      std::vector<RigidBody *> &wakeCandidates,
      RigidBody *ignoredCandidate = nullptr);
    void removeCachedConstraintAt(int index);
    CollEvent makeCollisionEvent(const ContactConstraint &constraint) const;
    void dispatchCollisionCallbacks() const;

    void rebuildCachedConstraintLookup();
    void wakeIsland(RigidBody *rigidBody);
    void wakeBodiesTransformedExternally();
    void updateSleepStates();
    void refreshContacts();
    void removeInactiveContacts();
    void rebuildSolverConstraints();
    void detectAllContacts();
    void preSolveContacts();
    void warmStart();
    void solveVelocityConstraints();
    bool solvePositionConstraints();
    void detectSweptCollisions();
    void updateMeshColliderWorldStates();
  };

  CollisionScene *collisionSceneGetInstance();

} // namespace P64::Coll
