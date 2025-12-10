/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include <libdragon.h>
#include <t3d/t3dmath.h>
#include "collision/mesh.h"
// #include "../utils/math.h"
// #include "../debug/debugDraw.h"
// #include "../main.h"

using namespace P64;

namespace {

  constexpr T3DVec3 unitAxis0{1, 0, 0};
  constexpr T3DVec3 unitAxis1{0, 1, 0};
  constexpr T3DVec3 unitAxis2{0, 0, 1};

  float clamp(float x, float min, float max) {
    return fminf(fmaxf(x, min), max);
  }

  constexpr float MIN_PENETRATION = 0.0001f;

  bool intersectRaySphere(
    const T3DVec3 &rayStarting, const T3DVec3 &rayNormalizedDirection,
    const T3DVec3 &sphereCenter,
    float sphereRadiusSquared,
    float &intersectionDistance
  ) {
    T3DVec3 diff = sphereCenter - rayStarting;
    float t0 = t3d_vec3_dot(&diff, &rayNormalizedDirection);
    float dSquared = t3d_vec3_dot(&diff, &diff) - t0 * t0;
    if(dSquared > sphereRadiusSquared) {
      return false;
    }

    float t1 = sqrtf( sphereRadiusSquared - dSquared );
    intersectionDistance = t0 > t1 + MIN_PENETRATION ? t0 - t1 : t0 + t1;
    return intersectionDistance > MIN_PENETRATION;
  }

  [[maybe_unused]] bool intersectRaySphere(
      const T3DVec3 &rayStarting,
      const T3DVec3 &rayNormalizedDirection,
      const T3DVec3 &sphereCenter,
      float sphereRadius,
      T3DVec3 &intersectionPosition,
      T3DVec3 &intersectionNormal
    )
  {
    float distance;
    if(intersectRaySphere(rayStarting, rayNormalizedDirection, sphereCenter, sphereRadius * sphereRadius, distance))
    {
      intersectionPosition = rayStarting + rayNormalizedDirection * distance;
      intersectionNormal = (intersectionPosition - sphereCenter) / sphereRadius;
      return true;
    }
    return false;
  }

  float pointPlaneDistance(const T3DVec3 &p, const T3DVec3 planePos, const T3DVec3 planeNorm)
  {
    auto diff = (p - planePos);
    return t3d_vec3_dot(&diff, &planeNorm);
  }

  T3DVec3 getTriBaryCoord(const T3DVec3 &p, const T3DVec3 &a, const T3DVec3 &b, const T3DVec3 &c)
  {
		const auto v0 = c - a;
		const auto v1 = b - a;
		const auto v2 = p - a;

		const auto dot00 = t3d_vec3_dot(&v0, &v0);
		const auto dot01 = t3d_vec3_dot(&v0, &v1);
		const auto dot11 = t3d_vec3_dot(&v1, &v1);

		const float denom = ( dot00 * dot11 - dot01 * dot01 );

		if(denom == 0.0f) {
			return T3DVec3{-1.0f, -1.0f, -1.0f};
		}

    const auto dot02 = t3d_vec3_dot(&v0, &v2);
		const auto dot12 = t3d_vec3_dot(&v1, &v2);

		const float invDenom = 1.0f / denom;
		const float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
		const float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

		return {1.0f - u-v, v, u};
  }

  T3DVec3 closestPointOnLine(const T3DVec3 &p, const T3DVec3 &a, const T3DVec3 &b)
  {
    const T3DVec3 lineVec = b - a;
    const float length = t3d_vec3_len(&lineVec);
    if(length < MIN_PENETRATION)return a;
		const T3DVec3 pointToA = p - a;
		const T3DVec3 lineDir = lineVec / length;

		const float pointDist = t3d_vec3_dot(&pointToA, &lineDir);
		return a + (lineDir * clamp(pointDist, 0.0f, length));
  }

