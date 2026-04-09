/**
 * @file collide.cpp
 * @author Kevin Reier <github.com/Byterset>
 * @brief Functions to detect collisions between different shapes and objects and record them (see collide.h)
 */
#include "collision/collide.h"
#include "collision/collision_scene.h"
#include "collision/contact_utils.h"
#include "collision/gjk.h"
#include "scene/scene.h"

#include <cmath>
#include <cstdlib>

namespace P64::Coll {

  struct ColliderProxy {
    Collider *collider{nullptr};
    fm_vec3_t worldCenter{};
    Matrix3x3 shapeToSpace{Matrix3x3::identity()};
    Matrix3x3 shapeToSpaceTranspose{Matrix3x3::identity()};
    Matrix3x3 spaceToShape{Matrix3x3::identity()};
    Matrix3x3 normalToSpace{Matrix3x3::identity()};
    float effectiveRadius{std::numeric_limits<float>::max()};
  };

  static fm_vec3_t makeSafeContactNormal(const fm_vec3_t &normal, const fm_vec3_t &contactA, const fm_vec3_t &contactB) {
    if(fm_vec3_len2(&normal) > FM_EPSILON * FM_EPSILON) {
      fm_vec3_t normalized;
      fm_vec3_norm(&normalized, &normal);
      return normalized;
    }

    fm_vec3_t fallback = contactA - contactB;
    if(fm_vec3_len2(&fallback) > FM_EPSILON * FM_EPSILON) {
      fm_vec3_t normalized;
      fm_vec3_norm(&normalized, &fallback);
      return normalized;
    }

    return VEC3_UP;
  }

  static void swapConstraintOrder(
    RigidBody *&rigidBodyA, Collider *&colliderA, MeshCollider *&meshColliderA, Object *&objectA,
    RigidBody *&rigidBodyB, Collider *&colliderB, MeshCollider *&meshColliderB, Object *&objectB,
    EpaResult &result) {
    std::swap(rigidBodyA, rigidBodyB);
    std::swap(colliderA, colliderB);
    std::swap(meshColliderA, meshColliderB);
    std::swap(objectA, objectB);
    result.normal = -result.normal;
    std::swap(result.contactA, result.contactB);
  }

  static void colliderProxyGjkSupport(const void *data, const fm_vec3_t &direction, fm_vec3_t &output) {
    const auto *proxy = static_cast<const ColliderProxy *>(data);
    if(!proxy || !proxy->collider) {
      output = VEC3_ZERO;
      return;
    }

    fm_vec3_t localDir = matrix3Vec3Mul(proxy->shapeToSpaceTranspose, direction);
    fm_vec3_t localSupport = proxy->collider->support(localDir);
    output = matrix3Vec3Mul(proxy->shapeToSpace, localSupport) + proxy->worldCenter;
  }


  /// @brief Find the closest point on a triangle to a given point, using barycentric coordinates and clamping to the triangle edges.
  /// @param point      Query point.
  /// @param a          Triangle vertex 0.
  /// @param b          Triangle vertex 1.
  /// @param c          Triangle vertex 2.
  /// @param triNormal  Pre-computed triangle normal
  /// @return Closest point on the triangle to `point`.
  static fm_vec3_t closestPointOnTriangle(const fm_vec3_t &point,
                                            const fm_vec3_t &a, const fm_vec3_t &b, const fm_vec3_t &c)
  {
    const fm_vec3_t ap = point - a;
    const fm_vec3_t ab = b - a;
    const fm_vec3_t ac = c - a;

    // Barycentric coordinates — triNormal component of ap is orthogonal to
    // ab and ac, so the plane projection is implicit (no need to subtract it).
    const float d00 = fm_vec3_dot(&ab, &ab);
    const float d01 = fm_vec3_dot(&ab, &ac);
    const float d11 = fm_vec3_dot(&ac, &ac);
    const float d20 = fm_vec3_dot(&ap, &ab);
    const float d21 = fm_vec3_dot(&ap, &ac);

    const float invDenom = 1.0f / (d00 * d11 - d01 * d01);
    float v = (d11 * d20 - d01 * d21) * invDenom;
    float w = (d00 * d21 - d01 * d20) * invDenom;
    float u = 1.0f - v - w;

    // Clamp to triangle if outside
    if (u < 0.0f) {
        w = clamp(w / (v + w), 0.0f, 1.0f);
        v = 1.0f - w;
        u = 0.0f;
    } else if (v < 0.0f) {
        w = clamp(w / (u + w), 0.0f, 1.0f);
        u = 1.0f - w;
        v = 0.0f;
    } else if (w < 0.0f) {
        v = clamp(v / (u + v), 0.0f, 1.0f);
        u = 1.0f - v;
        w = 0.0f;
    }

    return a + ab * v + ac * w;
}

  static void projectTriangleOntoAxis(
    const fm_vec3_t &v0,
    const fm_vec3_t &v1,
    const fm_vec3_t &v2,
    const fm_vec3_t &axis,
    float &outMin,
    float &outMax) {
    outMin = fm_vec3_dot(&v0, &axis);
    outMax = outMin;

    const float p1 = fm_vec3_dot(&v1, &axis);
    outMin = fminf(outMin, p1);
    outMax = fmaxf(outMax, p1);

    const float p2 = fm_vec3_dot(&v2, &axis);
    outMin = fminf(outMin, p2);
    outMax = fmaxf(outMax, p2);
  }

  // ── Analytical collision helpers ──────────────────────────────────

  // Fast closed-form solutions for simple shape pairs. Used to avoid expensive GJK+EPA


  /// @brief Analytical sphere-sphere collision using distance check.
  /// @param a The first sphere collider.
  /// @param b The second sphere collider.
  /// @param result The result of the collision detection.
  /// @return True if a collision is detected, false otherwise.
  static bool analyticalSphereSphere(const Collider *a, const Collider *b, EpaResult &result) {
    if(!a || !b) return false;
    if(a->shapeType() != ShapeType::Sphere || b->shapeType() != ShapeType::Sphere) return false;

    float rA = a->sphereShape().radius;
    float rB = b->sphereShape().radius;
    float combinedRadius = rA + rB;

    fm_vec3_t diff = a->worldCenter() - b->worldCenter();
    float distSq = fm_vec3_len2(&diff);

    if(distSq >= combinedRadius * combinedRadius) return false;

    float dist = sqrtf(distSq);
    if(dist < FM_EPSILON) {
      result.normal = VEC3_UP;
      result.penetration = combinedRadius;
      result.contactA = a->worldCenter() - result.normal * rA;
      result.contactB = b->worldCenter() + result.normal * rB;
      return true;
    }

    result.normal = diff * (1.0f / dist);
    result.penetration = combinedRadius - dist;
    result.contactA = a->worldCenter() - result.normal * rA;
    result.contactB = b->worldCenter() + result.normal * rB;
    return true;
  }


  /// @brief Analytical sphere-box collision using closest-point-on-box + radius check.
  /// @param sphere The sphere collider.
  /// @param box The box collider.
  /// @param result The result of the collision detection.
  /// @return True if a collision is detected, false otherwise.
  static bool analyticalSphereBox(const Collider *sphere, const Collider *box, EpaResult &result) {
    if(!sphere || !box) return false;
    if(sphere->shapeType() != ShapeType::Sphere || box->shapeType() != ShapeType::Box) return false;

    float radius = sphere->sphereShape().radius;
    fm_vec3_t halfSize = box->boxShape().halfSize;

    // Transform sphere center to box local space
    const Matrix3x3 &boxRot = box->rotationMatrix();
    const Matrix3x3 &boxRotT = box->inverseRotationMatrix();
    fm_vec3_t localCenter = sphere->worldCenter() - box->worldCenter();
    fm_vec3_t localSpherePos = matrix3Vec3Mul(boxRotT, localCenter);

    // Clamp to box surface
    fm_vec3_t closest = fm_vec3_t{{ 
      fmaxf(-halfSize.x, fminf(localSpherePos.x, halfSize.x)),
      fmaxf(-halfSize.y, fminf(localSpherePos.y, halfSize.y)),
      fmaxf(-halfSize.z, fminf(localSpherePos.z, halfSize.z))
    }};

    fm_vec3_t diff = localSpherePos - closest;
    float distSq = fm_vec3_len2(&diff);

    if(distSq >= radius * radius) return false;

    float dist = sqrtf(distSq);
    fm_vec3_t localNormal;
    if(dist < FM_EPSILON) {
      // Sphere center is inside box — find shortest escape axis
      float dx = halfSize.x - fabsf(localSpherePos.x);
      float dy = halfSize.y - fabsf(localSpherePos.y);
      float dz = halfSize.z - fabsf(localSpherePos.z);
      if(dx <= dy && dx <= dz) {
        localNormal = fm_vec3_t{{copysignf(1.0f, localSpherePos.x), 0.0f, 0.0f}};
        result.penetration = dx + radius;
      } else if(dy <= dz) {
        localNormal = fm_vec3_t{{0.0f, copysignf(1.0f, localSpherePos.y), 0.0f}};
        result.penetration = dy + radius;
      } else {
        localNormal = fm_vec3_t{{0.0f, 0.0f, copysignf(1.0f, localSpherePos.z)}};
        result.penetration = dz + radius;
      }
    } else {
      localNormal = fm_vec3_t{{diff.x * (1.0f / dist), diff.y * (1.0f / dist), diff.z * (1.0f / dist)}};
      result.penetration = radius - dist;
    }

    result.normal = matrix3Vec3Mul(boxRot, localNormal);
    fm_vec3_t worldClosest = matrix3Vec3Mul(boxRot, closest) + box->worldCenter();
    result.contactB = worldClosest;
    result.contactA = sphere->worldCenter() - result.normal * radius;
    return true;
  }


