/**
 * @file gjk.cpp
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Gilbert–Johnson–Keerthi distance algorithm (GJK) implementation for collision detection. (see gjk.h)
 */
#include "collision/gjk.h"

using namespace P64::Coll;

namespace {
  constexpr int GJK_MAX_ITERATIONS = 24;

  void simplexMovePoint(Simplex &simplex, int to, int from) {
    simplex.points[to] = simplex.points[from];
    simplex.rigidBodyAPoint[to] = simplex.rigidBodyAPoint[from];
  }
}

fm_vec3_t *P64::Coll::simplexAddPoint(Simplex &simplex, const fm_vec3_t &aPoint, const fm_vec3_t &bPoint) {
  if(simplex.nPoints == GJK_MAX_SIMPLEX_SIZE) {
    //this normally shouldn't happen
    return nullptr;
  }

  int index = simplex.nPoints;
  simplex.rigidBodyAPoint[index] = aPoint;
  simplex.points[index] = aPoint - bPoint;
  ++simplex.nPoints;

  return &simplex.points[index];
}

bool P64::Coll::simplexCheck(Simplex &simplex, fm_vec3_t &nextDirection) {
  auto &lastAdded = simplex.points[simplex.nPoints - 1];
  auto aToOrigin = -lastAdded;

  if(simplex.nPoints == 2) {
    auto lastAddedToOther = simplex.points[0] - lastAdded;
    nextDirection = vec3TripleProduct(lastAddedToOther, aToOrigin, lastAddedToOther);

    if(fm_vec3_len2(&nextDirection) <= FM_EPSILON * FM_EPSILON) {
      nextDirection = vec3Perpendicular(lastAddedToOther);
    }
    return false;

  } else if(simplex.nPoints == 3) {
    auto ab = simplex.points[1] - lastAdded;
    auto ac = simplex.points[0] - lastAdded;
    fm_vec3_t normal;
    fm_vec3_cross(&normal, &ab, &ac);

    fm_vec3_t dirCheck;
    fm_vec3_cross(&dirCheck, &ab, &normal);
    if(fm_vec3_dot(&dirCheck, &aToOrigin) > 0.0f) {
      nextDirection = vec3TripleProduct(ab, aToOrigin, ab);
      if(fm_vec3_len2(&nextDirection) <= FM_EPSILON * FM_EPSILON) {
        nextDirection = normal;
      }
      // remove c
      simplexMovePoint(simplex, 0, 1);
      simplexMovePoint(simplex, 1, 2);
      simplex.nPoints = 2;
      return false;
    }

    fm_vec3_cross(&dirCheck, &normal, &ac);
    if(fm_vec3_dot(&dirCheck, &aToOrigin) > 0.0f) {
      nextDirection = vec3TripleProduct(ac, aToOrigin, ac);
      if(fm_vec3_len2(&nextDirection) <= FM_EPSILON * FM_EPSILON) {
        nextDirection = normal;
      }
      // remove b
      simplexMovePoint(simplex, 1, 2);
      simplex.nPoints = 2;
      return false;
    }

    if(fm_vec3_dot(&normal, &aToOrigin) > 0.0f) {
      nextDirection = normal;
      return false;
    }

    // change triangle winding
    simplexMovePoint(simplex, 3, 0);
    simplexMovePoint(simplex, 0, 1);
    simplexMovePoint(simplex, 1, 3);
    nextDirection = -normal;

  } else if(simplex.nPoints == 4) {
    int lastBehindIndex = -1;
    int lastInFrontIndex = -1;
    int isFrontCount = 0;

    fm_vec3_t normals[3];

    for(int i = 0; i < 3; ++i) {
      auto firstEdge = lastAdded - simplex.points[i];
      auto secondEdge = (i == 2) ? simplex.points[0] - simplex.points[i] : simplex.points[i + 1] - simplex.points[i];
      fm_vec3_cross(&normals[i], &firstEdge, &secondEdge);

      if(fm_vec3_dot(&aToOrigin, &normals[i]) > 0.0f) {
        ++isFrontCount;
        lastInFrontIndex = i;
      } else {
        lastBehindIndex = i;
      }
    }

    if(isFrontCount == 0) {
      return true; // origin enclosed
    } else if(isFrontCount == 1) {
      nextDirection = normals[lastInFrontIndex];
      if(lastInFrontIndex == 1) {
        simplexMovePoint(simplex, 0, 1);
        simplexMovePoint(simplex, 1, 2);
      } else if(lastInFrontIndex == 2) {
        simplexMovePoint(simplex, 1, 0);
        simplexMovePoint(simplex, 0, 2);
      }
      simplexMovePoint(simplex, 2, 3);
      simplex.nPoints = 3;
    } else if(isFrontCount == 2) {
      if(lastBehindIndex == 0) {
        simplexMovePoint(simplex, 0, 2);
      } else if(lastBehindIndex == 2) {
        simplexMovePoint(simplex, 0, 1);
      }
      simplexMovePoint(simplex, 1, 3);
      simplex.nPoints = 2;

      auto ab = simplex.points[0] - simplex.points[1];
      nextDirection = vec3TripleProduct(ab, aToOrigin, ab);
      if(fm_vec3_len2(&nextDirection) <= FM_EPSILON * FM_EPSILON) {
        nextDirection = vec3Perpendicular(ab);
      }
    } else {
      // origin in front of all three faces — reset to single point
      simplexMovePoint(simplex, 0, 3);
      simplex.nPoints = 1;
      nextDirection = aToOrigin;
    }
  }

  return false;
}

bool P64::Coll::gjkCheckForOverlap(
  Simplex &simplex,
  const void *colliderA, GjkSupportFunction colliderASupport,
  const void *colliderB, GjkSupportFunction colliderBSupport,
  const fm_vec3_t &firstDirection,
  fm_vec3_t *outSeparatingAxis
) {
  fm_vec3_t aPoint{};
  fm_vec3_t bPoint{};
  fm_vec3_t nextDirection{};
  fm_vec3_t dir = firstDirection;

  simplex.nPoints = 0;

  if(vec3IsZero(dir)) {
    dir = VEC3_RIGHT;
  }

  colliderASupport(colliderA, dir, aPoint);
  nextDirection = -dir;
  colliderBSupport(colliderB, nextDirection, bPoint);
  simplexAddPoint(simplex, aPoint, bPoint);

  for(int iteration = 0; iteration < GJK_MAX_ITERATIONS; ++iteration) {
    auto reverseDirection = -nextDirection;
    colliderASupport(colliderA, nextDirection, aPoint);
    colliderBSupport(colliderB, reverseDirection, bPoint);

    auto *addedPoint = simplexAddPoint(simplex, aPoint, bPoint);
    if(!addedPoint) {
      if(outSeparatingAxis) *outSeparatingAxis = nextDirection;
      return false;
    }

    if(fm_vec3_dot(addedPoint, &nextDirection) <= 0.0f) {
      if(outSeparatingAxis) *outSeparatingAxis = nextDirection;
      return false;
    }

    if(simplexCheck(simplex, nextDirection)) return true;
  }

  if(outSeparatingAxis) *outSeparatingAxis = nextDirection;
  return false;
}
