/**
 * @file raycast.cpp
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Raycast definitions and functions (see raycast.h)
 */
#include "collision/raycast.h"
#include "scene/object.h"
#include "collision/colliderShape.h"

namespace P64::Coll {

  static bool ray_sphere_intersection(const Raycast &ray, const fm_vec3_t &sphereCenter, float radius, uint16_t hitObjectId, RaycastHit &hit) {
    const float safeRadius = fabsf(radius);
    if(!std::isfinite(safeRadius)) return false;

    fm_vec3_t collToRay = ray.origin - sphereCenter;

    float a = fm_vec3_dot(&ray.dir, &ray.dir);
    if(a <= FM_EPSILON || !std::isfinite(a)) return false;

    float b = 2.0f * fm_vec3_dot(&collToRay, &ray.dir);
    float c = fm_vec3_dot(&collToRay, &collToRay) - safeRadius * safeRadius;

    float discriminant = b * b - 4.0f * a * c;
    if(!std::isfinite(discriminant)) return false;
    if(discriminant < 0.0f) return false;

    float sqrtDisc = sqrtf(discriminant);
    float t1 = (-b - sqrtDisc) / (2.0f * a);
    float t2 = (-b + sqrtDisc) / (2.0f * a);

    float t = t1;
    if(t < 0.0f) {
      t = t2;
      if(t < 0.0f) return false;
    }

    if(t > ray.maxDistance) return false;

    hit.didHit = true;
    hit.point = ray.origin + ray.dir * t;
    hit.normal = hit.point - sphereCenter;
    if(fm_vec3_len2(&hit.normal) <= FM_EPSILON * FM_EPSILON) {
      hit.normal = -ray.dir;
    } else {
      fm_vec3_norm(&hit.normal, &hit.normal);
    }
    hit.distance = t;
    hit.hitObjectId = hitObjectId;
    return true;
  }

  bool ray_sphere_intersection(const Raycast &ray, const Collider *coll, RaycastHit &hit) {
    const Object *owner = coll->ownerObject();
    return ray_sphere_intersection(ray, coll->worldCenter(), coll->sphereShape().radius, owner ? owner->id : 0, hit);
  }

  bool ray_box_intersection(const Raycast &ray, const Collider *coll, RaycastHit &hit) {
    Raycast localRay = ray;
    localRay.origin = coll->toLocalSpace(ray.origin);
    localRay.dir = coll->rotateToLocal(ray.dir);

    AABB local_box = coll->boxShape().boundingBox(nullptr); // Box AABB in local space

    float tMin = -INFINITY;
    float tMax = INFINITY;
    int hit_face = -1;

    // Check intersection with each pair of planes
    for(int i = 0; i < 3; ++i) {
      if(fabsf(localRay.dir.v[i]) < FM_EPSILON) {
        // Ray is parallel to planes, check if origin is between them
        if(localRay.origin.v[i] < local_box.min.v[i] || localRay.origin.v[i] > local_box.max.v[i]) {
          return false; // No intersection
        }
      } else {
        // Compute intersection withthe two planes
        float ood = 1.0f / localRay.dir.v[i];
        float t1 = (local_box.min.v[i] - localRay.origin.v[i]) * ood;
        float t2 = (local_box.max.v[i] - localRay.origin.v[i]) * ood;

        int face1 = i * 2; // Min face
        int face2 = i * 2 + 1; // Max face

        if(t1 > t2) {
          std::swap(t1, t2);
          std::swap(face1, face2);
        }

        if (t1 > tMin) {
          tMin = t1;
          hit_face = face1;
        }
        if (t2 < tMax) tMax = t2;

        if(tMin > tMax) return false; // No intersection
      }
    }

    if(tMin < 0.0f) {
      if(tMax < 0.0f || tMax > ray.maxDistance) return false;
      tMin = 0.0f; // Ray starts inside box; clamp to ray origin
    } else if(tMin > ray.maxDistance) {
      return false; // Intersection beyond max distance
    }

    //calculate local hit point and normal
    fm_vec3_t localHitPoint = localRay.origin + localRay.dir * tMin;
    fm_vec3_t localNormal = {0.0f, 0.0f, 0.0f};
    switch(hit_face) {
      case 0: localNormal.x = -1.0f; break; // -X face
      case 1: localNormal.x = 1.0f; break; // +X face
      case 2: localNormal.y = -1.0f; break; // -Y face
      case 3: localNormal.y = 1.0f; break; // +Y face
      case 4: localNormal.z = -1.0f; break; // -Z face
      case 5: localNormal.z = 1.0f; break; // +Z face
    }

    // Transform back to world space
    hit.point = coll->toWorldSpace(localHitPoint);
    hit.normal = coll->rotateToWorld(localNormal);
    hit.distance = tMin;
    hit.hitObjectId = coll->ownerObject() ? coll->ownerObject()->id : 0;
    hit.didHit = true;
    return true;
  }