  /// @brief Analytical sphere-capsule collision using closest-point-on-segment + radius check.
  /// @param sphere The sphere collider.
  /// @param capsule The capsule collider.
  /// @param result The result of the collision detection.
  /// @return True if a collision is detected, false otherwise.
  static bool analyticalSphereCapsule(const Collider *sphere, const Collider *capsule, EpaResult &result) {
    if(!sphere || !capsule) return false;
    if(sphere->shapeType() != ShapeType::Sphere || capsule->shapeType() != ShapeType::Capsule) return false;

    float rS = sphere->sphereShape().radius;
    float rC = capsule->capsuleShape().radius;
    float hh = capsule->capsuleShape().innerHalfHeight;

    // Capsule axis endpoints in world space
    fm_vec3_t localUp = fm_vec3_t{{0.0f, hh, 0.0f}};
    fm_vec3_t rotUp = matrix3Vec3Mul(capsule->rotationMatrix(), localUp);
    fm_vec3_t capTop = capsule->worldCenter() + rotUp;
    fm_vec3_t capBot = capsule->worldCenter() - rotUp;

    // Closest point on capsule segment to sphere center
    fm_vec3_t seg = capTop - capBot;
    float segLenSq = fm_vec3_len2(&seg);
    float t = 0.5f;
    if(segLenSq > FM_EPSILON) {
      fm_vec3_t capToSphere = sphere->worldCenter() - capBot;
      t = fm_vec3_dot(&capToSphere, &seg) / segLenSq;
      if(t < 0.0f) t = 0.0f;
      if(t > 1.0f) t = 1.0f;
    }
    fm_vec3_t closestOnSeg = capBot + seg * t;

    float combinedRadius = rS + rC;
  fm_vec3_t diff = sphere->worldCenter() - closestOnSeg;
    float distSq = fm_vec3_len2(&diff);

    if(distSq >= combinedRadius * combinedRadius) return false;

    float dist = sqrtf(distSq);
    if(dist < FM_EPSILON) {
      result.normal = VEC3_UP;
    } else {
      result.normal = diff * (1.0f / dist);
    }

    result.penetration = combinedRadius - dist;
    result.contactA = sphere->worldCenter() - result.normal * rS;
    result.contactB = closestOnSeg + result.normal * rC;
    return true;
  }


  /// @brief Analytical capsule-capsule collision using closest-points-between-segments + radius check.
  /// Avoids full GJK+EPA for this common collision pair.
  /// @param capsuleA 
  /// @param capsuleB 
  /// @param result 
  /// @return 
  static bool analyticalCapsuleCapsule(const Collider *capsuleA, const Collider *capsuleB, EpaResult &result) {
    if(!capsuleA || !capsuleB) return false;
    if(capsuleA->shapeType() != ShapeType::Capsule || capsuleB->shapeType() != ShapeType::Capsule) return false;

    float rA = capsuleA->capsuleShape().radius;
    float rB = capsuleB->capsuleShape().radius;
    float hhA = capsuleA->capsuleShape().innerHalfHeight;
    float hhB = capsuleB->capsuleShape().innerHalfHeight;

    // Capsule A axis endpoints in world space
    fm_vec3_t localUpA = fm_vec3_t{{0.0f, hhA, 0.0f}};
    fm_vec3_t rotUpA = matrix3Vec3Mul(capsuleA->rotationMatrix(), localUpA);
    fm_vec3_t topA = capsuleA->worldCenter() + rotUpA;
    fm_vec3_t botA = capsuleA->worldCenter() - rotUpA;

    // Capsule B axis endpoints in world space
    fm_vec3_t localUpB = fm_vec3_t{{0.0f, hhB, 0.0f}};
    fm_vec3_t rotUpB = matrix3Vec3Mul(capsuleB->rotationMatrix(), localUpB);
    fm_vec3_t topB = capsuleB->worldCenter() + rotUpB;
    fm_vec3_t botB = capsuleB->worldCenter() - rotUpB;

    // Closest points between two line segments
    fm_vec3_t d1 = topA - botA; // direction of segment A
    fm_vec3_t d2 = topB - botB; // direction of segment B
    fm_vec3_t r = botA - botB;

    float a = fm_vec3_dot(&d1, &d1); // squared length of seg A
    float e = fm_vec3_dot(&d2, &d2); // squared length of seg B
    float f = fm_vec3_dot(&d2, &r);

    float s = 0.0f;
    float t = 0.0f;

    if(a <= FM_EPSILON && e <= FM_EPSILON) {
      // Both degenerate to points
      s = 0.0f;
      t = 0.0f;
    } else if(a <= FM_EPSILON) {
      // Segment A degenerates to a point
      s = 0.0f;
      t = f / e;
      if(t < 0.0f) t = 0.0f;
      if(t > 1.0f) t = 1.0f;
    } else {
      float c = fm_vec3_dot(&d1, &r);
      if(e <= FM_EPSILON) {
        // Segment B degenerates to a point
        t = 0.0f;
        s = -c / a;
        if(s < 0.0f) s = 0.0f;
        if(s > 1.0f) s = 1.0f;
      } else {
        // General case
        float b = fm_vec3_dot(&d1, &d2);
        float denom = a * e - b * b;

        if(denom > FM_EPSILON) {
          s = (b * f - c * e) / denom;
          if(s < 0.0f) s = 0.0f;
          if(s > 1.0f) s = 1.0f;
        } else {
          s = 0.0f;
        }

        t = (b * s + f) / e;
        if(t < 0.0f) {
          t = 0.0f;
          s = -c / a;
          if(s < 0.0f) s = 0.0f;
          if(s > 1.0f) s = 1.0f;
        } else if(t > 1.0f) {
          t = 1.0f;
          s = (b - c) / a;
          if(s < 0.0f) s = 0.0f;
          if(s > 1.0f) s = 1.0f;
        }
      }
    }

    fm_vec3_t closestA = botA + d1 * s;
    fm_vec3_t closestB = botB + d2 * t;

    float combinedRadius = rA + rB;
    fm_vec3_t diff = closestA - closestB;
    float distSq = fm_vec3_len2(&diff);

    if(distSq >= combinedRadius * combinedRadius) return false;

    float dist = sqrtf(distSq);
    if(dist < FM_EPSILON) {
      result.normal = VEC3_UP;
    } else {
      result.normal = diff * (1.0f / dist);
    }

    result.penetration = combinedRadius - dist;
    result.contactA = closestA - result.normal * rA;
    result.contactB = closestB + result.normal * rB;
    return true;
  }


  // ── Analytical Box-Triangle (SAT) ────────────────────────────────

  // Forward declaration — used by analyticalBoxTriangle for contact point selection
  static float contactTriangleArea2(const fm_vec3_t &p0, const fm_vec3_t &p1, const fm_vec3_t &p2);

  /// @brief Clip a polygon against one side of a box slab (keep vertices with coord >= -limit).
  /// @param inVerts  Input polygon vertices.
  /// @param inCount  Number of input vertices.
  /// @param outVerts Output polygon vertices (must have room for inCount+1).
  /// @param axis     Component index to clip against (0=x, 1=y, 2=z).
  /// @param limit    Half-extent along that axis (clip plane at -limit).
  /// @return Number of output vertices.
  static int clipPolygonToBoxSlab(const fm_vec3_t *inVerts, int inCount,
                                  fm_vec3_t *outVerts, int axis, float limit) {
    int outCount = 0;
    for(int i = 0; i < inCount; ++i) {
      const fm_vec3_t &cur = inVerts[i];
      const fm_vec3_t &nxt = inVerts[(i + 1) % inCount];
      const float cVal = (&cur.x)[axis];
      const float nVal = (&nxt.x)[axis];
      const bool cInside = (cVal >= -limit);
      const bool nInside = (nVal >= -limit);
      if(cInside) outVerts[outCount++] = cur;
      if(cInside != nInside) {
        float t = (-limit - cVal) / (nVal - cVal);
        outVerts[outCount++] = cur + (nxt - cur) * t;
      }
    }
    return outCount;
  }

