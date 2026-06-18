/**
 * @file epa.cpp
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Expanding Polytope Algorithm (EPA) implementation for collision detection. (see epa.h)
 */
#include "collision/epa.h"
#include "collision/collisionScene.h"

#include <cassert>

using namespace P64::Coll;

namespace {

  constexpr int EPA_MAX_ITERATIONS = 8;
  constexpr int EPA_MAX_SIMPLEX_POINTS = 4 + EPA_MAX_ITERATIONS;
  constexpr int EPA_MAX_SIMPLEX_TRIANGLES = 4 + EPA_MAX_ITERATIONS * 2;

  constexpr float EPA_CONVERGENCE_TOLERANCE = 0.005f;

  constexpr unsigned char NEXT_FACE_LUT[3] = {1, 2, 0};

  inline unsigned char nextFace(unsigned char index) {
    return NEXT_FACE_LUT[index];
  }

  /// Triangle topology data for the expanding polytope
  union TriangleIndexData {
    struct {
      unsigned char indices[3];
      unsigned char adjacentFaces[3];
      unsigned char oppositePoints[3];
    };
    int alignment;
  };

  /// A face of the expanding polytope
  struct SimplexTriangle {
    TriangleIndexData indexData{};
    float distanceToOrigin{0.0f};
    fm_vec3_t normal{};
  };

  enum SimplexFlags {
    FlagsNone = 0,
    FlagsSkipDistance = (1 << 0),
  };

  /// The expanding polytope structure with min-heap for closest-face tracking
  struct ExpandingSimplex {
    fm_vec3_t points[EPA_MAX_SIMPLEX_POINTS]{};
    fm_vec3_t aPoints[EPA_MAX_SIMPLEX_POINTS]{};
    SimplexTriangle triangles[EPA_MAX_SIMPLEX_TRIANGLES]{};
    short pointCount{0};
    short triangleCount{0};
    unsigned char triangleHeap[EPA_MAX_SIMPLEX_TRIANGLES]{};
    unsigned char triangleToHeapIndex[EPA_MAX_SIMPLEX_TRIANGLES]{};
    short flags{0};
  };

  inline int getParentIndex(int heapIndex) { return (heapIndex - 1) >> 1; }
  inline int getChildIndex(int heapIndex, int child) { return (heapIndex << 1) + 1 + child; }
  inline float getTriDistance(const ExpandingSimplex &s, int triIdx) { return s.triangles[triIdx].distanceToOrigin; }

  // --- Point management ---

  inline void addPoint(ExpandingSimplex &es, const fm_vec3_t &aPoint, const fm_vec3_t &diff) {
    short idx = es.pointCount;
    es.aPoints[idx] = aPoint;
    es.points[idx] = diff;
    ++es.pointCount;
  }

  // --- Min-heap operations ---

  int siftDown(ExpandingSimplex &es, int heapIndex) {
    int parentIdx = getParentIndex(heapIndex);
    float currentDist = getTriDistance(es, es.triangleHeap[heapIndex]);

    while(heapIndex > 0) {
      if(currentDist >= getTriDistance(es, es.triangleHeap[parentIdx])) break;

      unsigned char tmp = es.triangleHeap[heapIndex];
      es.triangleHeap[heapIndex] = es.triangleHeap[parentIdx];
      es.triangleHeap[parentIdx] = tmp;

      es.triangleToHeapIndex[es.triangleHeap[heapIndex]] = static_cast<unsigned char>(heapIndex);
      es.triangleToHeapIndex[es.triangleHeap[parentIdx]] = static_cast<unsigned char>(parentIdx);

      heapIndex = parentIdx;
      parentIdx = getParentIndex(heapIndex);
    }
    return heapIndex;
  }

