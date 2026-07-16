/**
 * @file collisionScene.h
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Defines the Collision Scene which keeps track of physics participants and updates them
 */
#pragma once

#include "rigidBody.h"
#include "colliderShape.h"
#include "meshCollider.h"
#include "contact.h"
#include "aabbTree.h"
#include "raycast.h"
#include "capsuleSweep.h"
#include "sphereSweep.h"
#include <array>
#include <deque>
#include <functional>
#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>

namespace P64::Coll {
  constexpr int MAX_OBJ_COLLISION_CANDIDATES = 15;
  constexpr float DEFAULT_FIXED_DT = 1.0f / 50.0f;
  constexpr fm_vec3_t DEFAULT_GRAVITY = {0.0f, -9.8f, 0.0f};
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

    static void enableRigidBody(RigidBody *rigidBody);

    /// @brief Temporarily exclude the given RigidBody from the simulation.
    ///
    /// While disabled, the body will be immune to collision (allowing it to pass through objects) and will not receive
    /// changes to force, velocity, or acceleration.
    /// Manually moving the body (via RigidBody::setPosition()/RigidBody::setRotation()) is still allowed.
    /// Call enableRigidBody() to re-enable the RigidBody.
    void disableRigidBody(RigidBody *rigidBody);

    void addCollider(Collider *collider);
    void removeCollider(Collider *collider);
    const std::vector<Collider *> &getColliders() const { return colliders_; }

    void addMeshCollider(MeshCollider *mesh);
    void removeMeshCollider(MeshCollider *mesh);

    void configureSimulation(float fixedDt, const fm_vec3_t &gravity, uint8_t velocityIterations, uint8_t positionIterations, float gfxScale);
    void wakeRigidBodyIsland(RigidBody *rigidBody);

    void step();

    int getCachedConstraintCount() const;
    ContactConstraint &getCachedConstraint(int index);
    const ContactConstraint &getCachedConstraint(int index) const;
    ContactConstraint *createCachedConstraint(
      const ContactConstraintKey &key,
      RigidBody *rigidBodyA, Collider *colliderA, MeshCollider *meshColliderA, Object *objectA,
      RigidBody *rigidBodyB, Collider *colliderB, MeshCollider *meshColliderB, Object *objectB);
    ContactConstraint *findCachedConstraint(const ContactConstraintKey &key);

    bool raycast(Raycast &ray, RaycastHit &hit) const;

    /**
     * Sweeps a capsule through the scene and returns the earliest contact.
     *
     * @param center          Capsule center in physics space
     * @param axisUp          Normalized capsule-axis direction
     * @param radius          Capsule radius
     * @param innerHalfHeight Half-length of the cylindrical section (not including sphere caps)
     * @param displacement    Displacement vector in physics space (not normalized)
     * @param collTypes       Which collider types to test against
     * @param readMask        Collision read mask (@TODO: not implemented)
     * @param hit             Output contact result
     * @return true if any contact was found
     */
    bool capsuleSweep(
      const fm_vec3_t& center,
      const fm_vec3_t& axisUp,
      float radius,
      float innerHalfHeight,
      const fm_vec3_t& displacement,
      RaycastColliderTypeFlags collTypes,
      uint8_t readMask,
      CapsuleSweepHit& hit,
      const Object* ignoreOwner = nullptr
    ) const;

    bool sphereSweep(
      const fm_vec3_t& center,
      float radius,
      const fm_vec3_t& displacement,
      RaycastColliderTypeFlags collTypes,
      uint8_t readMask,
      SphereSweepHit& hit,
      const Object* ignoreOwner = nullptr
    ) const;

  private:

    std::vector<RigidBody *> rigidBodies_{};
    std::unordered_map<const Object *, RigidBody *> ownerRigidBodies_{};
    std::vector<Collider *> colliders_{};
    std::unordered_map<const Object *, std::vector<Collider *>> ownerColliders_{};
    std::deque<ContactConstraint> cachedConstraints_{};
    std::unordered_map<ContactConstraintKey, int, ContactConstraintKeyHash> cachedConstraintLookup_{};
    std::vector<ContactConstraint *> solverConstraints_{};

    // Contact solver working data (rebuilt each step in preSolveContacts)
    // preSolveContacts bakes each side's per-unit-impulse response, so warm start and the solver loops are just multiply-adds

    /// @brief Per-body data for the solver. Index 0 is a sentinel for the static side of a constraint (null/immovable), so the loops need no null checks.
    struct SolverBody {
      fm_vec3_t linearVelocity{};
      fm_vec3_t angularVelocity{};
      fm_vec3_t pushLinearVelocity{};
      fm_vec3_t pushAngularVelocity{};
      RigidBody *body{nullptr};
    };
    
    /// @brief Per-constraint data for the normal (non-penetration) iterations.
    /// linearResponse* is the velocity change per unit normal impulse, and is zeroed when that side can't respond
    struct SolverConstraintHeader {
      fm_vec3_t normal{};
      fm_vec3_t linearResponseA{};
      fm_vec3_t linearResponseB{};
      uint16_t bodyA{0};
      uint16_t bodyB{0};
      uint16_t pointStart{0};
      uint16_t pointCount{0};
    };

    /// @brief Per-contact point data for the normal iterations (hot loop).
    /// angularMeasure* = r x n, used to read relative velocity at the contact;
    /// angularResponse* = the spin change per unit impulse. Both zeroed when unused.
    struct SolverContactPoint {
      fm_vec3_t angularMeasureA{};
      fm_vec3_t angularMeasureB{};
      fm_vec3_t angularResponseA{};
      fm_vec3_t angularResponseB{};
      float normalMass{0.0f};
      float velocityBias{0.0f};
      float accumulatedImpulse{0.0f};
      float positionBias{0.0f};
      float accumulatedPushImpulse{0.0f};
    };