  /// @brief SAT overlap test on a single axis. Returns false if a separating axis is found.
  ///
  /// Normalization for depth and axis is deferred to caller
  ///
  /// @param boxHalf          Box half-extents projected onto axis.
  /// @param triMin           Minimum triangle projection.
  /// @param triMax           Maximum triangle projection.
  /// @param axisLen2         Squared length of the test axis.
  /// @param bestFound        Whether any valid axis has been recorded yet.
  /// @param bestDepthUnnorm  Current best unnormalized depth.
  /// @param bestAxisLen2     Squared length of the current best axis.
  /// @param bestAxis         Current best axis (unnormalized, unsigned).
  /// @param bestSign         Sign to apply when normalizing the best axis.
  /// @param testAxis         The axis being tested.
  /// @return true if intervals overlap (no separating axis), false if separated.
  static bool satAxisTest(float boxHalf, float triMin, float triMax,
                          float axisLen2,
                          bool &bestFound, float &bestDepthUnnorm, float &bestAxisLen2,
                          fm_vec3_t &bestAxis, float &bestSign,
                          const fm_vec3_t &testAxis) {
    if(axisLen2 < FM_EPSILON * FM_EPSILON) return true; // degenerate — skip
    float depth1 = boxHalf - triMin;  // overlap from the positive side
    float depth2 = triMax + boxHalf;  // overlap from the negative side
    if(depth1 < 0.0f || depth2 < 0.0f) return false; // separated
    // choose the shallower overlap direction
    float depth, sign;
    if(depth1 < depth2) { depth = depth1; sign =  1.0f; }
    else                { depth = depth2; sign = -1.0f; }
    // cross-multiplied comparison avoids sqrtf
    if(!bestFound || depth * depth * bestAxisLen2 < bestDepthUnnorm * bestDepthUnnorm * axisLen2) {
      bestFound = true;
      bestDepthUnnorm = depth;
      bestAxisLen2 = axisLen2;
      bestAxis = testAxis;
      bestSign = sign;
    }
    return true;
  }

  /// @brief Analytical SAT-based Box vs Triangle collision in box local space.
  ///
  /// Works entirely in the box's local frame so the box is axis-aligned.
  /// On overlap the minimum-penetration axis is selected and contact points are generated.
  ///
  /// @param proxy      ColliderProxy with the box collider already transformed
  ///                   into mesh local space (worldCenter = box center in mesh
  ///                   space, rotation/rotationT = relative orientation).
  /// @param boxShape   The box's half-extent definition.
  /// @param v0w        Triangle vertex 0 in mesh local space.
  /// @param v1w        Triangle vertex 1 in mesh local space.
  /// @param v2w        Triangle vertex 2 in mesh local space.
  /// @param triNormal  Triangle normal in mesh local space.
  /// @param results    Array to receive contact data on success (mesh local space).
  /// @param maxResults Maximum number of contact points to generate.
  /// @return Number of contact points generated (0 = no collision).
  static int analyticalBoxTriangle(
      const ColliderProxy &proxy,
      const BoxShape      &boxShape,
      const fm_vec3_t &v0w, const fm_vec3_t &v1w, const fm_vec3_t &v2w,
      EpaResult *results, int maxResults)
  {
    const fm_vec3_t &h = boxShape.halfSize;

    // Transform triangle vertices into box local space (box center = origin, axes aligned)
    const fm_vec3_t v0 = matrix3Vec3Mul(proxy.spaceToShape, v0w - proxy.worldCenter);
    const fm_vec3_t v1 = matrix3Vec3Mul(proxy.spaceToShape, v1w - proxy.worldCenter);
    const fm_vec3_t v2 = matrix3Vec3Mul(proxy.spaceToShape, v2w - proxy.worldCenter);

    // Triangle edges
    const fm_vec3_t e0 = v1 - v0;
    const fm_vec3_t e1 = v2 - v1;
    const fm_vec3_t e2 = v0 - v2;

    float bestDepthUnnorm = 0.0f;
    float bestAxisLen2 = 1.0f;
    float bestSign = 1.0f;
    bool bestFound = false;
    fm_vec3_t bestAxis = VEC3_UP;

    // --- 3 box face normals (X, Y, Z) ---
    float triMinX = fminf(v0.x, fminf(v1.x, v2.x));
    float triMaxX = fmaxf(v0.x, fmaxf(v1.x, v2.x));
    if(!satAxisTest(h.x, triMinX, triMaxX, 1.0f, bestFound, bestDepthUnnorm, bestAxisLen2, bestAxis, bestSign, VEC3_RIGHT)) return 0;

    float triMinY = fminf(v0.y, fminf(v1.y, v2.y));
    float triMaxY = fmaxf(v0.y, fmaxf(v1.y, v2.y));
    if(!satAxisTest(h.y, triMinY, triMaxY, 1.0f, bestFound, bestDepthUnnorm, bestAxisLen2, bestAxis, bestSign, VEC3_UP)) return 0;

    float triMinZ = fminf(v0.z, fminf(v1.z, v2.z));
    float triMaxZ = fmaxf(v0.z, fmaxf(v1.z, v2.z));
    if(!satAxisTest(h.z, triMinZ, triMaxZ, 1.0f, bestFound, bestDepthUnnorm, bestAxisLen2, bestAxis, bestSign, VEC3_FORWARD)) return 0;


    // Triangle face normal
    {
      fm_vec3_t localN = Coll::MeshCollider::triangleNormalFromVertices(v0, v1, v2);
      float boxHalf = fabsf(localN.x) * h.x + fabsf(localN.y) * h.y + fabsf(localN.z) * h.z;
      float triProj = fm_vec3_dot(&localN, &v0); // all tri verts project to same value
      float triMin = triProj, triMax = triProj;
      float l2 = fm_vec3_len2(&localN);
      if(!satAxisTest(boxHalf, triMin, triMax, l2, bestFound, bestDepthUnnorm, bestAxisLen2, bestAxis, bestSign, localN)) return 0;
    }


    // 9 edge-pair cross products: box_axis × tri_edge (Akenine-Möller specialization)
    // Since box axes are cardinal (1,0,0), (0,1,0), (0,0,1) in local space, the cross
    // products, projections, and half-extent computations simplify to 2-component forms.
    {
      const fm_vec3_t *edges[3] = {&e0, &e1, &e2};
      for(int j = 0; j < 3; ++j) {
        const fm_vec3_t &ej = *edges[j];

        // (1,0,0) × edge = (0, -edge.z, edge.y)
        {
          float l2 = ej.z * ej.z + ej.y * ej.y;
          if (l2 >= FM_EPSILON * FM_EPSILON)
          {
            float boxHalf = fabsf(ej.z) * h.y + fabsf(ej.y) * h.z;
            float p0 = -v0.y * ej.z + v0.z * ej.y;
            float p1 = -v1.y * ej.z + v1.z * ej.y;
            float p2 = -v2.y * ej.z + v2.z * ej.y;
            float tMin = fminf(p0, fminf(p1, p2));
            float tMax = fmaxf(p0, fmaxf(p1, p2));
            fm_vec3_t axis = fm_vec3_t{{0.0f, -ej.z, ej.y}};
            if (!satAxisTest(boxHalf, tMin, tMax, l2, bestFound, bestDepthUnnorm, bestAxisLen2, bestAxis, bestSign, axis))
              return 0;
          }
        }

        // (0,1,0) × edge = (edge.z, 0, -edge.x)
        {
          float l2 = ej.z * ej.z + ej.x * ej.x;
          if (l2 >= FM_EPSILON * FM_EPSILON)
          {
            float boxHalf = fabsf(ej.z) * h.x + fabsf(ej.x) * h.z;
            float p0 = v0.x * ej.z - v0.z * ej.x;
            float p1 = v1.x * ej.z - v1.z * ej.x;
            float p2 = v2.x * ej.z - v2.z * ej.x;
            float tMin = fminf(p0, fminf(p1, p2));
            float tMax = fmaxf(p0, fmaxf(p1, p2));
            fm_vec3_t axis = fm_vec3_t{{ej.z, 0.0f, -ej.x}};
            if (!satAxisTest(boxHalf, tMin, tMax, l2, bestFound, bestDepthUnnorm, bestAxisLen2, bestAxis, bestSign, axis))
              return 0;
          }
        }

        // (0,0,1) × edge = (-edge.y, edge.x, 0)
        {
          float l2 = ej.y * ej.y + ej.x * ej.x;
          if (l2 >= FM_EPSILON * FM_EPSILON)
          {
            float boxHalf = fabsf(ej.y) * h.x + fabsf(ej.x) * h.y;
            float p0 = -v0.x * ej.y + v0.y * ej.x;
            float p1 = -v1.x * ej.y + v1.y * ej.x;
            float p2 = -v2.x * ej.y + v2.y * ej.x;
            float tMin = fminf(p0, fminf(p1, p2));
            float tMax = fmaxf(p0, fmaxf(p1, p2));
            fm_vec3_t axis = fm_vec3_t{{-ej.y, ej.x, 0.0f}};
            if (!satAxisTest(boxHalf, tMin, tMax, l2, bestFound, bestDepthUnnorm, bestAxisLen2, bestAxis, bestSign, axis))
              return 0;
          }
        }
      }
    }

    // --- Overlap confirmed — normalize the winning axis (single sqrtf) ---
    {
      float invLen = 1.0f / sqrtf(bestAxisLen2);
      bestAxis = bestAxis * (bestSign * invLen);
    }

    // Ensure normal points from triangle toward box center (origin in box local space)
    fm_vec3_t triCenter = (v0 + v1 + v2) * (1.0f / 3.0f);
    if(fm_vec3_dot(&bestAxis, &triCenter) > 0.0f) bestAxis = -bestAxis;

    // --- Contact point generation via Sutherland-Hodgman polygon clipping ---
    // Clip triangle polygon against the 6 box slab planes along bestAxis reference face
    fm_vec3_t clipA[10], clipB[10];
    clipA[0] = v0; clipA[1] = v1; clipA[2] = v2;
    int n = 3;

    // Clip against each of the 6 slab boundaries (+x, -x, +y, -y, +z, -z)
    for(int axis = 0; axis < 3; ++axis) {
      float limit = (&h.x)[axis];
      // Clip against -limit (keep coord >= -limit)
      n = clipPolygonToBoxSlab(clipA, n, clipB, axis, limit);
      if(n < 1) return 0;
      // Clip against +limit by flipping sign: keep -coord >= -limit → coord <= limit
      // Flip the clipped axis, clip, then flip back
      for(int k = 0; k < n; ++k) (&clipB[k].x)[axis] = -(&clipB[k].x)[axis];
      n = clipPolygonToBoxSlab(clipB, n, clipA, axis, limit);
      if(n < 1) return 0;
      for(int k = 0; k < n; ++k) (&clipA[k].x)[axis] = -(&clipA[k].x)[axis];
    }

    // --- Select up to maxResults well-distributed contact points from clipped polygon ---
    float boxHalfAlongNormal = fabsf(bestAxis.x) * h.x + fabsf(bestAxis.y) * h.y + fabsf(bestAxis.z) * h.z;

    int selected[10];
    int selectedCount = 0;

    // Step 1: Always include the deepest penetrating point
    float deepestPen = -1e30f;
    int deepestIdx = 0;
    for(int i = 0; i < n; ++i) {
      float pen = fm_vec3_dot(&clipA[i], &bestAxis) + boxHalfAlongNormal;
      if(pen > deepestPen) { deepestPen = pen; deepestIdx = i; }
    }
    selected[selectedCount++] = deepestIdx;

    // Step 2: Point farthest from the deepest (maximize spread)
    if(n > 1 && maxResults > 1) {
      float bestDist2 = -1.0f;
      int farthestIdx = -1;
      for(int i = 0; i < n; ++i) {
        if(i == deepestIdx) continue;
        fm_vec3_t d = clipA[i] - clipA[deepestIdx];
        float dist2 = fm_vec3_len2(&d);
        if(dist2 > bestDist2) { bestDist2 = dist2; farthestIdx = i; }
      }
      if(farthestIdx >= 0) selected[selectedCount++] = farthestIdx;
    }

    // Step 3: Point that maximizes triangle area with the first two (best coverage)
    if(n > 2 && maxResults > 2 && selectedCount >= 2) {
      float bestArea = -1.0f;
      int bestAreaIdx = -1;
      for(int i = 0; i < n; ++i) {
        if(i == selected[0] || i == selected[1]) continue;
        float area = contactTriangleArea2(clipA[selected[0]], clipA[selected[1]], clipA[i]);
        if(area > bestArea) { bestArea = area; bestAreaIdx = i; }
      }
      if(bestAreaIdx >= 0) selected[selectedCount++] = bestAreaIdx;
    }

    // --- Generate results for selected points ---
    fm_vec3_t worldNormal = vec3NormalizeOrFallback(matrix3Vec3Mul(proxy.normalToSpace, bestAxis), VEC3_UP);
    int resultCount = 0;

    for(int k = 0; k < selectedCount && resultCount < maxResults; ++k) {
      fm_vec3_t p = clipA[selected[k]];
      float pen = fm_vec3_dot(&p, &bestAxis) + boxHalfAlongNormal;
      if(pen < FM_EPSILON) continue;

      // Contact on box surface: project triangle point onto box face along normal
      fm_vec3_t contactOnBox = p - bestAxis * pen;

      // Transform back to mesh local space
      results[resultCount].normal = worldNormal;
      results[resultCount].penetration = pen;
      results[resultCount].contactA = matrix3Vec3Mul(proxy.shapeToSpace, contactOnBox) + proxy.worldCenter;
      results[resultCount].contactB = matrix3Vec3Mul(proxy.shapeToSpace, p) + proxy.worldCenter;
      resultCount++;
    }

    return resultCount;
  }


