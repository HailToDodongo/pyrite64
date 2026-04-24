/**
 * @file rigid_body.cpp
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Contains the rigidBody definition, constants and related functions (see rigid_body.h)
 */
#include "collision/rigid_body.h"
#include "collision/collision_scene.h"
#include <cassert>
#include <cmath>

namespace P64::Coll {

  static bool matrix3Equals(const Matrix3x3 &lhs, const Matrix3x3 &rhs) {
    for(int row = 0; row < 3; ++row) {
      for(int col = 0; col < 3; ++col) {
        if(fabsf(lhs.m[row][col] - rhs.m[row][col]) > FM_EPSILON) {
          return false;
        }
      }
    }

    return true;
  }

  static fm_vec3_t scaleVec3Components(const fm_vec3_t &lhs, const fm_vec3_t &rhs) {
    return fm_vec3_t{{
      lhs.x * rhs.x,
      lhs.y * rhs.y,
      lhs.z * rhs.z
    }};
  }

  static fm_vec3_t diagonalInverse(const fm_vec3_t &v) {
    return fm_vec3_t{{
      v.x > FM_EPSILON ? 1.0f / v.x : 0.0f,
      v.y > FM_EPSILON ? 1.0f / v.y : 0.0f,
      v.z > FM_EPSILON ? 1.0f / v.z : 0.0f
    }};
  }

  static fm_vec3_t linearConstraintScale(Constraint constraints) {
    return fm_vec3_t{{
      hasFlag(constraints, Constraint::FreezePosX) ? 0.0f : 1.0f,
      hasFlag(constraints, Constraint::FreezePosY) ? 0.0f : 1.0f,
      hasFlag(constraints, Constraint::FreezePosZ) ? 0.0f : 1.0f
    }};
  }

  static fm_vec3_t angularConstraintScale(Constraint constraints) {
    return fm_vec3_t{{
      hasFlag(constraints, Constraint::FreezeRotX) ? 0.0f : 1.0f,
      hasFlag(constraints, Constraint::FreezeRotY) ? 0.0f : 1.0f,
      hasFlag(constraints, Constraint::FreezeRotZ) ? 0.0f : 1.0f
    }};
  }

  // ── Matrix utilities ──────────────────────────────────────────────

  fm_vec3_t matrix3Vec3Mul(const Matrix3x3 &mat, const fm_vec3_t &v) {
    return fm_vec3_t{{
      mat.m[0][0] * v.x + mat.m[0][1] * v.y + mat.m[0][2] * v.z,
      mat.m[1][0] * v.x + mat.m[1][1] * v.y + mat.m[1][2] * v.z,
      mat.m[2][0] * v.x + mat.m[2][1] * v.y + mat.m[2][2] * v.z
    }};
  }

