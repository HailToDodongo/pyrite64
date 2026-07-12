/**
 * @file boxBox.h
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Analytical SAT-based Box vs Box collision with one-shot contact manifold generation.
 *
 * Replaces the iterative GJK+EPA path for box-box pairs. A single call produces the
 * full contact manifold (up to 4 points) instead of one point per frame, which keeps
 * warm-started impulses valid across frames and lets stacks come to rest faster
 */
#pragma once

#include "vecMath.h"
#include "matrix3x3.h"
#include "contact.h"
#include "epa.h"

namespace P64::Coll {

  // Maximum number of contact points analyticalBoxBoxManifold can generate.
  constexpr int BOX_BOX_MAX_CONTACTS = 4;

  // Speculative contact margin: clipped manifold points separated by up to this
  // distance are kept instead of being dropped. Without this, micro-tilts of a resting box collapse the manifold
  // from 4 points to 1, which restarts impulse accumulation every frame and makes
  // stacks rock and fall over. Points inside the margin are treated as speculative
  constexpr float BOX_BOX_SPECULATIVE_MARGIN = CONTACT_BREAKING_SEPARATION;

  /// @brief Oriented box input for the SAT box-box test, decoupled from Collider
  struct SatObb {
    fm_vec3_t center{};   ///< world-space center
    Matrix3x3 rotation{}; ///< local-to-world rotation (column i = world direction of local axis i)
    fm_vec3_t halfSize{}; ///< half extents along the local axes
  };

  /// @brief Analytical SAT box-box test with one-shot manifold generation.
  ///
  /// Tests the 15 separating axes (6 face normals + 9 edge cross products). On overlap
  /// the minimum-penetration axis is selected with a bias that prefers face contacts
  /// over edge contacts (prevents the reference axis from flickering between nearly
  /// equal face/edge depths across frames). Face contacts clip the incident face
  /// against the reference face's side planes (Sutherland-Hodgman) and reduce the
  /// clipped polygon to at most `maxResults` well-spread points; edge contacts produce
  /// the single closest point between the two supporting edges.
  ///
  /// Results follow the EpaResult convention: normal points from B to A, contactA lies
  /// on A's surface, contactB on B's surface, penetration >= 0.
  ///
  /// @param a          first box
  /// @param b          second box
  /// @param results    array to receive contact data
  /// @param maxResults maximum number of contact points to generate. Pass 0 for a
  ///                   boolean overlap query (returns 1 on overlap, 0 otherwise).
  /// @return Number of contact points generated (0 = no collision).
  int analyticalBoxBoxManifold(const SatObb &a, const SatObb &b, EpaResult *results, int maxResults);

} // namespace P64::Coll