  // ── Analytical Sphere-Triangle ───────────────────────────────────

  /// @brief Analytical sphere-triangle collision using closest-point distance test.
  ///
  /// Finds the closest point on the triangle to the sphere center. If the
  /// distance is less than the sphere radius the shapes overlap and a contact point is generated. 
  /// This is a closed-form solution that replaces the iterative GJK+EPA path for sphere-vs-triangle pairs.
  ///
  /// All inputs and outputs are in mesh local space
  ///
  /// @param proxy      ColliderProxy with the sphere collider transformed into
  ///                   mesh local space (worldCenter = sphere center in mesh
  ///                   space, rotation matrices = relative orientation).
  /// @param sphere     The sphere shape definition.
  /// @param v0         Triangle vertex 0 in mesh local space.
  /// @param v1         Triangle vertex 1 in mesh local space.
  /// @param v2         Triangle vertex 2 in mesh local space.
  /// @param triNormal  Triangle normal in mesh local space.
  /// @param result     Single EpaResult to receive contact data (mesh local space).
  /// @return True if a collision is detected, false otherwise.
  static bool analyticalSphereTriangle(
      const ColliderProxy &proxy,
      const SphereShape   &sphere,
      const fm_vec3_t &v0, const fm_vec3_t &v1, const fm_vec3_t &v2,
      const fm_vec3_t &triNormal,
      EpaResult &result)
  {
    const float radius = sphere.radius;
    const fm_vec3_t &center = proxy.worldCenter;

    // Conservative early-out
    fm_vec3_t closest = closestPointOnTriangle(center, v0, v1, v2);
    fm_vec3_t diff = center - closest;
    float dist = fm_vec3_len(&diff);

    //conservative early-out if closest point is outside the sphere radius
    //scaled by the maximum inverse meshscale component
    if(dist >= proxy.effectiveRadius) {
      return false;
    }
    fm_vec3_t normal = triNormal;
    if(dist > FM_EPSILON) {
      normal = diff * (1.0f / dist);
    }

    // Compute contactA via the sphere support function
    // This should also handle mesh scale which distorts the sphere into an
    // ellipsoid in mesh-local space.
    fm_vec3_t negNormal = -normal;
    fm_vec3_t localDir = matrix3Vec3Mul(proxy.shapeToSpaceTranspose, negNormal);
    fm_vec3_t localSupport = sphere.support(localDir);
    fm_vec3_t contactA = matrix3Vec3Mul(proxy.shapeToSpace, localSupport) + center;

    //penetration
    fm_vec3_t contactToClosest = closest - contactA;
    float penetration = fm_vec3_dot(&contactToClosest, &normal);

    // Only reject non-overlapping contacts — match EPA which reports all positive penetrations
    if(penetration < FM_EPSILON) return false;

    // Derive contactB from contactA + normal * penetration to match EPA output convention
    result.normal = normal;
    result.penetration = penetration;
    result.contactA = contactA;
    result.contactB = contactA + normal * penetration;
    return true;
  }