  bool ray_capsule_intersection(const Raycast &ray, const Collider *coll, RaycastHit &hit) {
    Raycast localRay = ray;
    localRay.origin = coll->toLocalSpace(ray.origin);
    localRay.dir = coll->rotateToLocal(ray.dir);

    // First check if ray intersects infinite cylinder around capsule axis
    fm_vec3_t rayOriginXZ = {localRay.origin.x, 0.0f, localRay.origin.z};
    fm_vec3_t rayDirXZ = {localRay.dir.x, 0.0f, localRay.dir.z};
    float a = fm_vec3_dot(&rayDirXZ, &rayDirXZ);
    float b = 2.0f * fm_vec3_dot(&rayOriginXZ, &rayDirXZ);
    float c = fm_vec3_dot(&rayOriginXZ, &rayOriginXZ) - coll->capsuleShape().radius * coll->capsuleShape().radius;

    float discriminant = b * b - 4.0f * a * c;
    float tCylinder = std::numeric_limits<float>::max();
    fm_vec3_t cylinderNormal = {};

    if(discriminant >= 0.0f && std::isfinite(discriminant) && a > FM_EPSILON) {
      float sqrtDisc = sqrtf(discriminant);
      float t1 = (-b - sqrtDisc) / (2.0f * a);
      float t2 = (-b + sqrtDisc) / (2.0f * a);

      float t = t1 > 0.0f ? t1 : t2;

      if( t > 0.0f && t < ray.maxDistance) {
        fm_vec3_t hitPoint = localRay.origin + localRay.dir * t;
        if(fabsf(hitPoint.y) <= coll->capsuleShape().innerHalfHeight) {
          tCylinder = t;
          cylinderNormal = {hitPoint.x, 0.0f, hitPoint.z};
          vec3NormalizeOrFallback(cylinderNormal, (-localRay.dir));
          
        }
      }
    }

    // Next check intersection with the hemispherical ends
  fm_vec3_t capCenterTop = {0.0f, coll->capsuleShape().innerHalfHeight, 0.0f};
  fm_vec3_t capCenterBottom = {0.0f, -coll->capsuleShape().innerHalfHeight, 0.0f};

    RaycastHit capHitTop, capHitBottom;
  bool hitTop = ray_sphere_intersection(localRay, capCenterTop, coll->capsuleShape().radius, 0, capHitTop);
  bool hitBottom = ray_sphere_intersection(localRay, capCenterBottom, coll->capsuleShape().radius, 0, capHitBottom);

    // Determine closest valid hit
    float tSphere = std::numeric_limits<float>::max();
    fm_vec3_t sphereNormal = {};

    if(hitTop && capHitTop.distance < tSphere) {
      // check if hit is within hemisphere
      fm_vec3_t hitToCenter = capHitTop.point - capCenterTop;
      if(hitToCenter.y > 0.0f) {
        tSphere = capHitTop.distance;
        sphereNormal = capHitTop.normal;
      }
    }

    if(hitBottom && capHitBottom.distance < tSphere) {
      // check if hit is within hemisphere
      fm_vec3_t hitToCenter = capHitBottom.point - capCenterBottom;
      if(hitToCenter.y < 0.0f) {
        tSphere = capHitBottom.distance;
        sphereNormal = capHitBottom.normal;
      }
    }

    if(tCylinder == std::numeric_limits<float>::max() && tSphere == std::numeric_limits<float>::max()) {
      return false; // No hit
    }

    fm_vec3_t localHit;
    fm_vec3_t localNormal;
    float t;

    if(tCylinder < tSphere) {
      localHit = localRay.origin + localRay.dir * tCylinder;
      localNormal = cylinderNormal;
      t = tCylinder;
    } else {
      localHit = hitTop && capHitTop.distance < capHitBottom.distance ? capHitTop.point : capHitBottom.point;
      localNormal = sphereNormal;
      t = tSphere;
    }

    // Transform back to world space
    hit.point = coll->toWorldSpace(localHit);
    hit.normal = coll->rotateToWorld(localNormal);
    hit.distance = t;
    hit.hitObjectId = coll->ownerObject() ? coll->ownerObject()->id : 0;
    hit.didHit = true;
    return true;
  }

