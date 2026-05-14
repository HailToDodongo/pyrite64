/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include "vecMath.h"
#include "raycast.h"
#include "collisionScene.h"

namespace P64
{
  class Object;
}

namespace P64::Coll
{
  struct CharacterBody
  {
    struct Settings
    {
      fm_vec3_t up{0.0f, 1.0f, 0.0f};  // Worlds up-direction, determines gravity direction and what floors are
      fm_vec3_t centerOffset{0.0f, 0.0f, 0.0f}; /// Offset in meters from the object origin to the capsule center.

      float gravity{30.0f};            // meters / s^2 applied along -up
      float maxFallSpeed{55.0f};       // Terminal speed along -up
      float floorMaxAngle{45.0_deg};   // Max walkable slope (radians from up)
      /// Max height of a step the character automatically climbs.
      /// The physics capsule is shortened from the bottom by this amount, making
      /// stair risers below this height invisible to collision. The floor snap
      /// then lifts the character up. Must be <= innerHalfHeight (height/2 - radius).
      /// floorSnapDistance must be >= stepHeight for stair climbing to work.
      float stepHeight{0.25f};
      /// How far below the full capsule bottom the floor snap probe reaches.
      /// Controls sticking to ground on slopes and snapping over step edges.
      /// Must be >= stepHeight for stair climbing to work.
      float floorSnapDistance{0.30f};
      float radius{0.5f};              // Capsule radius in meters.
      /// Capsule total height in meters (including both hemispherical caps).
      /// Must be >= 2 * radius, values below that clamp to a sphere.
      float height{2.0f};
      RaycastColliderTypeFlags collTypes{RaycastColliderTypeFlags::MESH_COLLIDERS};
      uint8_t maxSlides{4};            // Slide iterations per move
      uint8_t readMask{0xFF};
    };

    CharacterBody(Object& owner_);

    Settings settings{};

    /// velocity to be applied during the next 'moveAndSlide' call.
    fm_vec3_t inputVelocity{};

    /**
     * Returns the current internal velocity (after gravity + slide projection).
     * @return velocity
     */
    const fm_vec3_t& getVelocity() const { return velocity; }

    /**
     * Override the full internal velocity (e.g. to perform a jump impulse on Y).
     * For normal movement prefer setting 'inputVelocity'.
     * @param newVelocity
     */
    void setVelocity(const fm_vec3_t& newVelocity) { velocity = newVelocity; }

    /**
     * Check the current grounded state.
     * @return true if standing on a floor or steep surface
     */
    bool isOnFloor() const { return onFloor; }

    /**
     * Returns the normal of the last floor, including steep surfaces.
     * @return normal
     */
    const fm_vec3_t& floorNormal() const { return contactNormal; }

    /**
     * True when the body is on an upward-facing surface steeper than the limit.
     * If this is the case, isOnFloor() will also return true.
     * This function here can be used to determine on which of the two you are standing
     * @return
     */
    bool isOnSteepSurface() const { return onSteepSurface; }

    /**
     * Returns true if the body was snapped upwards to a floor (e.g. stairs) after the last 'moveAndSlide' call.
     * It will reset automatically at the beginning of 'moveAndSlide'.
     * @return true if snapped
     */
    bool didSnapToFloor() const { return snappedFloor; }

    /**
     * Performs movement for the body.
     * Handles: gravity, sweeps, slides on hits,
     * snaps to floor, then writes the final position back to the owning Object.
     *
     * @param deltaTime time step to move for, in seconds
     * @param scene collision scene to query for sweeps and snaps
     */
    void moveAndSlide(float deltaTime, CollisionScene& scene);

    /**
     * Draws the capsule shape and floor-snap probe in debug wireframe.
     * Call once per frame after moveAndSlide.
     */
    void debugDraw() const;

  private:
    Object* owner; // Note: we can't use a reference since it prevents a copy-constructor
    fm_vec3_t velocity{};
    fm_vec3_t contactNormal{};

    uint8_t onFloor{};
    uint8_t onSteepSurface{};
    uint8_t snappedFloor{};

    /// Capsule center in physics-space, derived from owner's current position + offset.
    fm_vec3_t capsuleCenter() const;

    /// Support distance of the implicit capsule along a unit direction `dir`,
    /// measured from the capsule center. The capsule's long axis is along
    /// `settings.up`; for axis-aligned `dir` this collapses to either the
    /// radius (horizontal) or half-height (vertical).
    float extentAlong(const fm_vec3_t& dir) const;
  };
}