  // Area-based contact point reduction


  /// @brief Computes the squared area of a triangle formed by three points.
  /// @param p0 First point of the triangle.
  /// @param p1 Second point of the triangle.
  /// @param p2 Third point of the triangle.
  /// @return 
  static float contactTriangleArea2(const fm_vec3_t &p0, const fm_vec3_t &p1, const fm_vec3_t &p2) {
    fm_vec3_t e0 = p1 - p0;
    fm_vec3_t e1 = p2 - p0;
    fm_vec3_t cross;
    fm_vec3_cross(&cross, &e0, &e1);
    return fm_vec3_len2(&cross);
  }


  /// @brief Selects which existing contact point to replace when the manifold is full, using an area-maximizing heuristic.
  /// 1. Always keep the deepest penetrating point.
  /// 2. Among the remaining candidates, pick the configuration that maximizes contact area.
  /// @param points Array of existing contact points.
  /// @param pointCount Number of existing contact points.
  /// @param newContactA The new contact point on object A.
  /// @param newPenetration The penetration depth of the new contact point.
  /// @return Index of the contact point to replace.
  static int selectContactPointToReplace(const ContactPoint points[MAX_CONTACT_POINTS_PER_PAIR], int pointCount, const fm_vec3_t &newContactA, float newPenetration) {
    // Find deepest existing point — this one is always kept
    int deepestIdx = 0;
    float deepestPen = points[0].penetration;
    for(int i = 1; i < pointCount; ++i) {
      if(points[i].penetration > deepestPen) {
        deepestPen = points[i].penetration;
        deepestIdx = i;
      }
    }

    // If the new point is the deepest overall, we keep it and choose among existing points to replace
    // For each candidate removal, compute the area of the triangle formed by the remaining 3 + new point
    // Select the removal that maximizes this area

    // Gather all 4 candidate contact positions on A side (3 existing + 1 new)
    fm_vec3_t pts[MAX_CONTACT_POINTS_PER_PAIR + 1];
    for(int i = 0; i < pointCount; ++i) pts[i] = points[i].contactA;
    pts[MAX_CONTACT_POINTS_PER_PAIR] = newContactA;

    float bestArea = -1.0f;
    int replaceIdx = 0;

    for(int exclude = 0; exclude < pointCount; ++exclude) {
      // Never exclude the deepest existing point (unless new point is deeper)
      if(exclude == deepestIdx && newPenetration <= deepestPen) continue;

      // Build triangle from the 3 remaining existing points + the new point
      fm_vec3_t triPts[4];
      int triCount = 0;
      for(int j = 0; j < pointCount; ++j) {
        if(j != exclude) triPts[triCount++] = pts[j];
      }
      triPts[triCount] = pts[MAX_CONTACT_POINTS_PER_PAIR]; // new point

      float area = 0.0f;
      // Compute area of the quad as sum of two triangles
      if constexpr (MAX_CONTACT_POINTS_PER_PAIR >= 4) {
            area = contactTriangleArea2(triPts[0], triPts[1], triPts[2])
                  + contactTriangleArea2(triPts[0], triPts[2], triPts[3]);
      } else if constexpr (MAX_CONTACT_POINTS_PER_PAIR == 3) {
            area = contactTriangleArea2(triPts[0], triPts[1], triPts[2]);
      }


      if(area > bestArea) {
        bestArea = area;
        replaceIdx = exclude;
      }
    }

    return replaceIdx;
  }


  // ── Contact constraint caching ────────────────────────────────────

  /// @brief Cache or update a ContactConstraint for a newly detected contact pair, using the provided EPA result and contact parameters.
  /// @param rigidBodyA 
  /// @param colliderA 
  /// @param meshColliderA 
  /// @param objectA 
  /// @param rigidBodyB 
  /// @param colliderB 
  /// @param meshColliderB 
  /// @param objectB 
  /// @param result 
  /// @param combinedFriction 
  /// @param combinedBounce 
  /// @param isTrigger 
  /// @param respondsA 
  /// @param respondsB 
  /// @param triangleIndex 
  /// @return 
  ContactConstraint *collideCacheContactConstraint(
    RigidBody *rigidBodyA, Collider *colliderA, MeshCollider *meshColliderA, Object *objectA,
    RigidBody *rigidBodyB, Collider *colliderB, MeshCollider *meshColliderB, Object *objectB, const EpaResult &result,
    float combinedFriction, float combinedBounce, bool isTrigger, bool respondsA, bool respondsB, int triangleIndex) {

    CollisionScene *scene = collisionSceneGetInstance();
    EpaResult orderedResult = result;

    orderedResult.normal = makeSafeContactNormal(orderedResult.normal, orderedResult.contactA, orderedResult.contactB);

    const bool isColliderPair = colliderA && colliderB && !meshColliderA && !meshColliderB;
    const bool hasMeshOnA = meshColliderA && !meshColliderB && !colliderA && colliderB;

    if(hasMeshOnA) {
      swapConstraintOrder(rigidBodyA, colliderA, meshColliderA, objectA, rigidBodyB, colliderB, meshColliderB, objectB, orderedResult);
      std::swap(respondsA, respondsB);
    }

    if(isColliderPair && shouldSwapColliderPairOrder(colliderA, colliderB)) {
      swapConstraintOrder(rigidBodyA, colliderA, meshColliderA, objectA, rigidBodyB, colliderB, meshColliderB, objectB, orderedResult);
      std::swap(respondsA, respondsB);
    }

    ContactConstraintKey key;
    if(colliderA && colliderB) {
      key = makeColliderPairConstraintKey(colliderA, colliderB);
    } else if(colliderA && meshColliderB && triangleIndex >= 0) {
      key = isTrigger
        ? makeColliderMeshConstraintKey(colliderA, meshColliderB)
        : makeColliderMeshConstraintKey(colliderA, meshColliderB, static_cast<uint16_t>(triangleIndex));
    } else {
      return nullptr;
    }

    ContactConstraint *existing = scene->findCachedConstraint(key);

    if(existing) {
      // Update existing constraint
      existing->rigidBodyA = rigidBodyA;
      existing->colliderA = colliderA;
      existing->meshColliderA = meshColliderA;
      existing->objectA = objectA;
      existing->rigidBodyB = rigidBodyB;
      existing->colliderB = colliderB;
      existing->meshColliderB = meshColliderB;
      existing->objectB = objectB;
      existing->isActive = true;
      existing->isTrigger = isTrigger;
      existing->normal = orderedResult.normal;
      vec3CalculateTangents(orderedResult.normal, existing->tangentU, existing->tangentV);
      existing->combinedFriction = combinedFriction;
      existing->combinedBounce = combinedBounce;
      existing->respondsA = respondsA;
      existing->respondsB = respondsB;

      if(isTrigger) {
        existing->pointCount = 1;
        ContactPoint &point = existing->points[0];
        point = ContactPoint{};
        point.contactA = orderedResult.contactA;
        point.contactB = orderedResult.contactB;
        point.point = (orderedResult.contactA + orderedResult.contactB) * 0.5f;
        point.penetration = orderedResult.penetration;
        point.active = true;
        return existing;
      }

      // Try to match new contact to an existing point by proximity
      float MATCH_DIST_SQ = 0.02f * scene->getPhysicsScale() * scene->getPhysicsScale(); // 2cm threshold (scaled)
      int matchedIdx = -1;
      float bestDistSq = MATCH_DIST_SQ;
      for(int i = 0; i < existing->pointCount; ++i) {
        fm_vec3_t diff = existing->points[i].contactA - orderedResult.contactA;
        float distA = fm_vec3_len2(&diff);
        diff = existing->points[i].contactB - orderedResult.contactB;
        float distB = fm_vec3_len2(&diff);
        float minDist = fminf(distA, distB);
        if(minDist < bestDistSq) {
          bestDistSq = minDist;
          matchedIdx = i;
        }
      }

      ContactPoint *target = nullptr;
      int targetIdx = -1;
      if(matchedIdx >= 0) {
        // Reuse existing point (preserves accumulated impulses for warm starting)
        target = &existing->points[matchedIdx];
        targetIdx = matchedIdx;
      } else if(existing->pointCount < MAX_CONTACT_POINTS_PER_PAIR) {
        // Add new point
        targetIdx = existing->pointCount;
        target = &existing->points[targetIdx];
        existing->pointCount++;
        target->accumulatedNormalImpulse = 0.0f;
        target->accumulatedTangentImpulseU = 0.0f;
        target->accumulatedTangentImpulseV = 0.0f;
      } else {
        // Full manifold: use Bullet-style area-maximizing heuristic to select which point to replace.
        // This keeps the deepest point and maximizes contact polygon coverage for better torque resistance.
        int replaceIdx = selectContactPointToReplace(existing->points, existing->pointCount, orderedResult.contactA, orderedResult.penetration);
        targetIdx = replaceIdx;
        target = &existing->points[replaceIdx];
        target->accumulatedNormalImpulse = 0.0f;
        target->accumulatedTangentImpulseU = 0.0f;
        target->accumulatedTangentImpulseV = 0.0f;
      }

      if(target) {
        target->contactA = orderedResult.contactA;
        target->contactB = orderedResult.contactB;
        target->point = (orderedResult.contactA + orderedResult.contactB) * 0.5f;
        target->penetration = orderedResult.penetration;
        target->active = true;

        target->localPointA = contactLocalPointFromWorldPoint(target->contactA, rigidBodyA, colliderA, meshColliderA);
        target->localPointB = contactLocalPointFromWorldPoint(target->contactB, rigidBodyB, colliderB, meshColliderB);
      }

      // Force all non-target points through current-frame revalidation.
      const float revalidationSeparationLimit = -0.05f * scene->getPhysicsScale();
      for(int i = 0; i < existing->pointCount; ++i) {
        ContactPoint &cp = existing->points[i];
        if(i == targetIdx) continue;

        cp.active = false;

        fm_vec3_t diff = cp.contactA - cp.contactB;
        float pen = -fm_vec3_dot(&diff, &existing->normal);
        if(pen > revalidationSeparationLimit) {
          cp.penetration = pen;
          cp.active = true;
        }
      }

      int writeIdx = 0;
      for(int i = 0; i < existing->pointCount; ++i) {
        if(!existing->points[i].active) continue;
        if(writeIdx != i) {
          existing->points[writeIdx] = existing->points[i];
        }
        ++writeIdx;
      }
      existing->pointCount = writeIdx;

      return existing;
    }

    // Create new constraint
    ContactConstraint *cc = scene->createCachedConstraint(
      key,
      rigidBodyA, colliderA, meshColliderA, objectA,
      rigidBodyB, colliderB, meshColliderB, objectB);
    if(!cc) return nullptr;
    cc->normal = orderedResult.normal;
    vec3CalculateTangents(orderedResult.normal, cc->tangentU, cc->tangentV);
    cc->combinedFriction = combinedFriction;
    cc->combinedBounce = combinedBounce;
    cc->isActive = true;
    cc->isTrigger = isTrigger;
    cc->respondsA = respondsA;
    cc->respondsB = respondsB;
    cc->pointCount = 1;

    ContactPoint &cp = cc->points[0];
    cp = ContactPoint{};
    cp.contactA = orderedResult.contactA;
    cp.contactB = orderedResult.contactB;
    cp.point = (orderedResult.contactA + orderedResult.contactB) * 0.5f;
    cp.penetration = orderedResult.penetration;
    cp.active = true;

    if(isTrigger) {
      return cc;
    }

    cp.localPointA = contactLocalPointFromWorldPoint(cp.contactA, rigidBodyA, colliderA, meshColliderA);
    cp.localPointB = contactLocalPointFromWorldPoint(cp.contactB, rigidBodyB, colliderB, meshColliderB);

    return cc;
  }