  int siftUp(ExpandingSimplex &es, int heapIndex) {
    float currentDist = getTriDistance(es, es.triangleHeap[heapIndex]);

    while(heapIndex < es.triangleCount) {
      int swapWith = -1;
      int childIdx = getChildIndex(heapIndex, 0);

      if(childIdx >= es.triangleCount) break;

      float childDist = getTriDistance(es, es.triangleHeap[childIdx]);
      if(childDist < currentDist) swapWith = childIdx;

      if(childIdx + 1 < es.triangleCount) {
        float otherDist = getTriDistance(es, es.triangleHeap[childIdx + 1]);
        if(otherDist < currentDist && (swapWith == -1 || otherDist < childDist)) {
          swapWith = childIdx + 1;
        }
      }

      if(swapWith == -1) break;

      unsigned char tmp = es.triangleHeap[heapIndex];
      es.triangleHeap[heapIndex] = es.triangleHeap[swapWith];
      es.triangleHeap[swapWith] = tmp;

      es.triangleToHeapIndex[es.triangleHeap[heapIndex]] = static_cast<unsigned char>(heapIndex);
      es.triangleToHeapIndex[es.triangleHeap[swapWith]] = static_cast<unsigned char>(swapWith);

      heapIndex = swapWith;
    }
    return heapIndex;
  }

  void fixHeap(ExpandingSimplex &es, int heapIndex) {
    int next = siftUp(es, heapIndex);
    if(next != heapIndex) return;
    siftDown(es, next);
  }

  // --- Triangle operations ---

  inline void triangleInitNormal(ExpandingSimplex &es, SimplexTriangle &tri) {
    auto edgeB = es.points[tri.indexData.indices[1]] - es.points[tri.indexData.indices[0]];
    auto edgeC = es.points[tri.indexData.indices[2]] - es.points[tri.indexData.indices[0]];
    fm_vec3_cross(&tri.normal, &edgeB, &edgeC);
  }

  bool triangleCheckEdge(ExpandingSimplex &es, SimplexTriangle &tri, int index) {
    auto &pointA = es.points[tri.indexData.indices[index]];
    auto edge = es.points[tri.indexData.indices[nextFace(static_cast<unsigned char>(index))]] - pointA;
    auto toOrigin = -pointA;

    fm_vec3_t crossCheck;
    fm_vec3_cross(&crossCheck, &edge, &toOrigin);
    if(fm_vec3_dot(&crossCheck, &tri.normal) >= 0.0f) return false;

    float edgeLerp = fm_vec3_dot(&toOrigin, &edge);
    float edgeMagSq = fm_vec3_len2(&edge);

    if(edgeLerp < 0.0f) {
      edgeLerp = 0.0f;
    } else if(edgeLerp > edgeMagSq) {
      edgeLerp = 1.0f;
    } else {
      edgeLerp /= edgeMagSq;
    }

    auto nearest = pointA + edge * edgeLerp;
    tri.distanceToOrigin = fm_vec3_len(&nearest);
    return true;
  }

  void triangleDetermineDistance(ExpandingSimplex &es, SimplexTriangle &tri) {
    fm_vec3_norm(&tri.normal, &tri.normal);

    for(int i = 0; i < 3; ++i) {
      if(triangleCheckEdge(es, tri, i)) return;
    }
    tri.distanceToOrigin = fm_vec3_dot(&tri.normal, &es.points[tri.indexData.indices[0]]);
  }

  void triangleInit(ExpandingSimplex &es, TriangleIndexData &data, SimplexTriangle &tri) {
    tri.indexData = data;
    triangleInitNormal(es, tri);
  }

  void addTriangle(ExpandingSimplex &es, TriangleIndexData &data) {
    if(es.triangleCount == EPA_MAX_SIMPLEX_TRIANGLES) return;

    short idx = es.triangleCount;
    triangleInit(es, data, es.triangles[idx]);
    ++es.triangleCount;

    if(es.flags & FlagsSkipDistance) return;

    triangleDetermineDistance(es, es.triangles[idx]);
    es.triangleHeap[idx] = static_cast<unsigned char>(idx);
    es.triangleToHeapIndex[idx] = static_cast<unsigned char>(idx);
    siftDown(es, idx);
  }

  // inline SimplexTriangle &closestFace(ExpandingSimplex &es) {
  //   return es.triangles[es.triangleHeap[0]];
  // }

  // Use the min-heap to efficiently find the closest face
  inline SimplexTriangle& closestFace(ExpandingSimplex& es) {
    return es.triangles[es.triangleHeap[0]];
  }

  // --- Edge rotation for convexity ---