  Coll::CollInfo triVsSphere(const Coll::BCS &sphere, const Coll::Triangle &face)
  {
    const auto &bcsPos = sphere.center;

    const auto &vert0 = *face.v[0];
    const auto &vert1 = *face.v[1];
    const auto &vert2 = *face.v[2];

    // Face tests
    float planeDist = pointPlaneDistance(bcsPos, vert0, face.normal);
    // when we are behind the face (negative), half the distance that is needed to snap back in
    float planeDistAbs = planeDist < 0.0f ? fabsf(planeDist*2.0f) : planeDist;
    if(planeDistAbs < sphere.getRadius())
    {
      auto baryPos = getTriBaryCoord(bcsPos, vert0, vert1, vert2);
      const bool isInTri = (baryPos.v[0] >= 0.0f) && (baryPos.v[1] >= 0.0f)
        && ((baryPos.v[0] + baryPos.v[1]) <= 1.0f);

      if(isInTri)
      {
        return {
          .penetration = face.normal * (planeDist - sphere.getRadius()),
          .floorWallAngle = face.normal,
          .collCount = 1
        };
      }
    }

    // Edge test
    const auto closestPoint1 = closestPointOnLine(bcsPos, vert0, vert1);
    const auto closestPoint2 = closestPointOnLine(bcsPos, vert1, vert2);
    const auto closestPoint3 = closestPointOnLine(bcsPos, vert2, vert0);

    const auto closestDist1 = t3d_vec3_distance2(&bcsPos, &closestPoint1);
    const auto closestDist2 = t3d_vec3_distance2(&bcsPos, &closestPoint2);
    const auto closestDist3 = t3d_vec3_distance2(&bcsPos, &closestPoint3);

    const float closestDist = fminf(closestDist1, fminf(closestDist2, closestDist3));
    if(closestDist <= (sphere.getRadius() * sphere.getRadius()))
    {
      const auto contactPoint = (closestDist == closestDist1)
        ? closestPoint1
        : ((closestDist == closestDist2) ? closestPoint2 : closestPoint3);

      const auto penVector = contactPoint - bcsPos;

      // prevent back-face collision
      const float faceDirAngle = t3d_vec3_dot(&penVector, &face.normal);
      if(faceDirAngle > 0.0f) {
        return {.collCount = 0};
      }

      const auto penLen = t3d_vec3_len(&penVector);
      auto penVectorNorm = penVector / penLen * fmaxf(sphere.getRadius() - penLen, 0.0f);

      return {
        .penetration = penVectorNorm,
        .floorWallAngle = face.normal,
        .collCount = 1
      };
    }

    return {.collCount = 0};
  }

  Coll::CollInfo triVsBox(const Coll::BCS &box, const Coll::Triangle &face)
  {
    // move triangle to origin
    const auto v0 = *face.v[0] - box.center;
    const auto v1 = *face.v[1] - box.center;
    const auto v2 = *face.v[2] - box.center;

    const auto edge0 = v1 - v0;
    const auto edge1 = v2 - v1;
    const auto edge2 = v0 - v2;

    float distance = 999999.0f;
    T3DVec3 lastAxis{};

    const auto testAxis = [&v0, &v1, &v2, &box, &lastAxis, &distance](const T3DVec3 &satAxis)
    {
      if((satAxis.x + satAxis.y + satAxis.z) == 0.0f) {
        return true;
      }

      // project vertices onto separating axis
      T3DVec3 points{
        t3d_vec3_dot(v0, satAxis),
        t3d_vec3_dot(v1, satAxis),
        t3d_vec3_dot(v2, satAxis),
      };
      // project AABB-extend onto sep. axis (AABB origin is always at axis origin)
      const auto combiExtend = box.halfExtend * Math::abs(satAxis);
      const float r = combiExtend.x + combiExtend.y + combiExtend.z;

      float pMin = Math::min(points);
      float pMax = Math::max(points);

      float overlap = r - fmaxf(-pMax, pMin);
      if(overlap > 0.0f) {
        if(overlap < distance) {
          distance = overlap;
          lastAxis = satAxis * Math::sign(points);
        }
        return true;
      }
      return false;
    };

    bool isColl =
      // AABB Face normals
      testAxis(unitAxis0) &&
      testAxis(unitAxis1) &&
      testAxis(unitAxis2) &&

      // Triangle Normal
      testAxis(face.normal) &&

      // 9 Triangle Edge combinations
      testAxis(Math::cross(unitAxis0, edge0)) &&
      testAxis(Math::cross(unitAxis0, edge1)) &&
      testAxis(Math::cross(unitAxis0, edge2)) &&
      testAxis(Math::cross(unitAxis1, edge0)) &&
      testAxis(Math::cross(unitAxis1, edge1)) &&
      testAxis(Math::cross(unitAxis2, edge2)) &&
      testAxis(Math::cross(unitAxis2, edge0)) &&
      testAxis(Math::cross(unitAxis2, edge1)) &&
      testAxis(Math::cross(unitAxis2, edge2))
    ;

    if(isColl) {
      return {
        .penetration = lastAxis * distance,
        .floorWallAngle = face.normal,
        .collCount = 1,
      };
    }

    return {};
  }

