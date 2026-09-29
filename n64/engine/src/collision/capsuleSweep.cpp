/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "collision/capsuleSweep.h"
#include "collision/vecMath.h"

#include <cmath>
#include <limits>
#include <algorithm>

namespace P64::Coll {

static constexpr float SWEEP_EPS  = 1e-6f;
static constexpr float PARAM_EPS  = 1e-4f; // inset to avoid double-counting end caps

// ── Point-in-triangle (P already projected onto face plane) ──────────────────

static bool pointInTriangle(
  const fm_vec3_t& P,
  const fm_vec3_t& v0, const fm_vec3_t& v1, const fm_vec3_t& v2,
  const fm_vec3_t& n
) {
  fm_vec3_t e0 = v1 - v0;
  fm_vec3_t e1 = v2 - v1;
  fm_vec3_t e2 = v0 - v2;
  fm_vec3_t p0 = P - v0;
  fm_vec3_t p1 = P - v1;
  fm_vec3_t p2 = P - v2;
  fm_vec3_t c0, c1, c2;
  fm_vec3_cross(&c0, &e0, &p0);
  fm_vec3_cross(&c1, &e1, &p1);
  fm_vec3_cross(&c2, &e2, &p2);
  return fm_vec3_dot(&c0, &n) >= 0.0f &&
         fm_vec3_dot(&c1, &n) >= 0.0f &&
         fm_vec3_dot(&c2, &n) >= 0.0f;
}

// ── Sphere sub-tests ──────────────────────────────────────────────────────────

// Sphere at S (radius r) sweeping along dir for up to t_max distance.
// Returns t ∈ [0, t_max] or FLT_MAX. depth is positive only when t == 0.
static float sphereFaceTest(
  const fm_vec3_t& S, float r, const fm_vec3_t& dir, float t_max,
  const fm_vec3_t& v0, const fm_vec3_t& v1, const fm_vec3_t& v2,
  const fm_vec3_t& triN,
  fm_vec3_t& outN, fm_vec3_t& outP, float& outDepth
) {
  float face_d   = fm_vec3_dot(&triN, &v0);
  float sdist    = fm_vec3_dot(&triN, &S) - face_d; // positive = in front

  if (sdist < -r) return std::numeric_limits<float>::max(); // fully behind

  float t;
  if (sdist >= r) {
    float ndotd = fm_vec3_dot(&triN, &dir);
    if (ndotd >= -SWEEP_EPS) return std::numeric_limits<float>::max();
    t = (sdist - r) / (-ndotd);
    if (t > t_max) return std::numeric_limits<float>::max();
    outDepth = 0.0f;
  } else {
    t        = 0.0f;
    outDepth = r - sdist;
  }

  fm_vec3_t C_t      = S + dir * t;
  fm_vec3_t hitPlane = C_t - triN * r;
  if (!pointInTriangle(hitPlane, v0, v1, v2, triN))
    return std::numeric_limits<float>::max();

  outN = triN;
  outP = hitPlane;
  return t;
}

static float sphereVertexTest(
  const fm_vec3_t& S, float r, const fm_vec3_t& dir, float t_max,
  const fm_vec3_t& V,
  fm_vec3_t& outN, fm_vec3_t& outP, float& outDepth
) {
  fm_vec3_t w    = S - V;
  float     d2   = fm_vec3_len2(&w);
  float     t;

  if (d2 < r * r) {
    t        = 0.0f;
    outDepth = r - sqrtf(d2);
  } else {
    float b    = fm_vec3_dot(&w, &dir);
    float disc = b * b - (d2 - r * r); // a == 1
    if (disc < 0.0f) return std::numeric_limits<float>::max();
    t = -b - sqrtf(disc);
    if (t < 0.0f) return std::numeric_limits<float>::max();
    if (t > t_max) return std::numeric_limits<float>::max();
    outDepth = 0.0f;
  }

  fm_vec3_t C_t = S + dir * t;
  outP = V;
  outN = vec3NormalizeOrFallback(C_t - V, -dir);
  return t;
}

// ── Cylinder body sub-tests ───────────────────────────────────────────────────
// These only fire when the contact is strictly interior to the capsule segment
// (not at either end cap), so they do not overlap with the sphere tests.

// ── Public: capsule vs. one triangle ─────────────────────────────────────────

bool capsuleSweepTriangle(
  const fm_vec3_t& center,
  const fm_vec3_t& axisUp,       // normalized capsule-axis direction
  float radius,
  float innerHalfHeight,
  const fm_vec3_t& displacement, // world-space, not normalized
  const fm_vec3_t& v0, const fm_vec3_t& v1, const fm_vec3_t& v2,
  const fm_vec3_t& triNormal,
  CapsuleSweepHit& hit
) {
  const float dist = sqrtf(fm_vec3_len2(&displacement));
  if (dist < SWEEP_EPS) return false;
  const fm_vec3_t dir = displacement / dist;
  const fm_vec3_t P1 = center - axisUp * innerHalfHeight;
  const fm_vec3_t P2 = center + axisUp * innerHalfHeight;

  // Reuse the capsule-axis projections across all cylinder sub-tests.
  const fm_vec3_t axis = P2 - P1;
  const float axisLength2 = fm_vec3_len2(&axis);
  float axisLength = 0.0f, dirPerpLength2 = 0.0f;
  fm_vec3_t axisUnit{}, dirPerp{};
  if (axisLength2 >= SWEEP_EPS * SWEEP_EPS) {
    axisLength = sqrtf(axisLength2);
    axisUnit = axis / axisLength;
    dirPerp = dir - axisUnit * fm_vec3_dot(&dir, &axisUnit);
    dirPerpLength2 = fm_vec3_len2(&dirPerp);
  }

  float     bestT     = std::numeric_limits<float>::max();
  float     bestDepth = 0.0f;
  fm_vec3_t bestN{}, bestP{};

  auto update = [&](float t, const fm_vec3_t& n, const fm_vec3_t& p, float depth) {
    if (t > dist + SWEEP_EPS) return;
    // Among t==0 hits keep the deepest; otherwise keep the earliest
    if (t < bestT || (t == 0.0f && depth > bestDepth)) {
      bestT     = t;
      bestN     = n;
      bestP     = p;
      bestDepth = depth;
    }
  };

  const fm_vec3_t* verts[3] = {&v0, &v1, &v2};
  fm_vec3_t n{}, p{};
  float depth = 0.0f;

  // Both sphere caps use the same triangle-edge projections.
  fm_vec3_t edgeUnits[3]{}, edgeDirPerps[3]{};
  float edgeLengths[3]{}, edgeDirPerpLength2[3]{};
  for (int i = 0; i < 3; ++i) {
    const fm_vec3_t edge = *verts[(i + 1) % 3] - *verts[i];
    const float length2 = fm_vec3_len2(&edge);
    if (length2 < SWEEP_EPS * SWEEP_EPS) continue;
    edgeLengths[i] = sqrtf(length2);
    edgeUnits[i] = edge / edgeLengths[i];
    edgeDirPerps[i] = dir - edgeUnits[i] * fm_vec3_dot(&dir, &edgeUnits[i]);
    edgeDirPerpLength2[i] = fm_vec3_len2(&edgeDirPerps[i]);
  }

  auto sphereEdgeTest = [&](const fm_vec3_t &S, int i,
    fm_vec3_t &outN, fm_vec3_t &outP, float &outDepth) -> float {
    const fm_vec3_t &E0 = *verts[i];
    if (edgeLengths[i] == 0.0f) return std::numeric_limits<float>::max();
    float     elen   = edgeLengths[i];
    const fm_vec3_t &u_hat = edgeUnits[i];

    fm_vec3_t ce     = S - E0;
    float     ce_u   = fm_vec3_dot(&ce, &u_hat);
    fm_vec3_t c0     = ce - u_hat * ce_u;          // perp component of (S-E0)
    const fm_vec3_t &d_perp = edgeDirPerps[i];

    float c0len2 = fm_vec3_len2(&c0);
    float c_val  = c0len2 - radius * radius;
    float a      = edgeDirPerpLength2[i];
    float b      = fm_vec3_dot(&d_perp, &c0);

    float t;
    if (c_val < 0.0f) {
      // already within radius of the infinite edge line
      float proj = ce_u;
      if (proj < 0.0f || proj > elen) return std::numeric_limits<float>::max();
      t        = 0.0f;
      outDepth = radius - sqrtf(c0len2);
    } else {
      if (a < SWEEP_EPS * SWEEP_EPS) return std::numeric_limits<float>::max();
      float disc = b * b - a * c_val;
      if (disc < 0.0f) return std::numeric_limits<float>::max();
      t = (-b - sqrtf(disc)) / a;
      if (t < 0.0f) return std::numeric_limits<float>::max();
      if (t > dist) return std::numeric_limits<float>::max();
      outDepth = 0.0f;
    }

    fm_vec3_t C_t = S + dir * t;
    fm_vec3_t C_t_rel = C_t - E0;
    float proj = fm_vec3_dot(&C_t_rel, &u_hat);
    if (proj < 0.0f || proj > elen) return std::numeric_limits<float>::max();

    outP = E0 + u_hat * proj;
    outN = vec3NormalizeOrFallback(C_t - outP, -dir);
    return t;
  };

  auto cylinderEdgeTest = [&](const fm_vec3_t &E0, const fm_vec3_t &E1,
    fm_vec3_t &outN, fm_vec3_t &outP, float &outDepth) -> float {
    fm_vec3_t d2 = E1 - E0; // triangle edge

    float a     = axisLength2;
    float e_val = fm_vec3_len2(&d2);
    float b     = fm_vec3_dot(&axis, &d2);
    float denom = a * e_val - b * b;

    if (denom < SWEEP_EPS * SWEEP_EPS) return std::numeric_limits<float>::max();

    fm_vec3_t n_cross;
    fm_vec3_cross(&n_cross, &axis, &d2);
    float n_len = sqrtf(denom);
    fm_vec3_t n_hat = n_cross / n_len;

    fm_vec3_t r0    = P1 - E0;
    float r0_dot    = fm_vec3_dot(&r0, &n_hat);
    float vel_dot   = fm_vec3_dot(&dir, &n_hat); // dir is unit length

    // Helper: verify both segment parameters are interior at a given t
    auto resolve = [&](float t_phys) -> bool {
      fm_vec3_t r0_t  = r0 + dir * t_phys;
      float c_t       = fm_vec3_dot(&axis, &r0_t);
      float f_t       = fm_vec3_dot(&d2, &r0_t);
      float s_star    = (b * f_t - c_t * e_val) / denom; // [0,1] for interior
      float u_star    = (a * f_t - b * c_t) / denom;      // [0,1] for interior
      if (s_star <= PARAM_EPS || s_star >= 1.0f - PARAM_EPS) return false;
      if (u_star < 0.0f || u_star > 1.0f) return false;
      outP = E0 + d2 * u_star;
      outN = (r0_dot >= 0.0f) ? n_hat : -n_hat;
      return true;
    };

    if (fabsf(r0_dot) < radius) {
      if (!resolve(0.0f)) return std::numeric_limits<float>::max();
      outDepth = radius - fabsf(r0_dot);
      return 0.0f;
    }

    if (fabsf(vel_dot) < SWEEP_EPS) return std::numeric_limits<float>::max();

    float sign  = (r0_dot > 0.0f) ? 1.0f : -1.0f;
    float t     = (sign * radius - r0_dot) / vel_dot;
    if (t < 0.0f || t > dist) return std::numeric_limits<float>::max();
    if (!resolve(t)) return std::numeric_limits<float>::max();
    outDepth = 0.0f;
    return t;
  };

  auto cylinderVertexTest = [&](const fm_vec3_t &V,
    fm_vec3_t &outN, fm_vec3_t &outP, float &outDepth) -> float {
    if (axisLength2 < SWEEP_EPS * SWEEP_EPS) return std::numeric_limits<float>::max();

    // Decompose relative to capsule axis
    fm_vec3_t w        = V - P1; // from P1 to vertex
    float w_along      = fm_vec3_dot(&w, &axisUnit);
    fm_vec3_t w_perp   = w - axisUnit * w_along;

    float A      = dirPerpLength2;
    float wperp2 = fm_vec3_len2(&w_perp);

    float t;
    if (wperp2 < radius * radius) {
      // already within radius of the capsule axis line
      t = 0.0f;
      outDepth = radius - sqrtf(wperp2);
    } else {
      if (A < SWEEP_EPS * SWEEP_EPS) return std::numeric_limits<float>::max();
      // dist_perp²(t) = |w_perp - t*dirPerp|²
      float B    = fm_vec3_dot(&w_perp, &dirPerp);
      float C    = wperp2 - radius * radius;
      float disc = B * B - A * C;
      if (disc < 0.0f) return std::numeric_limits<float>::max();
      t = (B - sqrtf(disc)) / A; // smallest root
      if (t < 0.0f) t = (B + sqrtf(disc)) / A;
      if (t < 0.0f || t > dist) return std::numeric_limits<float>::max();
      outDepth = 0.0f;
    }

    // Verify s is strictly interior (not at end caps)
    fm_vec3_t w_t = w - dir * t; // V - P1(t)
    float s       = fm_vec3_dot(&w_t, &axisUnit);
    if (s <= PARAM_EPS || s >= axisLength - PARAM_EPS)
      return std::numeric_limits<float>::max();

    fm_vec3_t axis_pt = P1 + dir * t + axisUnit * s;
    outP = V;
    outN = vec3NormalizeOrFallback(axis_pt - V, -dir);
    return t;
  };

  for (const fm_vec3_t* S : {&P1, &P2}) {
    float t = sphereFaceTest(*S, radius, dir, dist, v0, v1, v2, triNormal, n, p, depth);
    update(t, n, p, depth);

    for (int i = 0; i < 3; ++i) {
      t = sphereEdgeTest(*S, i, n, p, depth);
      update(t, n, p, depth);
    }

    for (const fm_vec3_t* V : verts) {
      t = sphereVertexTest(*S, radius, dir, dist, *V, n, p, depth);
      update(t, n, p, depth);
    }
  }

  for (int i = 0; i < 3; ++i) {
    float t = cylinderEdgeTest(*verts[i], *verts[(i+1)%3], n, p, depth);
    update(t, n, p, depth);
  }

  for (const fm_vec3_t* V : verts) {
    float t = cylinderVertexTest(*V, n, p, depth);
    update(t, n, p, depth);
  }

  if (bestT > dist + SWEEP_EPS) return false;

  hit.t      = fmaxf(0.0f, fminf(1.0f, bestT / dist));
  hit.depth  = bestDepth;
  hit.normal = vec3NormalizeOrFallback(bestN, -dir);
  hit.point  = bestP;
  hit.didHit = true;
  return true;
}

} // namespace P64::Coll
