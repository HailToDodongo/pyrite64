# Character Body

The character body is special helper to implement movement of objects\
that need non-physical behavior.\
That is, they take part in the collision scene, \
but need manual control over how collisions are resolved.\
You most likely want this for controlling the player or certain enemies.\
It handles gravity, collision sweeps, sliding along walls, stair stepping, and floor snapping.\
This page covers how it works and the full C++ API.

If you're looking for a usage example, check the `char_body` example project.

## Overview

A character body represents a capsule-shaped physics volume attached to a scene object.\
Each frame you set an input velocity and call `moveAndSlide()`, the body then:

- Applies gravity
- Sweeps the capsule through the collision scene
- Slides along surfaces it hits
- Steps over small obstacles (stairs)
- Snaps to the floor on slopes and ledges
- Writes the resolved position back to the owning object

```{image} /_static/img/char_body_overview.png
:align: center
:width: 640px
```

In contrast to actual colliders, it will never be moved by other colliders via forces.\
It itself is also invisible to other real colliders / rigid-bodies.

Certain responses are handled differently to make it control well.\
For example standing on a slope will not make it slip off (unless you cross the defined threshold).\
Moving on or off a ramp also keeps it to the floor, so you don't fly off when you move fast.

## Capsule Shape

The capsule consists of a cylindrical middle section with hemispherical caps at both ends.\
Its orientation is fixed to the defined up-axis in the settings.

```{image} /_static/img/char_body_capsule.png
:align: center
:width: 400px
```

Two distinct capsules exist internally:

| Capsule | Purpose |
|---|---|
| **Logical capsule** | The full capsule matching your radius/height settings. Used for the floor-snap probe origin and debug visualization. |
| **Physics capsule** | Shortened from the bottom by `stepHeight`. This is what actually collides during sweeps, letting the character walk over low obstacles. |

The `centerOffset` setting shifts the entire capsule relative to the object origin.\
Typically you set this to place the capsule bottom at the object's feet.

## Settings

All parameters are configured through the **Character-Body** component in the editor.\
When you add the component to an object, each setting appears as a UI field.\
The table below maps each editor label to its C++ API name.

```{list-table} Character-Body Component Settings
:header-rows: 1

* - Editor Label
  - API Name
  - Type
  - Default
  - Description
* - Radius
  - `radius`
  - `float`
  - `0.5`
  - Capsule radius in meters.
* - Height
  - `height`
  - `float`
  - `2.0`
  - Total capsule height in meters including both caps. Must be ≥ 2 × radius.
* - Offset
  - `centerOffset`
  - `fm_vec3_t`
  - `{0, 0, 0}`
  - Offset in meters from the object origin to the capsule center. Use `setCenterOffset()` at runtime to change.
* - Step Height
  - `stepHeight`
  - `float`
  - `0.25`
  - Max stair riser height the character automatically climbs. Must be ≤ inner half-height and ≤ Floor Snap Dist.
* - Floor Snap Dist.
  - `floorSnapDistance`
  - `float`
  - `0.30`
  - How far below the capsule the floor probe reaches. Controls slope stickiness and snap-over-step distance. Must be ≥ Step Height.
* - Gravity
  - `gravity`
  - `float`
  - `30.0`
  - Downward acceleration along `-up` in m/s².
* - Max Fall Speed
  - `maxFallSpeed`
  - `float`
  - `55.0`
  - Terminal velocity along `-up` in m/s.
* - Floor Max Angle
  - `floorMaxAngle`
  - `float`
  - `45°`
  - Maximum walkable slope angle (shown in degrees, stored in radians). Cosine is cached internally. Use `setUp()` at runtime to change.
* - Max Slides
  - `maxSlides`
  - `uint8_t`
  - `4`
  - Maximum slide iterations per `moveAndSlide` call. Clamped to 1–8.
* - Follow Floor
  - `followFloor`
  - `bool`
  - `true`
  - When enabled, the body is carried along with the mesh collider it is standing on as that mesh translates or rotates. Only position is carried; the body's own rotation is not changed.
* - Up Direction
  - `up`
  - `fm_vec3_t`
  - `{0, 1, 0}`
  - World up-direction. Determines gravity direction and what counts as a floor. Use `setUp()` at runtime to change.
* - Read Mask
  - `readMask`
  - `uint8_t`
  - `0x01`
  - Collision layer read mask as a bitmask. Select which collision layers the body collides with.
* - Collider Types
  - `collTypes`
  - `RaycastColliderTypeFlags`
  - `Mesh Colliders`
  - Which collider types the body interacts with (Mesh, Collider Bodies, or All).
```

### Runtime API

