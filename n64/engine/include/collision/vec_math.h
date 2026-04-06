/**
 * @file vec_math.h
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Additional Vector Math Functions and Operators for Collision Detection
 */
#pragma once

#include <cmath>
#include <t3d/t3dmath.h>
#include "lib/math.h"

using namespace P64::Math;
namespace P64::Coll {

  // ----- Additional vector utilities -----

  inline fm_vec3_t vec3ReciprocalScaleComponents(const fm_vec3_t &scale) {
    return fm_vec3_t{{
      fabsf(scale.x) > FM_EPSILON ? 1.0f / scale.x : 0.0f,
      fabsf(scale.y) > FM_EPSILON ? 1.0f / scale.y : 0.0f,
      fabsf(scale.z) > FM_EPSILON ? 1.0f / scale.z : 0.0f,
    }};
  }

  inline fm_vec3_t vec3NormalizeOrFallback(const fm_vec3_t &vector, const fm_vec3_t &fallback) {
    if(fm_vec3_len2(&vector) > FM_EPSILON * FM_EPSILON) {
      fm_vec3_t normalized;
      fm_vec3_norm(&normalized, &vector);
      return normalized;
    }
    return fallback;
  }

  inline fm_vec3_t vec3Perpendicular(const fm_vec3_t &a) {
    fm_vec3_t temp;
    if(fabsf(a.x) > fabsf(a.z)) {
      fm_vec3_cross(&temp, &a, &VEC3_FORWARD);
      return temp;
    }
    fm_vec3_cross(&temp, &a, &VEC3_RIGHT);
    return temp;
  }
  
  /// @brief Computes The vector triple product: (a × b) × c = b(a·c) - a(b·c)
  /// @param a 
  /// @param b 
  /// @param c 
  /// @return The result of the vector triple product
  inline fm_vec3_t vec3TripleProduct(const fm_vec3_t &a, const fm_vec3_t &b, const fm_vec3_t &c) {
    float ac = fm_vec3_dot(&a, &c);
    float bc = fm_vec3_dot(&b, &c);
    return fm_vec3_t{{
      b.x * ac - a.x * bc,
      b.y * ac - a.y * bc,
      b.z * ac - a.z * bc
    }};
  }

  /// @brief Checks if a vector is the zero vector (0, 0, 0).
  /// @param v 
  /// @return 
  inline bool vec3IsZero(const fm_vec3_t &v) {
    return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f;
  }