  float matrix3Determinant(const Matrix3x3 &matrix) {
    return matrix.m[0][0] * (matrix.m[1][1] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][1])
         - matrix.m[0][1] * (matrix.m[1][0] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][0])
         + matrix.m[0][2] * (matrix.m[1][0] * matrix.m[2][1] - matrix.m[1][1] * matrix.m[2][0]);
  }

  Matrix3x3 matrix3Inverse(const Matrix3x3 &matrix) {
    const float determinant = matrix3Determinant(matrix);
    if(fabsf(determinant) <= FM_EPSILON) {
      return Matrix3x3::identity();
    }

    const float invDet = 1.0f / determinant;
    Matrix3x3 inverse{};
    inverse.m[0][0] =  (matrix.m[1][1] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][1]) * invDet;
    inverse.m[0][1] = -(matrix.m[0][1] * matrix.m[2][2] - matrix.m[0][2] * matrix.m[2][1]) * invDet;
    inverse.m[0][2] =  (matrix.m[0][1] * matrix.m[1][2] - matrix.m[0][2] * matrix.m[1][1]) * invDet;
    inverse.m[1][0] = -(matrix.m[1][0] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][0]) * invDet;
    inverse.m[1][1] =  (matrix.m[0][0] * matrix.m[2][2] - matrix.m[0][2] * matrix.m[2][0]) * invDet;
    inverse.m[1][2] = -(matrix.m[0][0] * matrix.m[1][2] - matrix.m[0][2] * matrix.m[1][0]) * invDet;
    inverse.m[2][0] =  (matrix.m[1][0] * matrix.m[2][1] - matrix.m[1][1] * matrix.m[2][0]) * invDet;
    inverse.m[2][1] = -(matrix.m[0][0] * matrix.m[2][1] - matrix.m[0][1] * matrix.m[2][0]) * invDet;
    inverse.m[2][2] =  (matrix.m[0][0] * matrix.m[1][1] - matrix.m[0][1] * matrix.m[1][0]) * invDet;
    return inverse;
  }

  Matrix3x3 matrix3Mul(const Matrix3x3 &a, const Matrix3x3 &b) {
    Matrix3x3 r{};
    for(int i = 0; i < 3; ++i) {
      for(int j = 0; j < 3; ++j) {
        r.m[i][j] = a.m[i][0] * b.m[0][j]
                   + a.m[i][1] * b.m[1][j]
                   + a.m[i][2] * b.m[2][j];
      }
    }
    return r;
  }

  Matrix3x3 matrix3Transpose(const Matrix3x3 &m) {
    Matrix3x3 r{};
    for(int i = 0; i < 3; ++i) {
      for(int j = 0; j < 3; ++j) {
        r.m[i][j] = m.m[j][i];
      }
    }
    return r;
  }

  Matrix3x3 quatToMatrix3(const fm_quat_t &q) {
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

    Matrix3x3 r{};
    r.m[0][0] = 1.0f - 2.0f * (yy + zz);
    r.m[0][1] = 2.0f * (xy - wz);
    r.m[0][2] = 2.0f * (xz + wy);

    r.m[1][0] = 2.0f * (xy + wz);
    r.m[1][1] = 1.0f - 2.0f * (xx + zz);
    r.m[1][2] = 2.0f * (yz - wx);

    r.m[2][0] = 2.0f * (xz - wy);
    r.m[2][1] = 2.0f * (yz + wx);
    r.m[2][2] = 1.0f - 2.0f * (xx + yy);
    return r;
  }

  // ── RigidBody ─────────────────────────────────────────────────

  void RigidBody::init(P64::Object *object, float m) {
    assertf(m > 0.0f, "Mass must be greater than zero");
    assertf(object, "RigidBody must be initialized with a valid owner object");

    owner_ = object;
    position_ = &object->pos;
    rotation_ = &object->rot;
    aabbTreeNodeId_ = NULL_NODE;
    constraints_ = Constraint::None;
    sleepCounter_ = 0;
    isSleeping_ = false;
    isKinematic_ = false;
    hasGravity_ = true;

    linearVelocity_ = VEC3_ZERO;
    angularVelocity_ = VEC3_ZERO;
    acceleration_ = VEC3_ZERO;
    torqueAccumulator_ = VEC3_ZERO;
    centerOffset_ = VEC3_ZERO;
    compoundScale_ = object->scale;
    compoundPropertiesDirty_ = true;

    timeScale_ = 1.0f;
    gravityScale_ = 1.0f;
    angularDamping_ = 0.03f;

    setMass(m);

    if(rotation_) {
      previousStepRotation_ = *rotation_;
    }
    if(position_) {
      previousStepPosition_ = *position_;
    }
    previousStepScale_ = object->scale;
  }

  void RigidBody::setMass(float newMass) {
    assert(newMass > 0.0f);
    mass_ = newMass;
    inverseMass_ = 1.0f / newMass;
    compoundPropertiesDirty_ = true;

    // Initialize inertia tensor for a solid sphere as a simple default.
    // This can be overridden later by the colliders associated with the owner object.
    float inertia = 0.4f * mass_;
    defaultLocalInertiaTensor_ = fm_vec3_t{{inertia, inertia, inertia}};
    localInertiaTensor_ = defaultLocalInertiaTensor_;
    invLocalInertiaTensor_ = diagonalInverse(localInertiaTensor_);
    refreshConstraintCaches();
    updateWorldInertia();
  }

  void RigidBody::setConstraints(Constraint newConstraints) {
    constraints_ = newConstraints;
    refreshConstraintCaches();
    linearVelocity_ = constrainLinearWorld(linearVelocity_);
    angularVelocity_ = constrainAngularWorld(angularVelocity_);
  }

  void RigidBody::applyCompoundProperties(const fm_vec3_t &centerOffset, const fm_vec3_t &localInertiaTensor, const fm_vec3_t &compoundScale) {
    centerOffset_ = centerOffset;
    localInertiaTensor_ = localInertiaTensor;
    invLocalInertiaTensor_ = diagonalInverse(localInertiaTensor_);
    compoundScale_ = compoundScale;
    compoundPropertiesDirty_ = false;
    updateWorldInertia();
  }

  fm_vec3_t RigidBody::constrainLinearWorld(const fm_vec3_t &worldLinear) const {
    if(!hasLinearConstraints_) return worldLinear;
    return scaleVec3Components(worldLinear, linearConstraintScale_);
  }

  fm_vec3_t RigidBody::constrainAngularWorld(const fm_vec3_t &worldAngular) const {
    if(!hasAngularConstraints_) return worldAngular;
    return matrix3Vec3Mul(angularConstraintProjection_, worldAngular);
  }

  void RigidBody::applyConstrainedLinearVelocityDelta(const fm_vec3_t &deltaLinearVelocity) {
    linearVelocity_ = linearVelocity_ + constrainLinearWorld(deltaLinearVelocity);
  }

  void RigidBody::applyConstrainedImpulseAtContact(const fm_vec3_t &impulse, const fm_vec3_t &toContact) {
    if(isKinematic_) return;

    applyConstrainedLinearVelocityDelta(impulse * inverseMass_);
    if(!canApplyAngularResponse()) return;

    fm_vec3_t cross;
    fm_vec3_cross(&cross, &toContact, &impulse);
    angularVelocity_ = angularVelocity_ + applyConstrainedWorldInertia(cross);
  }

  float RigidBody::constrainedLinearInvMassAlong(const fm_vec3_t &direction) const {
    if(isKinematic_) return 0.0f;
    if(inverseMass_ <= FM_EPSILON) return 0.0f;
    if(!hasLinearConstraints_) return inverseMass_;

    return direction.x * direction.x * linearInvMassScale_.x +
           direction.y * direction.y * linearInvMassScale_.y +
           direction.z * direction.z * linearInvMassScale_.z;
  }

  void RigidBody::refreshConstraintCaches() {
    hasLinearConstraints_ = (constraints_ & Constraint::FreezePosAll) != Constraint::None;
    hasAngularConstraints_ = (constraints_ & Constraint::FreezeRotAll) != Constraint::None;
    linearConstraintScale_ = linearConstraintScale(constraints_);
    linearInvMassScale_ = linearConstraintScale_ * inverseMass_;
    refreshAngularConstraintProjection();
    refreshConstrainedInertiaTensor();
  }

  void RigidBody::refreshAngularConstraintProjection() {
    if(!hasAngularConstraints_) {
      angularConstraintProjection_ = Matrix3x3::identity();
      return;
    }

    const Matrix3x3 localProjection = diagonalMatrix(angularConstraintScale(constraints_));
    angularConstraintProjection_ = matrix3Mul(matrix3Mul(rotationMatrix_, localProjection), matrix3Transpose(rotationMatrix_));
  }

  void RigidBody::refreshConstrainedInertiaTensor() {
    if(!hasAngularConstraints_) {
      constrainedInvWorldInertiaTensor_ = invWorldInertiaTensor_;
      return;
    }

    constrainedInvWorldInertiaTensor_ = matrix3Mul(angularConstraintProjection_, invWorldInertiaTensor_);
  }

  void RigidBody::integrateVelocity(float fixedDt, const fm_vec3_t &gravity) {
    if(isKinematic_) return;

    // Sleeping bodies must have their velocities explicitly zeroed each frame.
    // Although sleep() zeros them once, the warm start and velocity solver apply
    // impulses to sleeping bodies that participate in constraints.  Without this
    // reset the stale solver-output velocity persists into the next frame, causing
    // the solver to compute incorrect relative velocities for awake neighbours
    // (e.g. under-correcting gravity), which leaves residual velocity on awake
    // bodies and prevents them from ever reaching the sleep threshold.
    // Bullet avoids this by excluding sleeping bodies from the solver entirely;
    // zeroing here achieves the same effect for the velocity read-back.
    if(isSleeping_) {
      linearVelocity_ = VEC3_ZERO;
      acceleration_ = VEC3_ZERO;
      return;
    }

    if(hasFlag(constraints_, Constraint::FreezePosAll)) {
      linearVelocity_ = VEC3_ZERO;
      acceleration_ = VEC3_ZERO;
      return;
    }

    // Apply gravity
    if(hasGravity_) {
      acceleration_ = acceleration_ + (gravity * gravityScale_);
    }

    float dt = fixedDt * timeScale_;

    // Add user acceleration
    linearVelocity_ = linearVelocity_ + (acceleration_ * dt);
    acceleration_ = VEC3_ZERO;

    // Apply position constraints
    linearVelocity_ = constrainLinearWorld(linearVelocity_);

    CollisionScene *scene = collisionSceneGetInstance();
    float physicsScale = scene->getPhysicsScale();

    // Clamp to terminal speed
    float speedSq = fm_vec3_len2(&linearVelocity_);
    if(speedSq > TERMINAL_SPEED * TERMINAL_SPEED * (physicsScale * physicsScale)) {
      linearVelocity_ = linearVelocity_ * ((TERMINAL_SPEED * physicsScale) / sqrtf(speedSq));
    }
  }

  void RigidBody::integrateAngularVelocity(float fixedDt) {
    if(isKinematic_) return;

    // See integrateVelocity for rationale — sleeping bodies must start each step
    // with zero angular velocity so the solver treats them as effectively static.
    if(isSleeping_) {
      angularVelocity_ = VEC3_ZERO;
      torqueAccumulator_ = VEC3_ZERO;
      return;
    }
    if(inverseMass_ < FM_EPSILON) return;

    if(hasFlag(constraints_, Constraint::FreezeRotAll)) {
      angularVelocity_ = VEC3_ZERO;
      torqueAccumulator_ = VEC3_ZERO;
      return;
    }

    float dt = fixedDt * timeScale_;
    const bool hadExternalTorque = !vec3IsZero(torqueAccumulator_);

    // Apply torque accumulator
    if(hadExternalTorque) {
      fm_vec3_t angAccel = applyWorldInertia(torqueAccumulator_);
      angularVelocity_ = angularVelocity_ + (angAccel * dt);
      torqueAccumulator_ = VEC3_ZERO;
    }

    // Clamp angular speed
    float angSpeedSq = fm_vec3_len2(&angularVelocity_);
    if(angSpeedSq > TERMINAL_ANGULAR_SPEED_SQ) {
      angularVelocity_ = angularVelocity_ * (TERMINAL_ANGULAR_SPEED / sqrtf(angSpeedSq));
      angSpeedSq = TERMINAL_ANGULAR_SPEED_SQ;
    }

    // Apply rotation constraints in local space
    if(hasAngularConstraints_) {
      angularVelocity_ = constrainAngularWorld(angularVelocity_);
    }

    // Angular damping — amplify near rest
    float dampFactor = angularDamping_;
    if(!hadExternalTorque && angSpeedSq < AMPLIFY_ANG_DAMPING_THRESHOLD_SQ && angSpeedSq > FM_EPSILON) {
      float ratio = angSpeedSq * AMPLIFY_ANG_DAMPING_THRESHOLD_SQ_INV;
      dampFactor = angularDamping_ + (1.0f - angularDamping_) * (1.0f - ratio);
    }
    angularVelocity_ = angularVelocity_ * (1.0f - dampFactor);
  }

  void RigidBody::integratePosition(float fixedDt) {
    if(isKinematic_ || isSleeping_) return;
    if(!position_) return;

    float dt = fixedDt * timeScale_;
    previousStepPosition_ = *position_;
    *position_ = *position_ + (linearVelocity_ * dt);
  }

  void RigidBody::integrateRotation(float fixedDt) {
    if(isKinematic_ || isSleeping_) return;
    if(!rotation_) return;
    if(vec3IsZero(angularVelocity_)) return;

    float dt = fixedDt * timeScale_;

    previousStepRotation_ = *rotation_;

    // Store old world center of mass for offset correction
    fm_vec3_t oldWorldCOM = *position_ + (*rotation_ * centerOffset_);

    *rotation_ = quatApplyAngularVelocity(*rotation_, angularVelocity_, dt);

    // Correct position so that the center of mass stays in place
    if(!vec3IsZero(centerOffset_)) {
      fm_vec3_t newWorldCOM = *position_ + (*rotation_ * centerOffset_);
      fm_vec3_t correction = oldWorldCOM - newWorldCOM;
      *position_ = *position_ + correction;
    }
  }

  void RigidBody::accelerate(const fm_vec3_t &accel) {
    acceleration_ = acceleration_ + accel;
    if(isSleeping_) wake();
  }

  void RigidBody::setVelocity(const fm_vec3_t &vel) {
    linearVelocity_ = constrainLinearWorld(vel);
    if(isSleeping_) wake();
  }

  void RigidBody::applyLinearImpulse(const fm_vec3_t &impulse) {
    if(isKinematic_) return;
    fm_vec3_t deltaV = constrainLinearWorld(impulse * inverseMass_);
    linearVelocity_ = linearVelocity_ + deltaV;
    if(isSleeping_) wake();
  }

  void RigidBody::applyTorque(const fm_vec3_t &torque) {
    if(isKinematic_) return;
    torqueAccumulator_ = torqueAccumulator_ + torque;
    if(isSleeping_) wake();
  }

  void RigidBody::applyAngularImpulse(const fm_vec3_t &angImpulse) {
    if(isKinematic_) return;
    angularVelocity_ = angularVelocity_ + applyConstrainedWorldInertia(angImpulse);
    if(isSleeping_) wake();
  }

  void RigidBody::setAngularVelocity(const fm_vec3_t &angVel) {
    angularVelocity_ = constrainAngularWorld(angVel);
    if(isSleeping_) wake();
  }

  fm_vec3_t RigidBody::getVelocityAtPoint(const fm_vec3_t &worldPoint) const {
    fm_vec3_t pointVelocity = linearVelocity_;

    if(vec3IsZero(angularVelocity_)) {
      return pointVelocity;
    }

    fm_vec3_t offset = worldPoint - worldCenterOfMass_;
    fm_vec3_t angularPointVelocity;
    fm_vec3_cross(&angularPointVelocity, &angularVelocity_, &offset);
    return pointVelocity + angularPointVelocity;
  }

  void RigidBody::applyForceAtPoint(const fm_vec3_t &force, const fm_vec3_t &worldPoint) {
    if(isKinematic_) return;
    accelerate(force * inverseMass_);
    fm_vec3_t r = worldPoint - worldCenterOfMass_;
    fm_vec3_t torque;
    fm_vec3_cross(&torque, &r, &force);
    applyTorque(torque);
  }

  void RigidBody::updateWorldInertia() {
    Matrix3x3 newRotationMatrix = rotation_ ? quatToMatrix3(*rotation_) : Matrix3x3::identity();
    Matrix3x3 newInverseRotationMatrix = rotation_ ? quatToMatrix3(quatConjugate(*rotation_)) : Matrix3x3::identity();

    const Matrix3x3 localInv = diagonalMatrix(invLocalInertiaTensor_);
    Matrix3x3 newInvWorldInertiaTensor = matrix3Mul(matrix3Mul(newRotationMatrix, localInv), matrix3Transpose(newRotationMatrix));

    const fm_vec3_t worldOffset = matrix3Vec3Mul(newRotationMatrix, centerOffset_);
    const fm_vec3_t newWorldCenterOfMass = position_ ? *position_ + worldOffset : worldOffset;
    const bool transformChanged = !matrix3Equals(rotationMatrix_, newRotationMatrix) ||
                    !matrix3Equals(inverseRotationMatrix_, newInverseRotationMatrix) ||
                    !matrix3Equals(invWorldInertiaTensor_, newInvWorldInertiaTensor) ||
                                  fm_vec3_distance2(&worldCenterOfMass_, &newWorldCenterOfMass) > FM_EPSILON * FM_EPSILON;

    rotationMatrix_ = newRotationMatrix;
    inverseRotationMatrix_ = newInverseRotationMatrix;
    invWorldInertiaTensor_ = newInvWorldInertiaTensor;

    if(rotation_) {
      rotationMatrix_ = newRotationMatrix;
    }
    refreshAngularConstraintProjection();
    refreshConstrainedInertiaTensor();
    worldCenterOfMass_ = newWorldCenterOfMass;
    if(transformChanged) {
      ++transformVersion_;
    }
  }

  fm_vec3_t RigidBody::toWorldSpace(const fm_vec3_t &localPoint) const {
    if(!position_) return localPoint;
    return *position_ + rotateToWorld(localPoint);
  }

  fm_vec3_t RigidBody::toLocalSpace(const fm_vec3_t &worldPoint) const {
    if(!position_) return worldPoint;
    return rotateToLocal(worldPoint - *position_);
  }

  fm_vec3_t RigidBody::rotateToWorld(const fm_vec3_t &localDir) const {
    if(!rotation_) return localDir;
    return *rotation_ * localDir;
  }

  fm_vec3_t RigidBody::rotateToLocal(const fm_vec3_t &worldDir) const {
    if(!rotation_) return worldDir;
    return quatConjugate(*rotation_) * worldDir;
  }

  void RigidBody::applyPositionConstraints() {
    if(!position_) return;
    if(hasFlag(constraints_, Constraint::FreezePosX)) position_->x = previousStepPosition_.x;
    if(hasFlag(constraints_, Constraint::FreezePosY)) position_->y = previousStepPosition_.y;
    if(hasFlag(constraints_, Constraint::FreezePosZ)) position_->z = previousStepPosition_.z;
  }

} // namespace P64::Coll