When using the editor component, settings are applied via `configure()` automatically.\
You only need the runtime accessors and setters:

```cpp
const Settings& getSettings() const;            // read-only access to all settings
void setUp(const fm_vec3_t& newUp);             // change up direction (auto-normalized)
void setCenterOffset(const fm_vec3_t& offset);   // change capsule center offset
```

`setUp()` has an early-out: if the new up is within ~0.8° of the current one,\
the cache refresh is skipped entirely.

If you create the body programmatically, also call `configure()` once at init:

```cpp
void configure(const Settings& s);              // bulk-apply settings + refresh caches
```

## Movement

### Input Velocity vs. Internal Velocity

The character body separates desired movement from the resolved velocity:

- **`inputVelocity`**: Set this each frame to the movement you want. It represents player/AI intent and is preserved across frames. Only the horizontal component (perpendicular to `up`) is used; the vertical component is always ignored.
- **Internal velocity**: Managed by `moveAndSlide()`. Gravity is added to it, slides modify it, and floor contact zeroes the vertical component. Read it via `getVelocity()`.

```cpp
// Set desired horizontal movement
charBody.inputVelocity = {moveX, 0.0f, moveZ};

// For a jump, directly modify internal velocity
charBody.setVelocity(charBody.getVelocity() + up * jumpSpeed);
```

### moveAndSlide

```cpp
void moveAndSlide(float deltaTime);
```

This is the main update function. Call it once per frame, typically from `update()` or `fixedUpdate()`.\
Make sure to call it even if you don't move to apply gravity.\
The collision scene is retrieved internally via `SceneManager::getCurrent()`.

```{image} /_static/img/char_body_move_and_slide.png
:align: center
:width: 640px
```

Each call performs these steps in order:

1. **Reset state**: Clears per-frame flags (`onSteepSurface`, `probeFoundFloor`).
2. **Follow floor carry**: If `followFloor` is enabled and the body was on a floor, carries the body along with the floor mesh's movement since the last frame.
3. **Apply gravity**: If not on a walkable floor, gravity is added to the vertical velocity.
4. **Reshape velocity for slopes**: When grounded on a walkable slope, the horizontal velocity is projected onto the surface so movement follows the slope instead of clipping into it.
5. **Swept slide loop**: The displacement vector (velocity × deltaTime) is swept through the collision scene up to `maxSlides` times. Uses a BVH broadphase to query only nearby mesh colliders. Each hit projects the remaining displacement onto the hit plane (slide), with special handling for:
   - **Stair stepping**: The shortened physics capsule passes over risers below `stepHeight`
   - **T=0 overlap resolution**: If the capsule already overlaps, it's pushed out before re-trying
   - **Crease fix**: In V-corners, motion is projected onto the intersection line of both walls
   - **Ceiling hits**: Upward velocity is cancelled on ceiling contact
   - **Steep wall damping**: When grounded and sliding along a steep wall, vertical sliding is stripped
6. **Overlap resolution**: A zero-length sweep finds and resolves any remaining lateral overlaps.
7. **Floor probe**: A raycast downward from the capsule center detects the floor. Based on clearance and surface angle it either:
   - **Lifts** the character when sunken (stair top-out or landing correction)
   - **Snaps** the character down to maintain ground contact on slopes
   - **Sets grounded state** on walkable surfaces (updating `isOnFloor()` and `floorNormal()`)
   - **Sets steep surface state** on surfaces steeper than `floorMaxAngle`

### Slope Following

When grounded on a walkable floor, the horizontal velocity is reshaped to follow the surface.\
This means walking up or down a ramp keeps the character on the ground without manual input.

```{image} /_static/img/char_body_slope.png
:align: center
:width: 400px
```

### Stair Stepping

The physics capsule is shortened from the bottom by `stepHeight`.\
Risers below this height are invisible to the sweep, so the capsule passes through them.\
After the sweep, the floor probe detects the higher ground and lifts the character up.

```{image} /_static/img/char_body_stairs.png
:align: center
:width: 500px
```

For stair climbing to work, `floorSnapDistance` must be ≥ `stepHeight`.\
The snap distance determines how far the probe reaches below the capsule to find the floor.

### Corner & Crease Handling

When the character is pushed into a V-shaped corner (two walls meeting at an angle),\
the slide from each wall would point into the other, trapping the character.\
The crease fix detects this and projects motion onto the intersection line of both planes,\
so the character slides along the corner crease.

The same logic applies to the static overlap resolver at T=0.

### Moving Platforms

When `followFloor` is enabled (default), the body is carried along with the mesh collider it currently stands on.\
Each frame the contact point at the capsule foot is recorded in the mesh's local space.\
On the next frame the new world position of that same local point is read back and the body is shifted by the difference,\
so the character rides translating and rotating platforms naturally.

