/**
 * @file boxBox.cpp
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Analytical SAT Box vs Box collision with one-shot contact manifold generation (see boxBox.h)
 */
#include "collision/boxBox.h"
#include "collision/matrix3x3.h"

#include <cmath>

namespace P64::Coll {

  // Added to |R| entries so near-parallel edges can't produce a false separating axis
  // from rounding error (ODE/Bullet do the same).
  constexpr float ABS_R_EPS = 1e-5f;
  // Edge cross products shorter than this come from parallel edges; the face axes cover them.
  constexpr float EDGE_AXIS_MIN_LEN2 = 1e-6f;
  // An edge axis must beat the best face axis by this relative factor to win, so the
  // reference axis doesn't flicker between near-equal face/edge depths (breaks warm starting).
  constexpr float FACE_PREFERENCE_FUDGE = 1.05f;
  // ...plus this absolute margin. Near rest the depths are tiny, so a relative fudge alone
  // can't stop an edge axis parallel to the face normal (criss-cross stacks) from stealing
  // the contact on a micro-tilt and collapsing the manifold to one point.
  constexpr float EDGE_PREFERENCE_ABS_TOL = 0.005f;
  // Clipped points within this separation still enter the manifold, so a resting stack
  // keeps its full patch under micro-tilts.
  constexpr float CLIP_KEEP_SEPARATION = BOX_BOX_SPECULATIVE_MARGIN;

  static constexpr int NEXT_AXIS[3] = {1, 2, 0};

  static inline fm_vec3_t matrixColumn(const Matrix3x3 &m, int col) {
    return fm_vec3_t{{m.m[0][col], m.m[1][col], m.m[2][col]}};
  }