    /// @brief Per-constraint friction data (solved in one pass after the normal iterations)
    /// linearMeasureScale* is 0 when that side's velocity is excluded (disabled/kinematic)
    struct SolverFrictionHeader {
      fm_vec3_t tangentU{};
      fm_vec3_t tangentV{};
      fm_vec3_t linearResponseUA{};
      fm_vec3_t linearResponseUB{};
      fm_vec3_t linearResponseVA{};
      fm_vec3_t linearResponseVB{};
      float friction{0.0f};
      float linearMeasureScaleA{0.0f};
      float linearMeasureScaleB{0.0f};
    };

    /// @brief Per-point friction data; index-aligned with solverPoints_
    struct SolverFrictionPoint {
      fm_vec3_t angularMeasureUA{};
      fm_vec3_t angularMeasureUB{};
      fm_vec3_t angularMeasureVA{};
      fm_vec3_t angularMeasureVB{};
      fm_vec3_t angularResponseUA{};
      fm_vec3_t angularResponseUB{};
      fm_vec3_t angularResponseVA{};
      fm_vec3_t angularResponseVB{};
      float tangentMassU{0.0f};
      float tangentMassV{0.0f};
      float accumulatedImpulseU{0.0f};
      float accumulatedImpulseV{0.0f};
      ContactPoint *source{nullptr};   // accumulated impulses are written back here for warm starting
    };

    std::vector<SolverBody> solverBodies_{};
    std::vector<SolverConstraintHeader> solverHeaders_{};
    std::vector<SolverContactPoint> solverPoints_{};
    std::vector<SolverFrictionHeader> solverFrictionHeaders_{};
    std::vector<SolverFrictionPoint> solverFrictionPoints_{};
    std::vector<uint16_t> solverOrder_{}; // constraint processing order, shuffled once per step to avoid bias

    AABBTree colliderAABBTree;
    AABBTree meshColliderAABBTree;

    // Multiple mesh colliders
    std::vector<MeshCollider *> meshColliders_{};

    float fixedDt_{DEFAULT_FIXED_DT};
    fm_vec3_t gravity_{DEFAULT_GRAVITY};
    uint8_t velocitySolverIterations_{DEFAULT_VELOCITY_SOLVER_ITERATIONS};
    uint8_t positionSolverIterations_{DEFAULT_POSITION_SOLVER_ITERATIONS};

    int cachedConstraintCount_{0};

    // Reusable per-step scratch state to avoid allocations

    std::vector<NodeProxy> colliderCandidateScratch_{};
    std::vector<NodeProxy> meshCandidateScratch_{};
    std::vector<RigidBody *> islandScratch_{};
    std::vector<RigidBody *> islandStackScratch_{};
    // Monotonic counter for island traversals. Bodies stamped with the current
    // epoch count as "visited" without needing a per-traversal set.
    uint32_t islandVisitEpoch_{0};

    struct PendingPairKey {
      const Object *first{nullptr};
      const Object *second{nullptr};
      int32_t dispatchIndex{-1}; // -1 = neither object has a collision handler
    };
    struct PendingPairDispatch {
      bool wantFirst{false};
      bool wantSecond{false};
      bool hasFirstEvent{false};
      bool hasSecondEvent{false};
      CollEvent firstEvent{};
      CollEvent secondEvent{};
    };
    std::vector<PendingPairKey> pendingDispatchKeys_{};
    std::vector<PendingPairDispatch> pendingDispatchScratch_{};

    static bool shouldTrackSleepState(const RigidBody *rigidBody);
    static bool rigidBodyTransformExceededSleepThreshold(const RigidBody *rigidBody);
    static bool rigidBodyVelocitiesExceededSleepThreshold(const RigidBody *rigidBody);
    static bool rigidBodyCompoundPropertiesNeedUpdate(const RigidBody *rigidBody);
    RigidBody *findRigidBodyByOwner(const Object *owner) const;
    const std::vector<Collider *> *findCollidersForOwner(const Object *owner) const;
    void updateCompoundProperties(RigidBody *rigidBody) const;
    void syncCompoundProperties(RigidBody *rigidBody) const;
    void collectConnectedIsland(RigidBody *seed, std::vector<RigidBody *> &island);
    uint32_t nextIslandEpoch();
    static void addWakeCandidate(std::vector<RigidBody *> &wakeCandidates, RigidBody *candidate, RigidBody *ignoredCandidate = nullptr);
    void wakeCandidateIslands(const std::vector<RigidBody *> &wakeCandidates);
    void removeCachedConstraints(
      const std::function<bool(const ContactConstraint &)> &shouldRemove,
      std::vector<RigidBody *> &wakeCandidates,
      RigidBody *ignoredCandidate = nullptr);
    void removeCachedConstraintAt(int index);
    void dispatchCollisionCallbacks();

    void rebuildCachedConstraintLookup();
    void wakeIsland(RigidBody *rigidBody);
    void wakeBodiesTransformedExternally();
    void updateSleepStates();
    void refreshContacts();
    void removeInactiveContacts();
    void rebuildSolverConstraints();
    void detectAllContacts();
    uint16_t acquireSolverBodyIndex(RigidBody *body);
    void preSolveContacts();
    void warmStart();
    void solveVelocityConstraints();
    void solvePositionConstraints();
    void detectSweptCollisions();
    void updateMeshColliderWorldStates();
  };

  CollisionScene *collisionSceneGetInstance();

} // namespace P64::Coll