Only the body's position is carried, not its rotation, so the character does not visually spin with the platform.\
If you need the character to face-spin with the platform, apply the platform's yaw delta to the object yourself.

The carry is applied before the sweep, so player input is integrated relative to the new floor position,\
and any residual overlap from a fast-moving platform is resolved by the regular collision sweep.

## State Queries

After `moveAndSlide()` completes, you can query the body's state:

```cpp
bool isOnFloor() const;
```
Returns `true` when standing on a walkable floor or steep surface.

```cpp
bool isOnSteepSurface() const;
```
Returns `true` when on a surface steeper than `floorMaxAngle`.\
In this state `isOnFloor()` also returns `true`, use this to distinguish between normal ground and steep slopes.

```cpp
const fm_vec3_t& floorNormal() const;
```
Returns the normal of the surface the character is standing on.\
Includes both walkable floors and steep surfaces.

## Teleport

```cpp
void teleport(const fm_vec3_t& ownerPos, bool resetForces = true);
```

Instantly moves the character to a new position.\
With `resetForces = true` (default), also zeroes velocity and clears grounded state, use for respawning.\
With `resetForces = false`, only the position changes, use for portals or seamless teleports.

```cpp
// Respawn at a checkpoint
charBody.teleport({spawnX, spawnY, spawnZ});

// Portal teleport preserving momentum
charBody.teleport({destX, destY, destZ}, false);
```

## Debug Draw

```cpp
void debugDraw() const;
```

Draws the capsule shape and floor-snap probe in debug wireframe.\
Call once per frame after `moveAndSlide()`.

The debug visualization uses color coding:

| Color | Meaning |
|---|---|
| **White** | Airborne, capsule is not touching any surface |
| **Green** | On a walkable floor |
| **Orange** | On a steep surface |
| **Yellow line** | Step zone indicator at the bottom of the physics capsule |
| **Blue line** | Floor snap probe ray, reaches from capsule center downward |
| **Cyan line** | Contact normal direction from the capsule bottom |
| **Dark grey outline** | Full logical capsule (for comparison with the shortened physics capsule) |

## Usage Example

### Via the Editor Component (recommended)

The typical workflow: add a **Character-Body** component to an object in the editor,\
configure its settings in the inspector, then access it from your user script.

```cpp
#include "scene/components/charBody.h"

void update(Object& obj, Data* data, float deltaTime)
{
  // Get the character body from the editor-assigned component
  auto &body = obj.getComponent<P64::Comp::CharBody>()->getBody();

  // Change up direction at runtime (e.g. planet gravity transition)
  body.setUp(data->currentUp);

  // Read the current up for camera alignment / movement math
  const fm_vec3_t up = body.getSettings().up;

  // Set desired horizontal movement from player input
  body.inputVelocity = {moveX, 0.0f, moveZ};

  // Jump, override internal velocity along up
  if(jumpPressed && body.isOnFloor()) {
    body.setVelocity(body.getVelocity() + up * jumpSpeed);
  }

  // Run physics, collision scene is retrieved internally
  body.moveAndSlide(deltaTime);

  // Query state after the step
  if(body.isOnFloor()) {
    // normal ground or steep surface
  }
  if(body.isOnSteepSurface()) {
    // slope too steep to walk on
  }

  // Debug overlay (hold Z in the char_body example)
  body.debugDraw();
}
```

### Programmatic Setup (without editor)

If you need to create a character body entirely from code,\
construct it directly and call `configure()`:

```cpp
#include "collision/characterBody.h"

void init(Object& obj, Data* data)
{
  data->charBody = Coll::CharacterBody(&obj);

  data->charBody.configure({
    .up               = {0.0f, 1.0f, 0.0f},
    .centerOffset     = {0.0f, 1.0f, 0.0f},   // place capsule bottom at origin
    .gravity          = 30.0f,
    .maxFallSpeed     = 55.0f,
    .floorMaxAngle    = 45.0_deg,
    .stepHeight       = 0.25f,
    .floorSnapDistance = 0.30f,
    .radius           = 0.25f,
    .height           = 1.0f,
    .collTypes        = Coll::RaycastColliderTypeFlags::MESH_COLLIDERS,
    .maxSlides        = 4,
    .readMask         = 0xFF,
    .followFloor      = true,
  });
}

void update(Object& obj, Data* data, float deltaTime)
{
  auto& body = data->charBody;
  // ... same usage as the component example above
}
```

For a complete example with camera controls, jumping, coyote time, planet gravity,\
and steep-surface speed reduction, see the `char_body` example project in the repository.