  bool ray_cylinder_intersection(const Raycast &ray, const Collider *coll, RaycastHit &hit) {
    Raycast localRay = ray;
    localRay.origin = coll->toLocalSpace(ray.origin);
    localRay.dir = coll->rotateToLocal(ray.dir);

    // Check intersection with infinite cylinder around the axis
    fm_vec3_t rayOriginXZ = {localRay.origin.x, 0.0f, localRay.origin.z};
    fm_vec3_t rayDirXZ = {localRay.dir.x, 0.0f, localRay.dir.z};
    float a = fm_vec3_dot(&rayDirXZ, &rayDirXZ);
    float b = 2.0f * fm_vec3_dot(&rayOriginXZ, &rayDirXZ);
    float c = fm_vec3_dot(&rayOriginXZ, &rayOriginXZ) - coll->cylinderShape().radius * coll->cylinderShape().radius;

    float discriminant = b * b - 4.0f * a * c;
    float tCylinder = std::numeric_limits<float>::max();
    fm_vec3_t cylinderNormal = {};

    if(discriminant >= 0.0f && std::isfinite(discriminant) && a > FM_EPSILON) {
      float sqrtDisc = sqrtf(discriminant);
      float t1 = (-b - sqrtDisc) / (2.0f * a);
      float t2 = (-b + sqrtDisc) / (2.0f * a);

      for(int i = 0; i < 2; ++i) {
        float t = i == 0 ? t1 : t2;
        if(t < 0.0f || t > ray.maxDistance) continue;

        fm_vec3_t hitPoint = localRay.origin + localRay.dir * t;
        if(fabsf(hitPoint.y) <= coll->cylinderShape().halfHeight) {

            tCylinder = t;
            cylinderNormal = {hitPoint.x, 0.0f, hitPoint.z};
            vec3NormalizeOrFallback(cylinderNormal, (-localRay.dir));
          
        }
      }
    }

    // Check intersection with the circular caps
    float tCap = std::numeric_limits<float>::max();
    fm_vec3_t capNormal = {};

    for(int i = 0; i < 2; ++i) {
      float y = i == 0 ? coll->cylinderShape().halfHeight : -coll->cylinderShape().halfHeight;
      fm_vec3_t capNormalLocal = {0.0f, i == 0 ? 1.0f : -1.0f, 0.0f};

      // Check if ray is parallel to cap plane
      if(fabsf(localRay.dir.y) < FM_EPSILON) {
        continue;
      }

      float t = (y - localRay.origin.y) / localRay.dir.y;
      if(t > 0.0f && t < ray.maxDistance && t < tCap){
        fm_vec3_t hitPoint = localRay.origin + localRay.dir * t;
        float distsq = hitPoint.x * hitPoint.x + hitPoint.z * hitPoint.z;
        if(distsq <= coll->cylinderShape().radius * coll->cylinderShape().radius) {
          tCap = t;
          capNormal = capNormalLocal;
        }
      }
    }

    // Determine closest valid hit
    if(tCylinder == std::numeric_limits<float>::max() && tCap == std::numeric_limits<float>::max()) {
      return false; // No hit
    }

    fm_vec3_t localHit;
    fm_vec3_t localNormal;
    float t;

    if(tCylinder < tCap) {

      localNormal = cylinderNormal;
      t = tCylinder;
    } else {
      localNormal = capNormal;
      t = tCap;
    }

    localHit = localRay.origin + localRay.dir * t;

    // Transform back to world space
    hit.point = coll->toWorldSpace(localHit);
    hit.normal = coll->rotateToWorld(localNormal);
    hit.distance = t;
    hit.hitObjectId = coll->ownerObject() ? coll->ownerObject()->id : 0;
    hit.didHit = true;
    return true;
  }

