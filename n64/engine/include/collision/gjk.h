/**
 * @file gjk.h
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Gilbert–Johnson–Keerthi distance algorithm (GJK) implementation for collision detection.
 * Efficiently determines if two convex shapes overlap or not by attempting to build a Simplex (Tetrahedron for 3D GJK)
 * that contains the origin.
 */
#pragma once

#include "vecMath.h"

namespace P64::Coll {

  /// GJK support function: returns the furthest point on a convex shape in a given direction
  using GjkSupportFunction = void (*)(const void *data, const fm_vec3_t &direction, fm_vec3_t &output);

  //Max simplex size in 3d is a tetrahedron
  constexpr int GJK_MAX_SIMPLEX_SIZE = 4;

  /// @brief Simplex (tetrahedron) used during GJK and as input to EPA
  struct Simplex {
    fm_vec3_t points[GJK_MAX_SIMPLEX_SIZE]{};
    fm_vec3_t rigidBodyAPoint[GJK_MAX_SIMPLEX_SIZE]{};
    short nPoints{0};
  };

  /// @brief Adds a new support point (Minkowski difference) to the simplex
  /// @param simplex 
  /// @param aPoint 
  /// @param bPoint 
  /// @return pointer to the new point in the simplex, or nullptr if full
  fm_vec3_t *simplexAddPoint(Simplex &simplex, const fm_vec3_t &aPoint, const fm_vec3_t &bPoint);


  /// @brief Checks whether the given simplex encloses the origin and updates the search direction
  /// @param simplex 
  /// @param nextDirection 
  /// @return 
  bool simplexCheck(Simplex &simplex, fm_vec3_t &nextDirection);


  /// @brief Performs GJK overlap test between two convex rigidBodys
  /// @param simplex pointer to the simplex structure to use
  /// @param colliderA first collider
  /// @param colliderASupport support function matching the first collider type
  /// @param colliderB second collider
  /// @param colliderBSupport support function matching the second collider type
  /// @param firstDirection initial direction to search for the origin (arbitrary)
  /// @return 
  bool gjkCheckForOverlap(
    Simplex &simplex,
    const void *colliderA, GjkSupportFunction colliderASupport,
    const void *colliderB, GjkSupportFunction colliderBSupport,
    const fm_vec3_t &firstDirection,
    fm_vec3_t *outSeparatingAxis = nullptr
  );

} // namespace P64::Coll
