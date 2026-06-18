/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include "meshCollider.h"

namespace P64::Coll
{
  /**
   * Helper to attach something to a transforming mesh collider.
   * This will track the relative movement at given point.
   * Which can be later applied to an object in order to move it with the mesh.
   */
  class Attach
  {
    private:
      fm_vec3_t refPos{};
      fm_vec3_t refPosLocal{};

      uint16_t refId{};
      uint16_t lastRefId{};

    public:
    Attach() = default;

    /**
     * Updates the difference and returns it
     * @return difference since last call
     */
    fm_vec3_t update(const fm_vec3_t &ownPos);

    /**
     * sets a new mesh to track.
     * @param meshCollider
     */
    void setReference(const Coll::MeshCollider *meshCollider);
  };
}