  bool ray_cone_intersection(const Raycast &ray, const Collider *coll, RaycastHit &hit) {
    Raycast localRay = ray;
    localRay.origin = coll->toLocalSpace(ray.origin);
    localRay.dir = coll->rotateToLocal(ray.dir);

    // Conse parameters
    float h = coll->coneShape().halfHeight * 2.0f;
    float tan_theta_sq = h > 0.0f ? (coll->coneShape().radius / h) * (coll->coneShape().radius / h) : 0.0f;
    fm_vec3_t apex = {0.0f, coll->coneShape().halfHeight, 0.0f};

    fm_vec3_t co = localRay.origin - apex;

    float a = localRay.dir.x * localRay.dir.x + localRay.dir.z * localRay.dir.z - tan_theta_sq * localRay.dir.y * localRay.dir.y;
    float b = 2.0f * (co.x * localRay.dir.x + co.z * localRay.dir.z - tan_theta_sq * co.y * localRay.dir.y);
    float c = co.x * co.x + co.z * co.z - tan_theta_sq * co.y * co.y;

    float discriminant = b * b - 4.0f * a * c;
    float tSide = std::numeric_limits<float>::max();
    fm_vec3_t sideNormal = {};

    if(discriminant >= 0.0f && std::isfinite(discriminant) && fabsf(a) > FM_EPSILON){
      float sqrtDisc = sqrtf(discriminant);
      float t1 = (-b - sqrtDisc) / (2.0f * a);
      float t2 = (-b + sqrtDisc) / (2.0f * a);

      // check both intersection points
      for (int i = 0; i < 2; ++i) {
        float t = i == 0 ? t1 : t2;
        if(t < 0.0f || t > ray.maxDistance || t >= tSide) continue;

        fm_vec3_t hitPoint = localRay.origin + localRay.dir * t;
        if(hitPoint.y >= -coll->coneShape().halfHeight && hitPoint.y <= coll->coneShape().halfHeight) {
          tSide = t;

          // Calculate normal
          fm_vec3_t axisToHit = {hitPoint.x, 0.0f, hitPoint.z};
          float axisToHitLen = sqrtf(axisToHit.x * axisToHit.x + axisToHit.z * axisToHit.z);
          if(axisToHitLen > FM_EPSILON) {
             axisToHit = axisToHit / axisToHitLen;
             float coneAngle = atanf(coll->coneShape().radius / h);
             sideNormal = {
              axisToHit.x,
              sinf(coneAngle),
              axisToHit.z
             };
             fm_vec3_norm(&sideNormal, &sideNormal);
          } else {
            sideNormal = {0.0f, 1.0f, 0.0f}; // Ray hits the tip of the cone
          }
        }
      }
    }

    // Check intersection with base disk
    float tBase = std::numeric_limits<float>::max();
    fm_vec3_t baseNormal = {0.0f, -1.0f, 0.0f};

    if(fabsf(localRay.dir.y) > FM_EPSILON) {
      float t = ( -coll->coneShape().halfHeight - localRay.origin.y) / localRay.dir.y;
      if(t > 0.0f && t < ray.maxDistance && t < tBase) {
        fm_vec3_t hitPoint = localRay.origin + localRay.dir * t;
        if(hitPoint.x * hitPoint.x + hitPoint.z * hitPoint.z <= coll->coneShape().radius * coll->coneShape().radius) {
          tBase = t;
        }
      }
    }

    // Determine closest valid hit
    if(tSide == std::numeric_limits<float>::max() && tBase == std::numeric_limits<float>::max()) {
      return false; // No hit
    }

    fm_vec3_t localHit;
    fm_vec3_t localNormal;
    float t;

    if(tSide < tBase) {
      localNormal = sideNormal;
      t = tSide;
    } else {
      localNormal = baseNormal;
      t = tBase;
    }

    localHit = localRay.origin + localRay.dir * t;
    hit.point = coll->toWorldSpace(localHit);
    hit.normal = coll->rotateToWorld(localNormal);
    hit.distance = t;
    hit.hitObjectId = coll->ownerObject() ? coll->ownerObject()->id : 0;
    hit.didHit = true;
    return true;
  }

