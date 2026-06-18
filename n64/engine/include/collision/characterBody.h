/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include "vecMath.h"
#include "raycast.h"
#include "collisionScene.h"
#include "attach.h"

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
      /// When true, position is carried along with the mesh collider currently
      /// stood on (translation + rotation of the contact point). The character's
      /// own facing is not changed.
      bool followFloor{true};
    };

    CharacterBody(Object* owner_);

    /**
     * Applies settings in bulk (e.g. from editor init data).
     * Refreshes all internal caches derived from settings.
     */
    void configure(const Settings& s);

    /// Read-only access to current settings.
    const Settings& getSettings() const { return settings; }

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
     * World-space position of the bottom of the capsule.
     * @return foot position in world space
     */
    fm_vec3_t getFootPos() const;

    /**
     * Object id of the surface the body is currently grounded on (0 when airborne).
     * Lets other objects react to the character standing on them (e.g. platforms,
     * pressure plates) now that the body produces no collision events itself.
     * @return id of the floor object, or 0
     */
    uint16_t floorObjectId() const { return floorObjId; }

    /**
     * Whether the floor moved (carried) the body this frame via followFloor — i.e.
     * the surface it stands on translated or rotated and dragged the body along.
     * Useful e.g. to avoid banking a respawn point while on a moving platform.
     * Note: only reflects followFloor carry (mesh-collider floors).
     * @return true if a moving floor carried the body this frame
     */
    bool wasMovedByFloor() const { return movedByFloor; }

    /**
     * True when the body is on an upward-facing surface steeper than the limit.
     * If this is the case, isOnFloor() will also return true.
     * This function here can be used to determine on which of the two you are standing
     * @return
     */
    bool isOnSteepSurface() const { return onSteepSurface; }

    /**
     * Sets the body's up vector and refreshes internal caches.
     * The input does not need to be pre-normalized.
     * @param newUp new up direction
     */
    void setUp(const fm_vec3_t& newUp);

    /**
     * Sets the center offset and refreshes the cached rotated offset.
     * @param offset offset in meters from the object origin to the capsule center
     */
    void setCenterOffset(const fm_vec3_t& offset);

    /**
     * Instantly moves the character to a new owner position.
     * When resetForces is true (default), also zeroes velocity and clears the
     * grounded state so the body starts clean, use this for respawning.
     * When false, only the position changes (e.g. portal / seamless teleport).
     *
     * @param ownerPos New position in world space (same space as Object::pos).
     * @param resetForces If true, zero velocity and clear grounded state.
     */
    void teleport(const fm_vec3_t& ownerPos, bool resetForces = true);

    /**
     * Performs movement for the body.
     * Handles: gravity, sweeps, slides on hits,
     * snaps to floor, then writes the final position back to the owning Object.
     *
     * @param deltaTime time step to move for, in seconds
     */
    void moveAndSlide(float deltaTime);

    /**
     * Draws the capsule shape and floor-snap probe in debug wireframe.
     * Call once per frame after moveAndSlide.
     */
    void debugDraw() const;

  private:
    void refreshCache();

    Settings settings{};
    fm_vec3_t velocity{};
    fm_vec3_t contactNormal{};
    Object* owner; // Note: we can't use a reference since it prevents a copy-constructor

    // Cached derived values (refreshed by configure/setUp/setCenterOffset)
    fm_vec3_t normUp{0, 1, 0};              // normalized settings.up
    fm_vec3_t cachedCenterOffset{};          // settings.centerOffset rotated from +Y-up to normUp
    float halfHeight{1.0f};                  // max(height/2, radius)
    float innerHalfHeight{0.5f};             // halfHeight - radius
    float walkCos{0.707f};                   // cos(floorMaxAngle), used every frame

    uint8_t onFloor{};
    uint8_t onSteepSurface{};
    uint8_t probeFoundFloor{}; // set when floor probe confirms solid ground; gates gravity suppression
    uint16_t floorObjId{};     // id of the object currently grounded on (0 = airborne)
    uint8_t movedByFloor{};    // set when followFloor carried the body this frame

    Attach floorAttach{}; // tracks the contact point on the mesh stood on for followFloor

    /// Capsule center in physics-space, derived from owner's current position + offset.
    fm_vec3_t capsuleCenter() const;

    /// Support distance of the implicit capsule along a unit direction `dir`,
    /// measured from the capsule center. The capsule's long axis is along
    /// `settings.up`; for axis-aligned `dir` this collapses to either the
    /// radius (horizontal) or half-height (vertical).
    float extentAlong(const fm_vec3_t& dir) const;
  };
}