  /// @brief Returns a vector containing the component-wise minimum of two vectors.
  /// @param a 
  /// @param b 
  /// @return 
  inline fm_vec3_t vec3Min(const fm_vec3_t &a, const fm_vec3_t &b) {
    return fm_vec3_t{{fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)}};
  }

  /// @brief Returns a vector containing the component-wise maximum of two vectors.
  /// @param a 
  /// @param b 
  /// @return 
  inline fm_vec3_t vec3Max(const fm_vec3_t &a, const fm_vec3_t &b) {
    return fm_vec3_t{{fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)}};
  }


  /// @brief Projects vector v onto vector onto.
  /// @param v The vector to be projected.
  /// @param onto The vector onto which v is projected.
  /// @return The projected vector.
  inline fm_vec3_t vec3Project(const fm_vec3_t &v, const fm_vec3_t &onto)
  {
    float d = fm_vec3_dot(&v, &onto);
    float m = fm_vec3_len2(&onto);
    if (m < FM_EPSILON)
      return VEC3_ZERO;
    return onto * (d / m);
  }


  /// @brief Clamps the magnitude of a vector to a maximum value.
  /// @param v The vector to be clamped.
  /// @param maxMag The maximum allowed magnitude.
  /// @return The clamped vector.
  inline fm_vec3_t vec3ClampMag(const fm_vec3_t &v, float maxMag) {
    float magSq = fm_vec3_len2(&v);
    if(magSq > maxMag * maxMag) {
      return v * (maxMag / sqrtf(magSq));
    }
    return v;
  }


  /// @brief Calculates two tangent vectors orthogonal to a given normal vector.
  /// @param normal The normal vector.
  /// @param tangentU The first tangent vector (output).
  /// @param tangentV The second tangent vector (output).
  inline void vec3CalculateTangents(const fm_vec3_t &normal, fm_vec3_t &tangentU, fm_vec3_t &tangentV) {
    if(fabsf(normal.x) > fabsf(normal.z)) {
      float invLen = 1.0f / sqrtf(normal.x * normal.x + normal.y * normal.y);
      tangentU = fm_vec3_t{{-normal.y * invLen, normal.x * invLen, 0.0f}};
    } else {
      float invLen = 1.0f / sqrtf(normal.y * normal.y + normal.z * normal.z);
      tangentU = fm_vec3_t{{0.0f, -normal.z * invLen, normal.y * invLen}};
    }
    fm_vec3_cross(&tangentV, &normal, &tangentU);
  }

  // ----- Barycentric Coordinates -----

  inline float calculateLerp(const fm_vec3_t &a, const fm_vec3_t &b, const fm_vec3_t &point) {
    auto v0 = b - a;
    float denom = fm_vec3_len2(&v0);
    if(denom < FM_EPSILON * FM_EPSILON) return 0.5f;
    auto offset = point - a;
    return fm_vec3_dot(&offset, &v0) / denom;
  }

  inline fm_vec3_t calculateBarycentricCoords(
    const fm_vec3_t &a, const fm_vec3_t &b, const fm_vec3_t &c, const fm_vec3_t &point
  ) {
    auto v0 = b - a;
    auto v1 = c - a;
    auto v2 = point - a;

    float d00 = fm_vec3_dot(&v0, &v0);
    float d01 = fm_vec3_dot(&v0, &v1);
    float d11 = fm_vec3_dot(&v1, &v1);
    float d20 = fm_vec3_dot(&v2, &v0);
    float d21 = fm_vec3_dot(&v2, &v1);

    float denom = d00 * d11 - d01 * d01;

    if(fabsf(denom) < FM_EPSILON) {
      fm_vec3_t result;
      if(d00 > d11) {
        result.y = calculateLerp(a, b, point);
        result.x = 1.0f - result.y;
        result.z = 0.0f;
      } else {
        result.z = calculateLerp(a, c, point);
        result.x = 1.0f - result.z;
        result.y = 0.0f;
      }
      return result;
    }

    float denomInv = 1.0f / denom;
    fm_vec3_t result;
    result.y = (d11 * d20 - d01 * d21) * denomInv;
    result.z = (d00 * d21 - d01 * d20) * denomInv;
    result.x = 1.0f - result.y - result.z;
    return result;
  }

  inline fm_vec3_t evaluateBarycentricCoords(
    const fm_vec3_t &a, const fm_vec3_t &b, const fm_vec3_t &c, const fm_vec3_t &bary
  ) {
    auto result = a * bary.x;
    result = result + (b * bary.y);
    result = result + (c * bary.z);
    return result;
  }

  // ----- Plane -----

  /// @brief Represents a plane in 3D space defined by a normal vector and a distance from the origin.
  struct Plane {
    fm_vec3_t normal{};
    float d{0.0f};
  };


  /// @brief Creates a plane from a normal vector and a point on the plane.
  /// @param normal The normal vector of the plane.
  /// @param point A point on the plane.
  /// @return The constructed plane.
  inline Plane planeFromNormalAndPoint(const fm_vec3_t &normal, const fm_vec3_t &point) {
    return Plane{normal, -fm_vec3_dot(&normal, &point)};
  }

  /// @brief Returns the signed distance from a point to a plane.
  /// @param plane The plane to test against.
  /// @param point The point to measure from.
  /// @return Positive if the point is on the side the normal points to, negative otherwise.
  inline float planeSignedDistance(const Plane &plane, const fm_vec3_t &point)
  {
    return fm_vec3_dot(&plane.normal, &point) + plane.d;
  }

  /// @brief Projects a point onto a plane.
  /// @param plane The plane to project onto.
  /// @param point The point to project.
  /// @return The closest point on the plane to the given point.
  inline fm_vec3_t planeProjectPoint(const Plane &plane, const fm_vec3_t &point)
  {
    float dist = planeSignedDistance(plane, point);
    return fm_vec3_t{
        point.x - dist * plane.normal.x,
        point.y - dist * plane.normal.y,
        point.z - dist * plane.normal.z,
    };
  }
  

  /// @brief Tests for intersection between a ray and a plane, returning the distance along the ray to the intersection point if it exists.
  /// @param plane The plane to test against.
  /// @param rayOrigin The origin of the ray.
  /// @param rayDir The direction of the ray.
  /// @param outDistance The distance along the ray to the intersection point (output).
  /// @return True if the ray intersects the plane, false otherwise.
  inline bool planeRayIntersection(const Plane &plane, const fm_vec3_t &rayOrigin, const fm_vec3_t &rayDir, float &outDistance) {
    float normalDot = fm_vec3_dot(&plane.normal, &rayDir);
    if(fabsf(normalDot) < FM_EPSILON) return false;
    outDistance = -(fm_vec3_dot(&rayOrigin, &plane.normal) + plane.d) / normalDot;
    return true;
  }


  // ----- Quaternion utilities -----

  /// @brief Constructs the conjugate of a quaternion, which represents the inverse rotation for unit quaternions.
  /// @param q The quaternion to conjugate.
  /// @return The conjugated quaternion.
  inline fm_quat_t quatConjugate(const fm_quat_t &q) {
    return {-q.x, -q.y, -q.z, q.w};
  }

  /// @brief Computes the dot product of two quaternions.
  /// @param a The first quaternion.
  /// @param b The second quaternion.
  /// @return The dot product of the two quaternions.
  inline float quatDot(const fm_quat_t &a, const fm_quat_t &b) {
    return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
  }


  /// @brief Apply an Angular Velocity to an existing rotation quaternion, scaled by a given scalar 
  /// @param q the input quaternion
  /// @param omega the angular velocity
  /// @param dt scalar
  /// @return 
  inline fm_quat_t quatApplyAngularVelocity(const fm_quat_t &q, const fm_vec3_t &omega, float dt) {
    float hdt = 0.5f * dt;
    fm_quat_t dq;
    fm_quat_t result;
    dq.x = hdt * (omega.x * q.w + omega.y * q.z - omega.z * q.y);
    dq.y = hdt * (omega.y * q.w + omega.z * q.x - omega.x * q.z);
    dq.z = hdt * (omega.z * q.w + omega.x * q.y - omega.y * q.x);
    dq.w = hdt * (-omega.x * q.x - omega.y * q.y - omega.z * q.z);
    result = {q.x + dq.x, q.y + dq.y, q.z + dq.z, q.w + dq.w};
    fm_quat_norm(&result, &result);
    return result;
  }

  /// @brief Checks if two quaternions are the same component-wise
  /// @param a
  /// @param b
  /// @return
  inline bool quatIsIdentical(const fm_quat_t* a, const fm_quat_t* b) {
      return (a->x == b->x) && (a->y == b->y) && (a->z == b->z) && (a->w == b->w);
  }

} // namespace P64::Coll