  bool ray_pyramid_intersection(const Raycast &ray, const Collider *coll, RaycastHit &hit) {
    Raycast localRay = ray;
    localRay.origin = coll->toLocalSpace(ray.origin);
    localRay.dir = coll->rotateToLocal(ray.dir);

    const float halfHeight = coll->pyramidShape().halfHeight;
    const float baseHalfWidthX = coll->pyramidShape().baseHalfWidthX;
    const float baseHalfWidthZ = coll->pyramidShape().baseHalfWidthZ;

    const fm_vec3_t apex = {0.0f, halfHeight, 0.0f};
    const fm_vec3_t v0 = {-baseHalfWidthX, -halfHeight, -baseHalfWidthZ};
    const fm_vec3_t v1 = {baseHalfWidthX, -halfHeight, -baseHalfWidthZ};
    const fm_vec3_t v2 = {baseHalfWidthX, -halfHeight, baseHalfWidthZ};
    const fm_vec3_t v3 = {-baseHalfWidthX, -halfHeight, baseHalfWidthZ};

    float bestT = std::numeric_limits<float>::max();
    fm_vec3_t bestLocalPoint = {};
    fm_vec3_t bestLocalNormal = {};

    auto tryTriangle = [&](const fm_vec3_t &a, const fm_vec3_t &b, const fm_vec3_t &c) {
      const fm_vec3_t edge1 = b - a;
      const fm_vec3_t edge2 = c - a;

      fm_vec3_t h;
      fm_vec3_cross(&h, &localRay.dir, &edge2);
      const float det = fm_vec3_dot(&edge1, &h);
      if(fabsf(det) < FM_EPSILON) return;

      const float invDet = 1.0f / det;
      const fm_vec3_t s = localRay.origin - a;
      const float u = invDet * fm_vec3_dot(&s, &h);
      if(u < 0.0f || u > 1.0f) return;

      fm_vec3_t q;
      fm_vec3_cross(&q, &s, &edge1);
      const float v = invDet * fm_vec3_dot(&localRay.dir, &q);
      if(v < 0.0f || u + v > 1.0f) return;

      const float t = invDet * fm_vec3_dot(&edge2, &q);
      if(t < 0.0f || t > ray.maxDistance || t >= bestT) return;

      fm_vec3_t normal;
      fm_vec3_cross(&normal, &edge1, &edge2);
      if(fm_vec3_len2(&normal) < FM_EPSILON * FM_EPSILON) return;

      const fm_vec3_t faceCenter = (a + b + c) * (1.0f / 3.0f);
      if(fm_vec3_dot(&normal, &faceCenter) < 0.0f) {
        normal = -normal;
      }
      fm_vec3_norm(&normal, &normal);

      bestT = t;
      bestLocalPoint = localRay.origin + localRay.dir * t;
      bestLocalNormal = normal;
    };

    tryTriangle(v0, v1, v2);
    tryTriangle(v0, v2, v3);
    tryTriangle(apex, v0, v1);
    tryTriangle(apex, v1, v2);
    tryTriangle(apex, v2, v3);
    tryTriangle(apex, v3, v0);

    if(bestT == std::numeric_limits<float>::max()) {
      return false;
    }

    hit.point = coll->toWorldSpace(bestLocalPoint);
    hit.normal = coll->rotateToWorld(bestLocalNormal);
    hit.distance = bestT;
    hit.hitObjectId = coll->ownerObject() ? coll->ownerObject()->id : 0;
    hit.didHit = true;
    return true;
  }


  bool ray_collider_intersection(const Raycast &ray, const Collider *coll, RaycastHit &hit) {
    switch (coll->shapeType())
    {
    case ShapeType::Sphere:
      return ray_sphere_intersection(ray, coll, hit);
    case ShapeType::Box:
      return ray_box_intersection(ray, coll, hit);
    case ShapeType::Capsule:
      return ray_capsule_intersection(ray, coll, hit);
    case ShapeType::Cylinder:
      return ray_cylinder_intersection(ray, coll, hit);
    case ShapeType::Cone:
      return ray_cone_intersection(ray, coll, hit);
    case ShapeType::Pyramid:
      return ray_pyramid_intersection(ray, coll, hit);
    default:
      return false; // Unsupported shape type for ray intersection
      // If new shapes get added in the future, add their ray intersection logic here
    }
    return false;
  }