  void rotateEdge(ExpandingSimplex &es, SimplexTriangle &triA, int triAIndex, int heapIndex) {
    unsigned char triBIndex = triA.indexData.adjacentFaces[0];
    auto &triB = es.triangles[triBIndex];

    unsigned char rel0 = triA.indexData.oppositePoints[0];
    unsigned char rel1 = nextFace(rel0);
    unsigned char rel2 = nextFace(rel1);

    triA.indexData.adjacentFaces[0] = triB.indexData.adjacentFaces[rel2];
    triB.indexData.adjacentFaces[rel1] = triA.indexData.adjacentFaces[1];
    triA.indexData.adjacentFaces[1] = triBIndex;
    triB.indexData.adjacentFaces[rel2] = static_cast<unsigned char>(triAIndex);

    triA.indexData.indices[1] = triB.indexData.indices[rel0];
    triB.indexData.indices[rel2] = triA.indexData.indices[2];

    triA.indexData.oppositePoints[0] = triB.indexData.oppositePoints[rel2];
    triB.indexData.oppositePoints[rel1] = triA.indexData.oppositePoints[1];
    triA.indexData.oppositePoints[1] = rel1;
    triB.indexData.oppositePoints[rel2] = 0;

    auto &adjA = es.triangles[triA.indexData.adjacentFaces[0]];
    unsigned char adjIdx = nextFace(triA.indexData.oppositePoints[0]);
    adjA.indexData.adjacentFaces[adjIdx] = static_cast<unsigned char>(triAIndex);
    adjA.indexData.oppositePoints[adjIdx] = 2;

    auto &adjB = es.triangles[triB.indexData.adjacentFaces[rel1]];
    unsigned char adjBIdx = nextFace(triB.indexData.oppositePoints[rel1]);
    adjB.indexData.adjacentFaces[adjBIdx] = triBIndex;
    adjB.indexData.oppositePoints[adjBIdx] = rel0;

    triangleInitNormal(es, triA);
    if(!(es.flags & FlagsSkipDistance)) {
      triangleDetermineDistance(es, triA);
      fixHeap(es, heapIndex);
    }

    triangleInitNormal(es, triB);
    if(!(es.flags & FlagsSkipDistance)) {
      triangleDetermineDistance(es, triB);
      fixHeap(es, es.triangleToHeapIndex[triBIndex]);
    }
  }

  void checkRotate(ExpandingSimplex &es, int triIndex, int heapIndex) {
    auto &tri = es.triangles[triIndex];
    auto &adj = es.triangles[tri.indexData.adjacentFaces[0]];
    auto &oppPoint = es.points[adj.indexData.indices[tri.indexData.oppositePoints[0]]];
    auto &firstPoint = es.points[tri.indexData.indices[0]];

    auto offset = oppPoint - firstPoint;
    if(fm_vec3_dot(&offset, &tri.normal) > 0.0f) {
      rotateEdge(es, tri, triIndex, heapIndex);
    } else if(!(es.flags & FlagsSkipDistance)) {
      triangleDetermineDistance(es, tri);
      fixHeap(es, heapIndex);
    }
  }

  // --- Initial topology for a tetrahedron (4 triangular faces) ---

  TriangleIndexData gInitialData[] = {
    {{{0, 1, 2}, {3, 1, 2}, {2, 2, 2}}},
    {{{2, 1, 3}, {0, 3, 2}, {0, 1, 0}}},
    {{{0, 2, 3}, {0, 1, 3}, {1, 1, 0}}},
    {{{1, 0, 3}, {0, 2, 1}, {2, 1, 0}}},
  };

  void initExpandingSimplex(ExpandingSimplex &es, Simplex &simplex, int flags) {
    assert(simplex.nPoints == 4);
    es.triangleCount = 0;
    es.pointCount = 0;
    es.flags = static_cast<short>(flags);

    for(int i = 0; i < 4; ++i) {
      addPoint(es, simplex.rigidBodyAPoint[i], simplex.points[i]);
    }
    for(int i = 0; i < 4; ++i) {
      addTriangle(es, gInitialData[i]);
    }
  }

  // --- Polytope expansion ---