  /// @brief Directly fills a contact constraint with multiple SAT-derived contact points in one pass.
  /// Avoids repeated key lookups and proximity matching that collideCacheContactConstraint would
  /// perform when called per-point. Preserves warm-started impulses by proximity-matching against
  /// previously cached points.
  /// @param rigidBodyA 
  /// @param colliderA 
  /// @param objectA 
  /// @param meshColliderB 
  /// @param objectB 
  /// @param results 
  /// @param resultCount 
  /// @param combinedFriction 
  /// @param combinedBounce 
  /// @param respondsA 
  /// @param triangleIndex 
  /// @return 
  static ContactConstraint *collideCacheSatContactConstraint(
    RigidBody *rigidBodyA, Collider *colliderA, Object *objectA,
    MeshCollider *meshColliderB, Object *objectB,
    const EpaResult *results, int resultCount,
    float combinedFriction, float combinedBounce,
    bool respondsA, int triangleIndex) {

    if(resultCount <= 0 || !colliderA || !meshColliderB || triangleIndex < 0) return nullptr;

    CollisionScene *scene = collisionSceneGetInstance();
    fm_vec3_t normal = makeSafeContactNormal(results[0].normal, results[0].contactA, results[0].contactB);

    ContactConstraintKey key = makeColliderMeshConstraintKey(colliderA, meshColliderB, static_cast<uint16_t>(triangleIndex));

    ContactConstraint *cc = scene->findCachedConstraint(key);

    // Save old points for warm-start matching before we overwrite them
    ContactPoint oldPoints[MAX_CONTACT_POINTS_PER_PAIR]{};
    int oldCount = 0;
    if(cc) {
      oldCount = cc->pointCount;
      for(int i = 0; i < oldCount; ++i) oldPoints[i] = cc->points[i];
    } else {
      cc = scene->createCachedConstraint(key,
        rigidBodyA, colliderA, nullptr, objectA,
        nullptr, nullptr, meshColliderB, objectB);
      if(!cc) return nullptr;
    }

    cc->rigidBodyA = rigidBodyA;
    cc->colliderA = colliderA;
    cc->meshColliderA = nullptr;
    cc->objectA = objectA;
    cc->rigidBodyB = nullptr;
    cc->colliderB = nullptr;
    cc->meshColliderB = meshColliderB;
    cc->objectB = objectB;
    cc->isActive = true;
    cc->isTrigger = false;
    cc->normal = normal;
    vec3CalculateTangents(normal, cc->tangentU, cc->tangentV);
    cc->combinedFriction = combinedFriction;
    cc->combinedBounce = combinedBounce;
    cc->respondsA = respondsA;
    cc->respondsB = false;

    int newCount = resultCount < MAX_CONTACT_POINTS_PER_PAIR ? resultCount : MAX_CONTACT_POINTS_PER_PAIR;
    float MATCH_DIST_SQ = 0.02f * scene->getPhysicsScale() * scene->getPhysicsScale();
    bool oldClaimed[MAX_CONTACT_POINTS_PER_PAIR] = {};

    for(int i = 0; i < newCount; ++i) {
      const EpaResult &r = results[i];
      ContactPoint &cp = cc->points[i];

      // Find closest unclaimed old point for warm-start impulse transfer
      int bestOld = -1;
      float bestDistSq = MATCH_DIST_SQ;
      for(int j = 0; j < oldCount; ++j) {
        if(oldClaimed[j]) continue;
        fm_vec3_t diff = oldPoints[j].contactA - r.contactA;
        float distSq = fm_vec3_len2(&diff);
        if(distSq < bestDistSq) {
          bestDistSq = distSq;
          bestOld = j;
        }
      }

      if(bestOld >= 0) {
        oldClaimed[bestOld] = true;
        cp.accumulatedNormalImpulse = oldPoints[bestOld].accumulatedNormalImpulse;
        cp.accumulatedTangentImpulseU = oldPoints[bestOld].accumulatedTangentImpulseU;
        cp.accumulatedTangentImpulseV = oldPoints[bestOld].accumulatedTangentImpulseV;
      } else {
        cp.accumulatedNormalImpulse = 0.0f;
        cp.accumulatedTangentImpulseU = 0.0f;
        cp.accumulatedTangentImpulseV = 0.0f;
      }

      cp.contactA = r.contactA;
      cp.contactB = r.contactB;
      cp.point = (r.contactA + r.contactB) * 0.5f;
      cp.penetration = r.penetration;
      cp.active = true;
      cp.localPointA = contactLocalPointFromWorldPoint(cp.contactA, rigidBodyA, colliderA, nullptr);
      cp.localPointB = contactLocalPointFromWorldPoint(cp.contactB, nullptr, nullptr, meshColliderB);
    }

    cc->pointCount = newCount;
    return cc;
  }


