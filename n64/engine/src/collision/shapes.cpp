/**
 * @file shapes.cpp
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Defines the Properties and helper functions of the different basic Collider shapes (see shapes.h)
 */
#include "collision/shapes.h"

#include <cmath>

using namespace P64::Coll;

namespace {
  constexpr float SQRT_1_2 = 0.707106781f;
}

// ── Box ─────────────────────────────────────────────────────────────

// boundingBox() callers that already hold the rotation matrix use the
// matrix overload directly and skip rebuilding it.

AABB BoxShape::boundingBox(const fm_quat_t *q) const {
  if(!q) {
    return {fm_vec3_t{{-halfSize.x, -halfSize.y, -halfSize.z}},
            fm_vec3_t{{ halfSize.x,  halfSize.y,  halfSize.z}}};
  }
  return boundingBox(quatToMatrix3(*q));
}

AABB BoxShape::boundingBox(const Matrix3x3 &r) const {
  const float ex = halfSize.x*fabsf(r.m[0][0]) + halfSize.y*fabsf(r.m[0][1]) + halfSize.z*fabsf(r.m[0][2]);
  const float ey = halfSize.x*fabsf(r.m[1][0]) + halfSize.y*fabsf(r.m[1][1]) + halfSize.z*fabsf(r.m[1][2]);
  const float ez = halfSize.x*fabsf(r.m[2][0]) + halfSize.y*fabsf(r.m[2][1]) + halfSize.z*fabsf(r.m[2][2]);

  return {fm_vec3_t{{-ex, -ey, -ez}}, fm_vec3_t{{ex, ey, ez}}};
}

fm_vec3_t BoxShape::inertiaTensor(float mass) const {
  float hxSq = halfSize.x * halfSize.x;
  float hySq = halfSize.y * halfSize.y;
  float hzSq = halfSize.z * halfSize.z;
  float scale = mass / 3.0f;

  return fm_vec3_t{{
    scale * (hySq + hzSq),
    scale * (hxSq + hzSq),
    scale * (hxSq + hySq)
  }};
}

// ── Capsule ─────────────────────────────────────────────────────────

AABB CapsuleShape::boundingBox(const fm_quat_t *q) const {
  if(!q) {
    const float absY = fabsf(innerHalfHeight);
    return {
      fm_vec3_t{{-radius, -absY - radius, -radius}},
      fm_vec3_t{{ radius,  absY + radius,  radius}}
    };
  }
  return boundingBox(quatToMatrix3(*q));
}

AABB CapsuleShape::boundingBox(const Matrix3x3 &r) const {
  // Only the rotated local up axis matters, the caps are spherical
  const float absX = fabsf(r.m[0][1] * innerHalfHeight);
  const float absY = fabsf(r.m[1][1] * innerHalfHeight);
  const float absZ = fabsf(r.m[2][1] * innerHalfHeight);

  return {
    fm_vec3_t{{-absX - radius, -absY - radius, -absZ - radius}},
    fm_vec3_t{{ absX + radius,  absY + radius,  absZ + radius}}
  };
}

fm_vec3_t CapsuleShape::inertiaTensor(float mass) const {
  float cylHeight = 2.0f * innerHalfHeight;

  constexpr float PI = 3.14159265358979f;

  float cylVol = PI * radius * radius * cylHeight;
  float sphereVol = (4.0f / 3.0f) * PI * radius * radius * radius;
  float totalVol = cylVol + sphereVol;

  float cylMass = mass * (cylVol / totalVol);
  float sphereMass = mass * (sphereVol / totalVol);

  float rSq = radius * radius;
  float hSq = cylHeight * cylHeight;
  float cylPerp = cylMass * (3.0f * rSq + hSq) / 12.0f;
  float cylAxial = 0.5f * cylMass * rSq;

  float sphereInertia = 0.4f * sphereMass * rSq;
  float offsetSq = innerHalfHeight * innerHalfHeight;
  float hemiMass = sphereMass * 0.5f;
  float spherePerp = sphereInertia + 2.0f * hemiMass * offsetSq;

  return fm_vec3_t{{
    cylPerp + spherePerp,
    cylAxial + sphereInertia,
    cylPerp + spherePerp
  }};
}