  void expandPolytope(ExpandingSimplex &es, int newPointIndex, int faceToRemoveIndex) {
    if(newPointIndex == -1) return;

    auto &faceToRemove = es.triangles[faceToRemoveIndex];
    TriangleIndexData existing = faceToRemove.indexData;

    unsigned char triIndices[3];
    triIndices[0] = static_cast<unsigned char>(faceToRemoveIndex);
    triIndices[1] = static_cast<unsigned char>(es.triangleCount);
    triIndices[2] = static_cast<unsigned char>(es.triangleCount + 1);

    for(int i = 0; i < 3; ++i) {
      TriangleIndexData next{};
      unsigned char nf = nextFace(static_cast<unsigned char>(i));
      unsigned char nnf = nextFace(nf);

      next.indices[0] = existing.indices[i];
      next.indices[1] = existing.indices[nf];
      next.indices[2] = static_cast<unsigned char>(newPointIndex);

      next.adjacentFaces[0] = existing.adjacentFaces[i];
      next.adjacentFaces[1] = triIndices[nf];
      next.adjacentFaces[2] = triIndices[nnf];

      next.oppositePoints[0] = existing.oppositePoints[i];
      next.oppositePoints[1] = 1;
      next.oppositePoints[2] = 0;

      auto &otherTri = es.triangles[existing.adjacentFaces[i]];
      unsigned char backRef = nextFace(existing.oppositePoints[i]);
      otherTri.indexData.adjacentFaces[backRef] = triIndices[i];
      otherTri.indexData.oppositePoints[backRef] = 2;

      triangleInit(es, next, es.triangles[triIndices[i]]);
    }

    for(int i = 0; i < 3; ++i) {
      unsigned char ti = triIndices[i];
      if(i != 0) {
        es.triangleHeap[ti] = ti;
        es.triangleToHeapIndex[ti] = ti;
        ++es.triangleCount;
      }
      checkRotate(es, ti, (i == 0) ? 0 : ti);
    }
  }

  // --- Contact calculation ---

  void calculateContact(ExpandingSimplex &es, SimplexTriangle &face, const fm_vec3_t &planePos, EpaResult &result) {
    auto bary = calculateBarycentricCoords(
      es.points[face.indexData.indices[0]],
      es.points[face.indexData.indices[1]],
      es.points[face.indexData.indices[2]],
      planePos
    );
    result.contactA = evaluateBarycentricCoords(
      es.aPoints[face.indexData.indices[0]],
      es.aPoints[face.indexData.indices[1]],
      es.aPoints[face.indexData.indices[2]],
      bary
    );
    result.contactB = result.contactA + result.normal * result.penetration;
  }

} // anonymous namespace


bool P64::Coll::epaSolve(
  Simplex &startingSimplex,
  const void *rigidBodyA, GjkSupportFunction rigidBodyASupport,
  const void *rigidBodyB, GjkSupportFunction rigidBodyBSupport,
  EpaResult &result
) {
  ExpandingSimplex es{};
  initExpandingSimplex(es, startingSimplex, FlagsNone);

  SimplexTriangle *closest = nullptr;
  float projection = 0.0f;
  float epaTolerance = EPA_CONVERGENCE_TOLERANCE;

  for(int i = 0; i < EPA_MAX_ITERATIONS; ++i) {
    closest = &closestFace(es);
    short nextIdx = es.pointCount;

    auto &aPoint = es.aPoints[nextIdx];
    fm_vec3_t bPoint{};

    rigidBodyASupport(rigidBodyA, closest->normal, aPoint);
    fm_vec3_t reverseNormal = -closest->normal;
    rigidBodyBSupport(rigidBodyB, reverseNormal, bPoint);

    es.points[nextIdx] = aPoint - bPoint;
    projection = fm_vec3_dot(&es.points[nextIdx], &closest->normal);

    if((projection - closest->distanceToOrigin) < epaTolerance) break;

    ++es.pointCount;
    expandPolytope(es, nextIdx, es.triangleHeap[0]);
  }

  if(closest) {
    result.normal = -closest->normal;
    result.penetration = projection;
    fm_vec3_t planePos = closest->normal * closest->distanceToOrigin;
    calculateContact(es, *closest, planePos, result);
    return true;
  }

  return false;
}