  /// @brief Performs a collision test between a collider and a single Mesh triangle. Used as a subroutine for object-to-mesh collision detection.
  ///
  /// Hint: This function is designed to be called with the collider already transformed into the mesh's local space.
  /// @param colliderProxyMeshSpace Collider proxy containing the original collider and additional mesh-space transform data for GJK support.
  /// @param rigidBody Pointer to the rigid body associated with the collider, if any.
  /// @param mesh Reference to the mesh collider.
  /// @param triangleIndex Index of the triangle within the mesh to test against.
  /// @return True if a collision is detected, false otherwise.
  bool collideDetectObjectToTriangle(ColliderProxy *colliderProxyMeshSpace, RigidBody *rigidBody, const MeshCollider &mesh, int triangleIndex) {
    const bool isTriggerContact = colliderProxyMeshSpace->collider->isTrigger();
    const bool colliderRespondsToMesh = colliderProxyMeshSpace->collider->readsMeshCollider(&mesh);

    Object *objectA = colliderProxyMeshSpace->collider->ownerObject();
    Object *objectB = mesh.ownerObject();

    if(!colliderRespondsToMesh && !mesh.readsCollider(colliderProxyMeshSpace->collider)) {
      return false;
    }

    const float combinedFriction = fminf(colliderProxyMeshSpace->collider->friction(), mesh.friction());
    const float combinedBounce = fmaxf(colliderProxyMeshSpace->collider->bounce(), mesh.bounce());

    MeshTriangle tri;
    tri.vertices = nullptr;
    tri.tri = mesh.triangleIndices(triangleIndex);
    tri.normal = mesh.triangleNormal(triangleIndex);
    tri.mesh = &mesh;

    // --- Analytical Box-Triangle fast path (SAT) ---
    if(colliderProxyMeshSpace->collider->shapeType() == ShapeType::Box && !isTriggerContact) {
      const BoxShape &box = colliderProxyMeshSpace->collider->boxShape();
      const fm_vec3_t v0 = tri.localVertex(0);
      const fm_vec3_t v1 = tri.localVertex(1);
      const fm_vec3_t v2 = tri.localVertex(2);

      EpaResult satResults[MAX_CONTACT_POINTS_PER_PAIR];
      int satCount = analyticalBoxTriangle(*colliderProxyMeshSpace, box, v0, v1, v2, satResults, MAX_CONTACT_POINTS_PER_PAIR);
      if(satCount > 0) {
        for(int i = 0; i < satCount; ++i) {
          mesh.localResultToWorld(satResults[i]);
        }
        collideCacheSatContactConstraint(
            rigidBody, colliderProxyMeshSpace->collider, objectA,
            const_cast<MeshCollider *>(&mesh), objectB,
            satResults, satCount,
            combinedFriction, combinedBounce, colliderRespondsToMesh, triangleIndex);
        return true;
      }
      return false;
    }

    // --- Analytical Sphere-Triangle fast path (closest-point distance test) ---
    if(colliderProxyMeshSpace->collider->shapeType() == ShapeType::Sphere && !isTriggerContact) {
      const SphereShape &sphere = colliderProxyMeshSpace->collider->sphereShape();
      const fm_vec3_t v0 = tri.localVertex(0);
      const fm_vec3_t v1 = tri.localVertex(1);
      const fm_vec3_t v2 = tri.localVertex(2);

      EpaResult sphereResult;

      if(analyticalSphereTriangle(*colliderProxyMeshSpace, sphere, v0, v1, v2, tri.normal, sphereResult)) {
        mesh.localResultToWorld(sphereResult);

        collideCacheContactConstraint(
            rigidBody, colliderProxyMeshSpace->collider, nullptr, objectA,
            nullptr, nullptr, const_cast<MeshCollider *>(&mesh), objectB,
            sphereResult, combinedFriction, combinedBounce, isTriggerContact, colliderRespondsToMesh, false, triangleIndex);
        return true;
      }
      return false;
    }

    // --- General GJK + EPA path for all other shape types ---
    Simplex simplex;
    fm_vec3_t firstDir = ((tri.localVertex(0) + tri.localVertex(1) + tri.localVertex(2)) / 3.0f) - colliderProxyMeshSpace->worldCenter;
    if(fm_vec3_len2(&firstDir) < FM_EPSILON * FM_EPSILON) firstDir = VEC3_RIGHT;

    bool gjkOverlap = gjkCheckForOverlap(
      simplex,
      colliderProxyMeshSpace, colliderProxyGjkSupport,
      &tri, meshTriangleGjkSupport,
      firstDir
    );
    if(!gjkOverlap) return false;

    // Triggers only need overlap confirmation
    if (isTriggerContact)
    {
      const fm_vec3_t v0 = tri.worldVertex(0);
      const fm_vec3_t v1 = tri.worldVertex(1);
      const fm_vec3_t v2 = tri.worldVertex(2);
      const fm_vec3_t triCenter = (v0 + v1 + v2) / 3.0f;

      EpaResult dummyResult;
      dummyResult.normal = makeSafeContactNormal(mesh.localNormalToWorld(tri.normal), colliderProxyMeshSpace->collider->worldCenter(), triCenter);
      dummyResult.penetration = 0.0f;
      dummyResult.contactA = colliderProxyMeshSpace->collider->worldCenter();
      dummyResult.contactB = triCenter;

      collideCacheContactConstraint(
          rigidBody, colliderProxyMeshSpace->collider, nullptr, objectA,
          nullptr, nullptr, const_cast<MeshCollider *>(&mesh), objectB,
          dummyResult, 0.0f, 0.0f, true, false, false, triangleIndex);

      return true;
    }

    struct EpaResult epaResult;

    bool epaSuccess = epaSolve(
            simplex,
            colliderProxyMeshSpace, colliderProxyGjkSupport,
            &tri, meshTriangleGjkSupport,
            epaResult);
    if (epaSuccess)
    {
      mesh.localResultToWorld(epaResult);

      collideCacheContactConstraint(
          rigidBody, colliderProxyMeshSpace->collider, nullptr, objectA,
          nullptr, nullptr, const_cast<MeshCollider *>(&mesh), objectB,
          epaResult, combinedFriction, combinedBounce, isTriggerContact, colliderRespondsToMesh, false, triangleIndex);

      return true;
    }
    return false;
  }

  // ── Object-to-mesh ──────────────────────────────────────────────