// ── Cylinder ────────────────────────────────────────────────────────

fm_vec3_t CylinderShape::support(const fm_vec3_t &dir) const {
  float x = dir.x;
  float y = dir.y;
  float z = dir.z;

  fm_vec3_t output;
  output.y = copysignf(halfHeight, y);

  float absX = fabsf(x);
  float absZ = fabsf(z);

  if(absX < SQRT_1_2 * (absX + absZ) && absZ < SQRT_1_2 * (absX + absZ)) {
    output.x = (x >= 0.0f) ? radius * SQRT_1_2 : -radius * SQRT_1_2;
    output.z = (z >= 0.0f) ? radius * SQRT_1_2 : -radius * SQRT_1_2;
  } else if(absX > absZ) {
    output.x = (x >= 0.0f) ? radius : -radius;
    output.z = 0.0f;
  } else {
    output.x = 0.0f;
    output.z = (z >= 0.0f) ? radius : -radius;
  }

  return output;
}

AABB CylinderShape::boundingBox(const fm_quat_t *q) const {
  if(!q) {
    return {fm_vec3_t{{-radius, -halfHeight, -radius}}, fm_vec3_t{{radius, halfHeight, radius}}};
  }
  return boundingBox(quatToMatrix3(*q));
}

AABB CylinderShape::boundingBox(const Matrix3x3 &r) const {
  const float ex = radius, ey = halfHeight, ez = radius;

  const float wxe = fabsf(r.m[0][0])*ex + fabsf(r.m[0][1])*ey + fabsf(r.m[0][2])*ez;
  const float wye = fabsf(r.m[1][0])*ex + fabsf(r.m[1][1])*ey + fabsf(r.m[1][2])*ez;
  const float wze = fabsf(r.m[2][0])*ex + fabsf(r.m[2][1])*ey + fabsf(r.m[2][2])*ez;

  return {fm_vec3_t{{-wxe, -wye, -wze}}, fm_vec3_t{{wxe, wye, wze}}};
}

fm_vec3_t CylinderShape::inertiaTensor(float mass) const {
  float h = 2.0f * halfHeight;
  float rSq = radius * radius;
  float hSq = h * h;

  float perpInertia = mass * (3.0f * rSq + hSq) / 12.0f;
  float axialInertia = 0.5f * mass * rSq;

  return fm_vec3_t{{perpInertia, axialInertia, perpInertia}};
}

// ── Cone ────────────────────────────────────────────────────────────

fm_vec3_t ConeShape::support(const fm_vec3_t &dir) const {
  float dx = dir.x;
  float dy = dir.y;
  float dz = dir.z;

  float sinAlpha = radius / sqrtf(radius * radius + 4.0f * halfHeight * halfHeight);
  float sin2 = sinAlpha * sinAlpha;
  float sigma2 = dx * dx + dz * dz;
  float dy2 = dy * dy;
  float d2 = dy2 + sigma2;

  if(dy > 0.0f && dy2 > d2 * sin2) {
    return fm_vec3_t{{0.0f, halfHeight, 0.0f}};
  }

  if(sigma2 > 0.0f) {
    float invSigma = 1.0f / sqrtf(sigma2);
    return fm_vec3_t{{radius * dx * invSigma, -halfHeight, radius * dz * invSigma}};
  }

  return fm_vec3_t{{0.0f, -halfHeight, 0.0f}};
}

AABB ConeShape::boundingBox(const fm_quat_t *q) const {
  if(!q) {
    return {fm_vec3_t{{-radius, -halfHeight, -radius}}, fm_vec3_t{{radius, halfHeight, radius}}};
  }
  return boundingBox(quatToMatrix3(*q));
}

