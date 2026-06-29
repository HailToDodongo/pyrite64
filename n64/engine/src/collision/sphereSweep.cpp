/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "collision/sphereSweep.h"
#include "collision/meshCollider.h"
#include "collision/aabb.h"
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

static float sphereEdgeTest(
  const fm_vec3_t& S, float r, const fm_vec3_t& dir, float t_max,
  const fm_vec3_t& E0, const fm_vec3_t& E1,
  fm_vec3_t& outN, fm_vec3_t& outP, float& outDepth
) {
  fm_vec3_t edge   = E1 - E0;
  float     elen2  = fm_vec3_len2(&edge);
  if (elen2 < SWEEP_EPS * SWEEP_EPS) return std::numeric_limits<float>::max();
  float     elen   = sqrtf(elen2);
  fm_vec3_t u_hat  = edge / elen;

  fm_vec3_t ce     = S - E0;
  float     ce_u   = fm_vec3_dot(&ce, &u_hat);
  fm_vec3_t c0     = ce - u_hat * ce_u;          // perp component of (S-E0)
  float     d_u    = fm_vec3_dot(&dir, &u_hat);
  fm_vec3_t d_perp = dir - u_hat * d_u;          // perp component of dir

  float c0len2 = fm_vec3_len2(&c0);
  float c_val  = c0len2 - r * r;
  float a      = fm_vec3_len2(&d_perp);
  float b      = fm_vec3_dot(&d_perp, &c0);

  float t;
  if (c_val < 0.0f) {
    // already within r of the infinite edge line
    float proj = ce_u;
    if (proj < 0.0f || proj > elen) return std::numeric_limits<float>::max();
    t        = 0.0f;
    outDepth = r - sqrtf(c0len2);
  } else {
    if (a < SWEEP_EPS * SWEEP_EPS) return std::numeric_limits<float>::max();
    float disc = b * b - a * c_val;
    if (disc < 0.0f) return std::numeric_limits<float>::max();
    t = (-b - sqrtf(disc)) / a;
    if (t < 0.0f) return std::numeric_limits<float>::max();
    if (t > t_max) return std::numeric_limits<float>::max();
    outDepth = 0.0f;
  }

  fm_vec3_t C_t = S + dir * t;
  fm_vec3_t C_t_rel = C_t - E0;
  float proj = fm_vec3_dot(&C_t_rel, &u_hat);
  if (proj < 0.0f || proj > elen) return std::numeric_limits<float>::max();

  outP = E0 + u_hat * proj;
  outN = vec3NormalizeOrFallback(C_t - outP, -dir);
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

// ── Public: sphere vs. one triangle ─────────────────────────────────────────

bool sphereSweepTriangle(
  const fm_vec3_t& center,
  float radius,
  const fm_vec3_t& displacement, // world-space, not normalized
  const fm_vec3_t& v0, const fm_vec3_t& v1, const fm_vec3_t& v2,
  const fm_vec3_t& triNormal,
  SphereSweepHit& hit
) {
  float dist = sqrtf(fm_vec3_len2(&displacement));
  if (dist < SWEEP_EPS) return false;
  fm_vec3_t dir = displacement / dist;

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

  float t = sphereFaceTest(center, radius, dir, dist, v0, v1, v2, triNormal, n, p, depth);
  update(t, n, p, depth);

  for (int i = 0; i < 3; ++i) {
    t = sphereEdgeTest(center, radius, dir, dist, *verts[i], *verts[(i+1)%3], n, p, depth);
    update(t, n, p, depth);
  }

  for (const fm_vec3_t* V : verts) {
    t = sphereVertexTest(center, radius, dir, dist, *V, n, p, depth);
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