  bool pointVsTriangle2D(const fm_vec2_t &p, const Coll::Triangle2D &tri)
  {
    bool b0 = fm_vec2_dot(fm_vec2_t{p.v[0] - tri.v[0].v[0], p.v[1] - tri.v[0].v[1]}, fm_vec2_t{tri.v[0].v[1] - tri.v[1].v[1], tri.v[1].v[0] - tri.v[0].v[0]}) > 0.0f;
    bool b1 = fm_vec2_dot(fm_vec2_t{p.v[0] - tri.v[1].v[0], p.v[1] - tri.v[1].v[1]}, fm_vec2_t{tri.v[1].v[1] - tri.v[2].v[1], tri.v[2].v[0] - tri.v[1].v[0]}) > 0.0f;
    bool b2 = fm_vec2_dot(fm_vec2_t{p.v[0] - tri.v[2].v[0], p.v[1] - tri.v[2].v[1]}, fm_vec2_t{tri.v[2].v[1] - tri.v[0].v[1], tri.v[0].v[0] - tri.v[2].v[0]}) > 0.0f;
    return (b0 == b1 && b1 == b2);
  }

  T3DVec3 getTrianglePosFromXZ(const T3DVec3 &pos, const T3DVec3 &vert, const T3DVec3 &normal)
  {
    const float t = (t3d_vec3_dot(normal, pos) - t3d_vec3_dot(normal, vert)) / normal.v[1];
    return pos + T3DVec3{{0, -t, 0}};
  }
}

Coll::CollInfo Coll::Mesh::vsSphere(const Coll::BCS &sphere, const Coll::Triangle &triangle) const {
  return triVsSphere(sphere, triangle);
}

Coll::CollInfo Coll::Mesh::vsBox(const Coll::BCS &box, const Coll::Triangle &triangle) const {
  return triVsBox(box, triangle);
}

Coll::RaycastRes Coll::Mesh::vsFloorRay(const T3DVec3 &rayStart, const Coll::Triangle &face) const
{
    const auto &vert0 = *face.v[0];
    const auto &vert1 = *face.v[1];
    const auto &vert2 = *face.v[2];

    // raycast the floor, this means we can reduce this to a 2D point vs. triangle test
    // by projecting it down (aka ignoring height)
    auto tri2D = Coll::Triangle2D{{
      {vert0.x, vert0.z},
      {vert1.x, vert1.z},
      {vert2.x, vert2.z}
    }};

    if(!pointVsTriangle2D({rayStart.v[0], rayStart.v[2]}, tri2D)) {
      return {};
    }

    auto hitPos = getTrianglePosFromXZ(rayStart, vert0, face.normal);
    if(hitPos.v[1] > rayStart.v[1]) {
      return {};
    }

    return {
      .hitPos = hitPos,
      .normal = face.normal,
    };
  }