AABB ConeShape::boundingBox(const Matrix3x3 &r) const {
  auto rotate = [&](float px, float py, float pz) -> fm_vec3_t {
    return fm_vec3_t{{
      r.m[0][0]*px + r.m[0][1]*py + r.m[0][2]*pz,
      r.m[1][0]*px + r.m[1][1]*py + r.m[1][2]*pz,
      r.m[2][0]*px + r.m[2][1]*py + r.m[2][2]*pz
    }};
  };

  auto apex = rotate(0.0f, halfHeight, 0.0f);
  fm_vec3_t bmin = apex, bmax = apex;

  fm_vec3_t basePts[4] = {
    rotate( radius, -halfHeight,  0.0f),
    rotate(-radius, -halfHeight,  0.0f),
    rotate( 0.0f,   -halfHeight,  radius),
    rotate( 0.0f,   -halfHeight, -radius)
  };

  for(int i = 0; i < 4; ++i) {
    bmin = vec3Min(bmin, basePts[i]);
    bmax = vec3Max(bmax, basePts[i]);
  }

  return {bmin, bmax};
}

fm_vec3_t ConeShape::inertiaTensor(float mass) const {
  float h = 2.0f * halfHeight;
  float rSq = radius * radius;
  float hSq = h * h;

  float perpInertia = (3.0f / 80.0f) * mass * (4.0f * rSq + hSq);
  float axialInertia = 0.3f * mass * rSq;

  return fm_vec3_t{{perpInertia, axialInertia, perpInertia}};
}

// ── Pyramid ─────────────────────────────────────────────────────────

fm_vec3_t PyramidShape::support(const fm_vec3_t &dir) const {
  float apexDot = halfHeight * dir.y;
  float baseDot = fabsf(dir.x) * baseHalfWidthX + fabsf(dir.z) * baseHalfWidthZ - halfHeight * dir.y;

  if(apexDot > baseDot) {
    return fm_vec3_t{{0.0f, halfHeight, 0.0f}};
  }

  return fm_vec3_t{{
    copysignf(baseHalfWidthX, dir.x),
    -halfHeight,
    copysignf(baseHalfWidthZ, dir.z)
  }};
}

AABB PyramidShape::boundingBox(const fm_quat_t *q) const {
  if(!q) {
    return {fm_vec3_t{{-baseHalfWidthX, -halfHeight, -baseHalfWidthZ}},
            fm_vec3_t{{ baseHalfWidthX,  halfHeight,  baseHalfWidthZ}}};
  }
  return boundingBox(quatToMatrix3(*q));
}

AABB PyramidShape::boundingBox(const Matrix3x3 &r) const {
  const float ex = baseHalfWidthX*fabsf(r.m[0][0]) + halfHeight*fabsf(r.m[0][1]) + baseHalfWidthZ*fabsf(r.m[0][2]);
  const float ey = baseHalfWidthX*fabsf(r.m[1][0]) + halfHeight*fabsf(r.m[1][1]) + baseHalfWidthZ*fabsf(r.m[1][2]);
  const float ez = baseHalfWidthX*fabsf(r.m[2][0]) + halfHeight*fabsf(r.m[2][1]) + baseHalfWidthZ*fabsf(r.m[2][2]);

  return {fm_vec3_t{{-ex, -ey, -ez}}, fm_vec3_t{{ex, ey, ez}}};
}

fm_vec3_t PyramidShape::inertiaTensor(float mass) const {
  float mDiv20 = mass * 0.05f;
  float mDiv5 = mass * 0.2f;

  float Ixx = (mDiv5 * baseHalfWidthZ * baseHalfWidthZ) + (mDiv20 * 3.0f * halfHeight * halfHeight);
  float Iyy = (mDiv5 * baseHalfWidthX * baseHalfWidthX) + (mDiv5 * baseHalfWidthZ * baseHalfWidthZ);
  float Izz = (mDiv5 * baseHalfWidthX * baseHalfWidthX) + (mDiv20 * 3.0f * halfHeight * halfHeight);

  return fm_vec3_t{{Ixx, Iyy, Izz}};
}
