/**
 * @file epa.h
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Expanding Polytope Algorithm (EPA) implementation for collision detection.
 *
 * EPA refines the penetration information after GJK determines that two convex shapes intersect.
 * GJK efficiently detects collision but only provides a simplex containing the origin.
 * EPA expands this simplex into a polytope to find:
 *   - The exact penetration depth
 *   - The collision normal
 *   - Contact points on both objects
 *
 * The algorithm works by iteratively expanding the polytope toward the origin, finding the
 * face closest to the origin. This closest face defines the minimum translation vector needed
 * to separate the objects.
 */
#pragma once

#include "gjk.h"

namespace P64::Coll {

  /// Result of EPA solving: contact information for overlapping rigidBodys
  struct EpaResult {
    fm_vec3_t contactA{}; ///< Point on A's surface furthest inside B
    fm_vec3_t contactB{}; ///< Point on B's surface furthest inside A
    fm_vec3_t normal{};   ///< Contact normal pointing from B to A
    float penetration{0.0f}; ///< Overlap depth
  };

  /// Solves EPA to find penetration depth and contact information for overlapping rigidBodys
  bool epaSolve(
    Simplex &startingSimplex,
    const void *rigidBodyA, GjkSupportFunction rigidBodyASupport,
    const void *rigidBodyB, GjkSupportFunction rigidBodyBSupport,
    EpaResult &result
  );

} // namespace P64::Coll