  /// @brief Detects collision between a collider and a mesh and caches contact constraints if needed.
  /// @param collider The collider to test against the mesh.
  /// @param rigidBody The rigid body associated with the collider.
  /// @param mesh The mesh collider to test against.
  bool collideDetectObjectToMesh(Collider *collider, RigidBody *rigidBody, const MeshCollider &mesh) {

    // Transform the collider's world AABB into the mesh's local space for tree query
    AABB queryAABB = mesh.hasTransform()
      ? mesh.worldAabbToLocal(collider->worldAabb())
      : collider->worldAabb();

    // Query local-space mesh AABB tree for candidate triangles
    constexpr int MAX_CANDIDATES = 20; // arbitrary limit to avoid extreme cases
    NodeProxy candidates[MAX_CANDIDATES];
    int count = mesh.queryTriangleNodes(queryAABB, candidates, MAX_CANDIDATES);

    if(count <= 0) return false;

    bool detected = false;

    // Precompute the collider proxy in mesh local space once and reuse it across all candidate triangles.
    ColliderProxy colliderInMeshSpace;
    colliderInMeshSpace.collider = collider;

    const bool meshHasTransform = mesh.hasTransform();
    if(meshHasTransform) {
      colliderInMeshSpace.worldCenter = mesh.toLocalSpace(collider->worldCenter());

      Matrix3x3 relativeRotation = matrix3Mul(mesh.inverseRotationMatrix(), collider->rotationMatrix());
      if(mesh.hasScale()) {
        fm_vec3_t inverseScaleVec = vec3ReciprocalScaleComponents(mesh.ownerObject()->scale);
        Matrix3x3 inverseScale = diagonalMatrix(inverseScaleVec);
        colliderInMeshSpace.effectiveRadius = max(inverseScaleVec.x, max(inverseScaleVec.y, inverseScaleVec.z)) * collider->sphereShape().radius;
        colliderInMeshSpace.shapeToSpace = matrix3Mul(inverseScale, relativeRotation);
      } else {
        colliderInMeshSpace.effectiveRadius = collider->sphereShape().radius;
        colliderInMeshSpace.shapeToSpace = relativeRotation;
      }

      colliderInMeshSpace.shapeToSpaceTranspose = matrix3Transpose(colliderInMeshSpace.shapeToSpace);
      colliderInMeshSpace.spaceToShape = matrix3Inverse(colliderInMeshSpace.shapeToSpace);
      colliderInMeshSpace.normalToSpace = matrix3Transpose(colliderInMeshSpace.spaceToShape);
    } else {
      colliderInMeshSpace.worldCenter = collider->worldCenter();
      colliderInMeshSpace.shapeToSpace = collider->rotationMatrix();
      colliderInMeshSpace.shapeToSpaceTranspose = matrix3Transpose(colliderInMeshSpace.shapeToSpace);
      colliderInMeshSpace.spaceToShape = collider->inverseRotationMatrix();
      colliderInMeshSpace.normalToSpace = colliderInMeshSpace.shapeToSpace;
    }


    // For every candidate triangle perform precise collision test
    for(int i = 0; i < count; ++i) {
      int triIndex = mesh.triangleIndexForNode(candidates[i]);
      if(triIndex < 0) continue;

      // If there is a collision between the collider and the current triangle and the collider is a Trigger
      // we can skip the rest of the candidates since triggers just need to report that a collision happened
      // and don't need detailed contact information for each triangle.
      if(collideDetectObjectToTriangle(&colliderInMeshSpace, rigidBody, mesh, triIndex)) {
        detected = true;
        if(collider->isTrigger()) return true;
      }
    }
    return detected;
  }

  
  /// @brief Detects collision between two colliders and caches contact constraints if needed.
  /// @param colliderA The first collider.
  /// @param rbA The rigid body associated with the first collider.
  /// @param colliderB The second collider.
  /// @param rbB The rigid body associated with the second collider.
  bool collideDetectObjectToObject(Collider *colliderA, RigidBody *rbA, Collider *colliderB, RigidBody *rbB) {
    if(!colliderA || !colliderB) return false;

    CollisionScene *scene = collisionSceneGetInstance();
    const bool aReadsB = colliderA->readsCollider(colliderB);
    const bool bReadsA = colliderB->readsCollider(colliderA);

    if(rbA && rbB && rbA->isSleeping() && rbB->isSleeping() && !colliderA->isTrigger() && !colliderB->isTrigger()) return false;

    if(colliderA && colliderB) {
      if(!aReadsB && !bReadsA) return false;
      if(colliderA->isTrigger() && colliderB->isTrigger()) return false;
    }

    // Try analytical closed-form tests first before falling back to GJK+EPA for general convex shapes.
    
    EpaResult result;
    bool analyticalHit = false;
    bool hasAnalyticalPath = false;
    // Sphere-Sphere
    if(colliderA->shapeType() == ShapeType::Sphere && colliderB->shapeType() == ShapeType::Sphere) {
      hasAnalyticalPath = true;
      analyticalHit = analyticalSphereSphere(colliderA, colliderB, result);
    } 
    
    // Sphere-Box (both directions)
    else if(colliderA->shapeType() == ShapeType::Sphere && colliderB->shapeType() == ShapeType::Box) {
      hasAnalyticalPath = true;
      analyticalHit = analyticalSphereBox(colliderA, colliderB, result);
    }  else if(colliderB->shapeType() == ShapeType::Sphere && colliderA->shapeType() == ShapeType::Box) {
      hasAnalyticalPath = true;
      analyticalHit = analyticalSphereBox(colliderB, colliderA, result);
      if(analyticalHit) {
        result.normal = -result.normal;
        fm_vec3_t tmp = result.contactA;
        result.contactA = result.contactB;
        result.contactB = tmp;
      }
    } 
    // Sphere-Capsule (both directions)
    else if(colliderA->shapeType() == ShapeType::Sphere && colliderB->shapeType() == ShapeType::Capsule) {
      hasAnalyticalPath = true;
      analyticalHit = analyticalSphereCapsule(colliderA, colliderB, result);
    } else if(colliderB->shapeType() == ShapeType::Sphere && colliderA->shapeType() == ShapeType::Capsule) {
      hasAnalyticalPath = true;
      analyticalHit = analyticalSphereCapsule(colliderB, colliderA, result);
      if(analyticalHit) {
        result.normal = -result.normal;
        fm_vec3_t tmp = result.contactA;
        result.contactA = result.contactB;
        result.contactB = tmp;
      }
    } 
    // Capsule-Capsule
    else if(colliderA->shapeType() == ShapeType::Capsule && colliderB->shapeType() == ShapeType::Capsule) {
      hasAnalyticalPath = true;
      analyticalHit = analyticalCapsuleCapsule(colliderA, colliderB, result);
    }

    // TODO: Implement Box-Box with SAT?

    // If an analytical test exists for the pair but reports no collision, we can skip GJK+EPA entirely.
    if(hasAnalyticalPath && !analyticalHit) return false;

    bool doEpa = false;
    Simplex simplex;
    ColliderProxy proxyA;
    ColliderProxy proxyB;

    // Fall back to GJK + EPA if no analytical algorithm for the pair exists
    if(!hasAnalyticalPath) {
      // Try to reuse cached separating axis from previous frame
      fm_vec3_t firstDir = VEC3_ZERO;
      ContactConstraintKey lookupKey = makeColliderPairConstraintKey(colliderA, colliderB);
      ContactConstraint *cachedCC = scene->findCachedConstraint(lookupKey);
      if(cachedCC && fm_vec3_len2(&cachedCC->cachedSeparatingAxis) > FM_EPSILON * FM_EPSILON) {
        firstDir = cachedCC->cachedSeparatingAxis;
      } else {
        firstDir = colliderA->worldCenter() - colliderB->worldCenter();
      }
      if(fm_vec3_len2(&firstDir) < FM_EPSILON * FM_EPSILON) firstDir = VEC3_UP;

      proxyA.collider = colliderA;
      proxyA.worldCenter = colliderA->worldCenter();
      proxyA.shapeToSpace = colliderA->rotationMatrix();
      proxyA.shapeToSpaceTranspose = colliderA->inverseRotationMatrix();
      proxyA.spaceToShape = colliderA->inverseRotationMatrix();
      proxyA.normalToSpace = colliderA->rotationMatrix();

      proxyB.collider = colliderB;
      proxyB.worldCenter = colliderB->worldCenter();
      proxyB.shapeToSpace = colliderB->rotationMatrix();
      proxyB.shapeToSpaceTranspose = colliderB->inverseRotationMatrix();
      proxyB.spaceToShape = colliderB->inverseRotationMatrix();
      proxyB.normalToSpace = colliderB->rotationMatrix();

      simplex.nPoints = 0;
      fm_vec3_t separatingAxis{};
      bool overlapping = gjkCheckForOverlap(
        simplex,
        &proxyA, colliderProxyGjkSupport,
        &proxyB, colliderProxyGjkSupport,
        firstDir,
        &separatingAxis
      );

      if(!overlapping) {
        // Cache the last GJK search direction as separating axis for next frame
        if(cachedCC) {
          cachedCC->cachedSeparatingAxis = separatingAxis;
        }
        return false;
      }
      doEpa = true;

    }

    // Trigger pairs only need overlap confirmation, not a full contact manifold.
    // we can skip EPA and directly generate a fake contact that will be used for trigger events without any physics response.
    if ((colliderA && colliderA->isTrigger()) || (colliderB && colliderB->isTrigger())) {
      EpaResult dummyResult;
      dummyResult.normal = makeSafeContactNormal(VEC3_ZERO, colliderA->worldCenter(), colliderB->worldCenter());
      dummyResult.penetration = 0.0f;
      dummyResult.contactA = colliderA->worldCenter();
      dummyResult.contactB = colliderB->worldCenter();

      collideCacheContactConstraint(
        rbA, colliderA, nullptr, colliderA->ownerObject(),
        rbB, colliderB, nullptr, colliderB->ownerObject(),
        dummyResult,
        0.0f, 0.0f,
        true,
        false, false
      );
      return true;
    }

    // only do EPA if there is not already an analytical result and the GJK confirms overlap
    if (doEpa)
    {
      bool epaOk = epaSolve(
          simplex,
          &proxyA, colliderProxyGjkSupport,
          &proxyB, colliderProxyGjkSupport,
          result);

      if (!epaOk || result.penetration < FM_EPSILON)
        return false;
    }

    // Wake sleeping rigidBodies that are involved in the collision
    if(aReadsB && rbA && rbA->isSleeping()) scene->wakeRigidBodyIsland(rbA);
    if(bReadsA && rbB && rbB->isSleeping()) scene->wakeRigidBodyIsland(rbB);

    float combinedFriction = fmin(colliderA->friction(), colliderB->friction());
    float combinedBounce = fmaxf(colliderA->bounce(), colliderB->bounce());

    // Cache the constraint
    collideCacheContactConstraint(
      rbA, colliderA, nullptr, colliderA->ownerObject(),
      rbB, colliderB, nullptr, colliderB->ownerObject(),
      result, combinedFriction, combinedBounce, false, aReadsB, bReadsA);
    return true;
  }

} // namespace P64::Coll