  // Clip a convex polygon against one slab side, keeping vertices with v[axis] >= -limit.
  // outVerts needs room for inCount + 1 vertices.
  static int clipPolygonToSlabSide(const fm_vec3_t *inVerts, int inCount,
                                   fm_vec3_t *outVerts, int axis, float limit) {
    int outCount = 0;
    for(int i = 0; i < inCount; ++i) {
      const fm_vec3_t &cur = inVerts[i];
      const fm_vec3_t &nxt = inVerts[(i + 1) % inCount];
      const float cVal = cur.v[axis];
      const float nVal = nxt.v[axis];
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

  // Closest points between two segments, each given by center, unit direction and half length.
  static void closestPointsOnEdges(
    const fm_vec3_t &centerA, const fm_vec3_t &dirA, float halfLenA,
    const fm_vec3_t &centerB, const fm_vec3_t &dirB, float halfLenB,
    fm_vec3_t &outPointA, fm_vec3_t &outPointB) {
    const fm_vec3_t r = centerB - centerA;
    const float b = fm_vec3_dot(&dirA, &dirB);
    const float c = fm_vec3_dot(&dirA, &r);
    const float f = fm_vec3_dot(&dirB, &r);

    const float denom = 1.0f - b * b;
    float sA = (denom > FM_EPSILON) ? (c - b * f) / denom : 0.0f;
    sA = fminf(fmaxf(sA, -halfLenA), halfLenA);
    float sB = b * sA - f;
    sB = fminf(fmaxf(sB, -halfLenB), halfLenB);
    sA = fminf(fmaxf(c + b * sB, -halfLenA), halfLenA);

    outPointA = centerA + dirA * sA;
    outPointB = centerB + dirB * sB;
  }

  // SAT runs in A's local frame using the relative rotation R (Gottschalk's OBB test,
  // Real-Time Collision Detection 4.4.1). Face axes are unit length so the face path
  // needs no sqrt; the single edge normalization is deferred until an edge axis wins.
  int analyticalBoxBoxManifold(const SatObb &a, const SatObb &b, EpaResult *results, int maxResults) {
    const float *ha = a.halfSize.v;
    const float *hb = b.halfSize.v;

    // Relative rotation R = A^T * B (maps B local -> A local); R[i][j] = dot(axisA_i, axisB_j)
    float R[3][3];
    float AbsR[3][3];
    for(int i = 0; i < 3; ++i) {
      for(int j = 0; j < 3; ++j) {
        R[i][j] = a.rotation.m[0][i] * b.rotation.m[0][j] +
                  a.rotation.m[1][i] * b.rotation.m[1][j] +
                  a.rotation.m[2][i] * b.rotation.m[2][j];
        AbsR[i][j] = fabsf(R[i][j]) + ABS_R_EPS;
      }
    }

    // B's center relative to A's center, in A's local frame
    const fm_vec3_t d = b.center - a.center;
    float t[3];
    for(int i = 0; i < 3; ++i) {
      t[i] = a.rotation.m[0][i] * d.x + a.rotation.m[1][i] * d.y + a.rotation.m[2][i] * d.z;
    }

    float bestFaceDepth = 1e30f;
    int bestFaceAxis = 0;
    float bestFaceSign = 1.0f;
    bool faceOfA = true;

    // --- 3 face normals of A ---
    for(int i = 0; i < 3; ++i) {
      const float rb = AbsR[i][0] * hb[0] + AbsR[i][1] * hb[1] + AbsR[i][2] * hb[2];
      const float depth = ha[i] + rb - fabsf(t[i]);
      if(depth < 0.0f) return 0;
      if(depth < bestFaceDepth) {
        bestFaceDepth = depth;
        bestFaceAxis = i;
        bestFaceSign = (t[i] >= 0.0f) ? 1.0f : -1.0f;
        faceOfA = true;
      }
    }

    // --- 3 face normals of B ---
    for(int j = 0; j < 3; ++j) {
      const float tB = t[0] * R[0][j] + t[1] * R[1][j] + t[2] * R[2][j];
      const float ra = AbsR[0][j] * ha[0] + AbsR[1][j] * ha[1] + AbsR[2][j] * ha[2];
      const float depth = ra + hb[j] - fabsf(tB);
      if(depth < 0.0f) return 0;
      if(depth < bestFaceDepth) {
        bestFaceDepth = depth;
        bestFaceAxis = j;
        // face normal must point from the reference box (B) toward the other box (A)
        bestFaceSign = (tB >= 0.0f) ? -1.0f : 1.0f;
        faceOfA = false;
      }
    }

    // --- 9 edge cross products axisA_i x axisB_j ---
    // Depths are measured on the unnormalized axis; comparisons use the
    // cross-multiplied form to defer the sqrt until a winner is known.
    bool edgeFound = false;
    float bestEdgeDepthUnnorm = 0.0f;
    float bestEdgeLen2 = 1.0f;
    float bestEdgeSign = 1.0f;
    int bestEdgeI = 0;
    int bestEdgeJ = 0;

    for(int i = 0; i < 3; ++i) {
      const int i1 = NEXT_AXIS[i];
      const int i2 = NEXT_AXIS[i1];
      for(int j = 0; j < 3; ++j) {
        const int j1 = NEXT_AXIS[j];
        const int j2 = NEXT_AXIS[j1];

        const float len2 = R[i1][j] * R[i1][j] + R[i2][j] * R[i2][j];
        if(len2 < EDGE_AXIS_MIN_LEN2) continue; // parallel edges

        const float ra = ha[i1] * AbsR[i2][j] + ha[i2] * AbsR[i1][j];
        const float rb = hb[j1] * AbsR[i][j2] + hb[j2] * AbsR[i][j1];
        const float proj = t[i2] * R[i1][j] - t[i1] * R[i2][j];
        const float depth = ra + rb - fabsf(proj);
        if(depth < 0.0f) return 0;

        if(!edgeFound ||
           depth * depth * bestEdgeLen2 < bestEdgeDepthUnnorm * bestEdgeDepthUnnorm * len2) {
          edgeFound = true;
          bestEdgeDepthUnnorm = depth;
          bestEdgeLen2 = len2;
          bestEdgeSign = (proj >= 0.0f) ? 1.0f : -1.0f;
          bestEdgeI = i;
          bestEdgeJ = j;
        }
      }
    }

    // Overlap confirmed past this point
    if(maxResults <= 0 || !results) return 1; // boolean overlap query

    bool useEdge = false;
    float edgeDepth = 0.0f;
    float edgeInvLen = 0.0f;
    if(edgeFound) {
      edgeInvLen = 1.0f / sqrtf(bestEdgeLen2);
      edgeDepth = bestEdgeDepthUnnorm * edgeInvLen;
      useEdge = edgeDepth * FACE_PREFERENCE_FUDGE + EDGE_PREFERENCE_ABS_TOL < bestFaceDepth;
    }

    // --- Edge contact: single point between the two supporting edges ---
    if(useEdge) {
      const int i = bestEdgeI;
      const int i1 = NEXT_AXIS[i];
      const int i2 = NEXT_AXIS[i1];
      const int j = bestEdgeJ;
      const int j1 = NEXT_AXIS[j];
      const int j2 = NEXT_AXIS[j1];

      // Contact axis in A local space (unit, pointing from A toward B):
      // axisA_i x colB_j has components 0 / -R[i2][j] / R[i1][j] on axes i / i1 / i2.
      fm_vec3_t axisLocalA = VEC3_ZERO;
      axisLocalA.v[i1] = -R[i2][j] * (bestEdgeSign * edgeInvLen);
      axisLocalA.v[i2] = R[i1][j] * (bestEdgeSign * edgeInvLen);

      // Supporting edge on A: the edge along axis i on the side facing B
      fm_vec3_t edgeCenterALocal = VEC3_ZERO;
      edgeCenterALocal.v[i1] = (axisLocalA.v[i1] >= 0.0f) ? ha[i1] : -ha[i1];
      edgeCenterALocal.v[i2] = (axisLocalA.v[i2] >= 0.0f) ? ha[i2] : -ha[i2];

      // Same axis in B local space, pointing from B toward A: -(R^T * axisLocalA)
      fm_vec3_t axisLocalB;
      for(int m = 0; m < 3; ++m) {
        axisLocalB.v[m] = -(R[0][m] * axisLocalA.v[0] + R[1][m] * axisLocalA.v[1] + R[2][m] * axisLocalA.v[2]);
      }
      fm_vec3_t edgeCenterBLocal = VEC3_ZERO;
      edgeCenterBLocal.v[j1] = (axisLocalB.v[j1] >= 0.0f) ? hb[j1] : -hb[j1];
      edgeCenterBLocal.v[j2] = (axisLocalB.v[j2] >= 0.0f) ? hb[j2] : -hb[j2];

      const fm_vec3_t edgeCenterA = a.center + matrix3Vec3Mul(a.rotation, edgeCenterALocal);
      const fm_vec3_t edgeCenterB = b.center + matrix3Vec3Mul(b.rotation, edgeCenterBLocal);
      const fm_vec3_t edgeDirA = matrixColumn(a.rotation, i);
      const fm_vec3_t edgeDirB = matrixColumn(b.rotation, j);

      fm_vec3_t pointOnA, pointOnB;
      closestPointsOnEdges(edgeCenterA, edgeDirA, ha[i], edgeCenterB, edgeDirB, hb[j], pointOnA, pointOnB);

      results[0].normal = -matrix3Vec3Mul(a.rotation, axisLocalA); // B -> A
      results[0].penetration = edgeDepth;
      results[0].contactA = pointOnA;
      results[0].contactB = pointOnB;
      return 1;
    }

    // --- Face contact: clip the incident face against the reference face's side planes ---
    const SatObb &ref = faceOfA ? a : b;
    const SatObb &inc = faceOfA ? b : a;
    const float *hRef = ref.halfSize.v;
    const float *hInc = inc.halfSize.v;
    const int fi = bestFaceAxis;
    const float fSign = bestFaceSign; // reference face normal (ref local) = fSign * e_fi, points toward incident box

    // Relative transform: incident local -> reference local
    float relR[3][3];
    float relT[3];
    if(faceOfA) {
      for(int i = 0; i < 3; ++i) {
        for(int j = 0; j < 3; ++j) relR[i][j] = R[i][j];
        relT[i] = t[i];
      }
    } else {
      for(int i = 0; i < 3; ++i) {
        for(int j = 0; j < 3; ++j) relR[i][j] = R[j][i];
        relT[i] = -(R[0][i] * t[0] + R[1][i] * t[1] + R[2][i] * t[2]);
      }
    }

    // Incident face: the face of inc whose normal is most anti-parallel to the reference normal.
    // Incident axis j direction in ref local space = column j of relR.
    int ji = 0;
    float bestAlign = fabsf(relR[fi][0]);
    for(int j = 1; j < 3; ++j) {
      const float align = fabsf(relR[fi][j]);
      if(align > bestAlign) {
        bestAlign = align;
        ji = j;
      }
    }
    const float incSign = (fSign * relR[fi][ji] >= 0.0f) ? -1.0f : 1.0f;
    const int ju = NEXT_AXIS[ji];
    const int jv = NEXT_AXIS[ju];

    // 4 corners of the incident face (inc local), cyclic winding, then into ref local space
    static constexpr float CORNER_SIGNS[4][2] = {{1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
    fm_vec3_t poly[10];
    fm_vec3_t polyTmp[10];
    for(int k = 0; k < 4; ++k) {
      fm_vec3_t corner;
      corner.v[ji] = incSign * hInc[ji];
      corner.v[ju] = CORNER_SIGNS[k][0] * hInc[ju];
      corner.v[jv] = CORNER_SIGNS[k][1] * hInc[jv];
      for(int m = 0; m < 3; ++m) {
        poly[k].v[m] = relT[m] + relR[m][0] * corner.v[0] + relR[m][1] * corner.v[1] + relR[m][2] * corner.v[2];
      }
    }
    int polyCount = 4;

    // Clip against the 4 side planes of the reference face (slabs on the two non-face axes)
    const int u1 = NEXT_AXIS[fi];
    const int u2 = NEXT_AXIS[u1];
    for(int step = 0; step < 2; ++step) {
      const int axis = (step == 0) ? u1 : u2;
      const float limit = hRef[axis];
      polyCount = clipPolygonToSlabSide(poly, polyCount, polyTmp, axis, limit);
      if(polyCount < 1) break;
      // clip against +limit by mirroring the axis, clipping, mirroring back
      for(int k = 0; k < polyCount; ++k) polyTmp[k].v[axis] = -polyTmp[k].v[axis];
      polyCount = clipPolygonToSlabSide(polyTmp, polyCount, poly, axis, limit);
      if(polyCount < 1) break;
      for(int k = 0; k < polyCount; ++k) poly[k].v[axis] = -poly[k].v[axis];
    }

    // Collect penetrating (and barely grazing) points
    fm_vec3_t candPoints[10];
    float candSeps[10];
    int candCount = 0;
    for(int k = 0; k < polyCount; ++k) {
      const float sep = fSign * poly[k].v[fi] - hRef[fi]; // <= 0 when penetrating
      if(sep <= CLIP_KEEP_SEPARATION) {
        candPoints[candCount] = poly[k];
        candSeps[candCount] = sep;
        ++candCount;
      }
    }

    // Numerical fallback: SAT reported overlap, so force at least one contact point
    if(candCount == 0) {
      if(polyCount > 0) {
        int minIdx = 0;
        for(int k = 1; k < polyCount; ++k) {
          if(poly[k].v[fi] * fSign < poly[minIdx].v[fi] * fSign) minIdx = k;
        }
        candPoints[0] = poly[minIdx];
        candSeps[0] = fSign * poly[minIdx].v[fi] - hRef[fi];
      } else {
        // Clipped away entirely (degenerate edge-on case): incident face center
        // clamped into the reference slab
        fm_vec3_t p;
        for(int m = 0; m < 3; ++m) {
          p.v[m] = relT[m] + relR[m][ji] * (incSign * hInc[ji]);
        }
        p.v[u1] = fminf(fmaxf(p.v[u1], -hRef[u1]), hRef[u1]);
        p.v[u2] = fminf(fmaxf(p.v[u2], -hRef[u2]), hRef[u2]);
        candPoints[0] = p;
        candSeps[0] = fSign * p.v[fi] - hRef[fi];
      }
      candCount = 1;
    }

    // Reduce to at most maxKeep well-spread points: deepest point, farthest point from
    // it, then the extreme points on both sides of that segment (max/min signed area).
    const int maxKeep = (maxResults < BOX_BOX_MAX_CONTACTS) ? maxResults : BOX_BOX_MAX_CONTACTS;
    int selected[BOX_BOX_MAX_CONTACTS];
    int selectedCount = 0;

    if(candCount <= maxKeep) {
      for(int k = 0; k < candCount; ++k) selected[selectedCount++] = k;
    } else {
      int deepest = 0;
      for(int k = 1; k < candCount; ++k) {
        if(candSeps[k] < candSeps[deepest]) deepest = k;
      }
      selected[selectedCount++] = deepest;

      if(maxKeep > 1) {
        int farthest = -1;
        float bestDist2 = -1.0f;
        for(int k = 0; k < candCount; ++k) {
          if(k == deepest) continue;
          const float du = candPoints[k].v[u1] - candPoints[deepest].v[u1];
          const float dv = candPoints[k].v[u2] - candPoints[deepest].v[u2];
          const float dist2 = du * du + dv * dv;
          if(dist2 > bestDist2) {
            bestDist2 = dist2;
            farthest = k;
          }
        }
        if(farthest >= 0) selected[selectedCount++] = farthest;

        if(maxKeep > 2 && farthest >= 0) {
          // signed areas relative to the deepest->farthest segment (in the face plane)
          const float eu = candPoints[farthest].v[u1] - candPoints[deepest].v[u1];
          const float ev = candPoints[farthest].v[u2] - candPoints[deepest].v[u2];
          int maxSide = -1, minSide = -1;
          float maxArea = 0.0f, minArea = 0.0f;
          for(int k = 0; k < candCount; ++k) {
            if(k == deepest || k == farthest) continue;
            const float pu = candPoints[k].v[u1] - candPoints[deepest].v[u1];
            const float pv = candPoints[k].v[u2] - candPoints[deepest].v[u2];
            const float area = eu * pv - ev * pu;
            if(area > maxArea) { maxArea = area; maxSide = k; }
            if(area < minArea) { minArea = area; minSide = k; }
          }
          if(maxKeep >= 4) {
            if(maxSide >= 0) selected[selectedCount++] = maxSide;
            if(minSide >= 0) selected[selectedCount++] = minSide;
          } else {
            // room for one more: take the side with the larger spread
            const int pick = (maxArea >= -minArea) ? maxSide : minSide;
            if(pick >= 0) selected[selectedCount++] = pick;
          }
        }
      }
    }

    // --- Emit results in world space, EpaResult convention (normal B -> A) ---
    const fm_vec3_t refNormalWorld = matrixColumn(ref.rotation, fi) * fSign;
    const fm_vec3_t worldNormal = faceOfA ? -refNormalWorld : refNormalWorld;

    int resultCount = 0;
    for(int k = 0; k < selectedCount && resultCount < maxKeep; ++k) {
      const fm_vec3_t &p = candPoints[selected[k]];
      const float sep = candSeps[selected[k]];

      // Incident point stays on the incident box surface; the reference-side point
      // is its projection onto the reference face plane
      fm_vec3_t pOnFace = p;
      pOnFace.v[fi] = fSign * hRef[fi];

      const fm_vec3_t worldInc = ref.center + matrix3Vec3Mul(ref.rotation, p);
      const fm_vec3_t worldRef = ref.center + matrix3Vec3Mul(ref.rotation, pOnFace);

      EpaResult &out = results[resultCount++];
      out.normal = worldNormal;
      out.penetration = -sep;
      if(faceOfA) {
        out.contactA = worldRef;
        out.contactB = worldInc;
      } else {
        out.contactA = worldInc;
        out.contactB = worldRef;
      }
    }

    return resultCount;
  }

} // namespace P64::Coll