  bool ray_triangle_intersection(const Raycast &ray, const fm_vec3_t &v0, const fm_vec3_t &v1, const fm_vec3_t &v2, const fm_vec3_t &tri_norm, RaycastHit &hit) {
    // Calculate Triangl Edges
    fm_vec3_t edge1 = v1 - v0;
    fm_vec3_t edge2 = v2 - v0;

    //Check for degenerate triangle (zero area or very close to it)
    fm_vec3_t tri_normal;
    fm_vec3_cross(&tri_normal, &edge1, &edge2);
    if(fm_vec3_len2(&tri_normal) < FM_EPSILON * FM_EPSILON) {
      return false;
    }

    fm_vec3_t h;
    fm_vec3_cross(&h, &ray.dir, &edge2);
    float a = fm_vec3_dot(&edge1, &h);
    if(fabsf(a) < FM_EPSILON) {
      return false; // Ray is parallel to triangle
    }


    float f = 1.0f / a;
    fm_vec3_t s = ray.origin - v0;
    float u = f * fm_vec3_dot(&s, &h);

    // Check if hit is outside triangle (u parameter)
    // Allow for a small epsilon margin to account for floating point inaccuracies
    if(u < -FM_EPSILON || u > 1.0f + FM_EPSILON) {
      return false;
    }

    fm_vec3_t q;
    fm_vec3_cross(&q, &s, &edge1);
    float v = f * fm_vec3_dot(&ray.dir, &q);

    // Check if hit is outside triangle (v parameter)
    if(v < -FM_EPSILON || u + v > 1.0f + FM_EPSILON) {
      return false;
    }

    // Calculate t to intersection point
    float t = f * fm_vec3_dot(&edge2, &q);

    if (t > FM_EPSILON && t <= ray.maxDistance) {
      hit.distance = t;
      hit.normal = tri_norm;
      if(a < 0.0f) {
        hit.normal = -hit.normal; // Ensure normal faces against ray
      }

      float normal_len_sq = fm_vec3_len2(&hit.normal);
      if(normal_len_sq < FM_EPSILON * FM_EPSILON) {
        fm_vec3_norm(&hit.normal, &tri_normal);
        if(a < 0.0f) {
          hit.normal = -hit.normal;
        }
      } else if(fabsf(normal_len_sq - 1.0f) > FM_EPSILON) {
        fm_vec3_norm(&hit.normal, &hit.normal);
      }
      hit.point = ray.origin + ray.dir * t;
      hit.didHit = true;
      return true;
    }
    return false;
  }

  Raycast Raycast::create(const fm_vec3_t &origin, const fm_vec3_t &dir, float maxDist,
                          RaycastColliderTypeFlags collTypes, bool interactTrigger,
                          uint8_t readMask) {
    Raycast r;
    r.origin = origin;

    // Normalize direction
    float mag2 = fm_vec3_len2(&dir);
    if(mag2 < FM_EPSILON * FM_EPSILON) {
      r.dir = fm_vec3_t{{0.0f, -1.0f, 0.0f}};
    } else {
      r.dir = dir / sqrtf(mag2);
    }

    // Safe inverse direction for AABB ray tests
    r.invDir = fm_vec3_t{{ 
      fabsf(r.dir.x) > FM_EPSILON ? 1.0f / r.dir.x : copysignf(1.0f / FM_EPSILON, r.dir.x),
      fabsf(r.dir.y) > FM_EPSILON ? 1.0f / r.dir.y : copysignf(1.0f / FM_EPSILON, r.dir.y),
      fabsf(r.dir.z) > FM_EPSILON ? 1.0f / r.dir.z : copysignf(1.0f / FM_EPSILON, r.dir.z)
    }};

    r.maxDistance = maxDist > 0.0f ? maxDist : 0.0f;
    r.collTypes = collTypes;
    r.interactTrigger = interactTrigger;
    r.readMask = readMask;
    return r;
  }

} // namespace P64::Coll
