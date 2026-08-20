# Breaking Changes

Breaking Changes by version they were introduced in.

## v0.9.0

Every length in the editor and engine is now in meters.

Before, scenes stored positions and sizes in "visual units", and a per-scene
`Visual Units Per Meter` setting (default `100`) converted them into the meters the physics
system used. On top of that, models rendered at whatever size their vertices were quantized to
(the asset's `Base-Scale`), so the real-world size of a model depended on an import setting and
had to be compensated with object scale.

Now `Object::pos`, all component lengths, the viewport and the scene files are meters, and models
render at the size they were modeled at. The conversion into the fixed-point units the RSP needs
only happens at render time.

### Project migration

Scenes and prefabs carry a format version. Opening a project made with an older version asks once
before converting it and lists the files it will rewrite.\
Files are changed in place and the old format cannot be restored, so commit or back up your
project first.

Declining closes the project again. Building a project that was not updated fails with an error,
both in the editor and via the CLI.

Converted automatically:

- object positions and scales in all scenes and prefabs
- camera **Near**, **Far** and **Ortho Size**
- light **Size**
- collider and culling **Size** / **Offset**
- **Fog Min / Max**
- prefab-instance overrides of all of the above
- the scene setting `Visual Units Per Meter`, renamed to **Render Scale** and moved to the
  *Advanced* section. Its value is kept, so existing scenes render as before.
- built `.t3dm` files are deleted, so every model is re-exported at the new vertex precision

You have to adapt yourself:

- **Script arguments.** The editor cannot know which of them are lengths, so they keep their old
  value. Divide any argument holding a position, size, distance or speed by the scene's old
  `Visual Units Per Meter` (`100` unless you changed it).
- **Hardcoded lengths in C++ scripts**, same conversion. See below.

### Object positions and lengths in C++

`Object::pos` is in meters now, `Object::scale` stays a unitless multiplier of the model's
authored size.

Anything in a script that is a length has to be divided by the old visual-units-per-meter:

```cpp
// Before:
constexpr float CAMERA_DISTANCE = 200.0f;
obj.pos.y = data->startPos.y + fm_sinf(data->time) * 140;
if(obj.pos.y < -750.0f) body.teleport({0, 100, 0});

// Now:
constexpr float CAMERA_DISTANCE = 2.0f;
obj.pos.y = data->startPos.y + fm_sinf(data->time) * 1.4f;
if(obj.pos.y < -7.5f) body.teleport({0, 1.0f, 0});
```

This includes movement speeds, camera offsets, raycast origins, particle sizes and any epsilon
you compare positions against.

The physics and collision APIs were already in meters and did not change. What changes is that
you no longer convert into them:

```cpp
// Before:
auto ray = Coll::Raycast::create(obj.pos * Coll::getInvGfxScale(), dir, 5.0f, ...);
floorPos = hit.point * Coll::getGfxScale();

// Now:
auto ray = Coll::Raycast::create(obj.pos, dir, 5.0f, ...);
floorPos = hit.point;
```

### gfxScale removed

These no longer exist:

```
Coll::getGfxScale()
Coll::getInvGfxScale()
Coll::setGfxScale()
```

`collision/gfxScale.h` is still there, but only to fail the build with a message pointing here.
Including it at all is an error, so remove the include together with the calls. There is no
replacement, the values are already in meters.

`CollisionScene::configureSimulation()` lost its trailing `gfxScale` parameter.

### Rendering in meters

These take meters now and apply the scale internally.\
Passing an old visual-unit value makes things 100x too large:

```
Debug::drawLine, drawCapsule, and every other debug shape
PTX::Sprites::add                (particle position)
Lighting::addPointLight          (position and size)
Camera::setLookAt, pos, target
Camera::near, far, orthoSize
Camera::getScreenPos             (input is world position in meters, the returned screen position stays pixels)
```

If you project a world position to the screen yourself, use the camera instead of tiny3d, so the
scale is applied:

```cpp
// Before:
fm_vec3_t screenPos{};
t3d_viewport_calc_viewspace_pos(nullptr, &screenPos, &obj.pos);

// Now:
auto &cam = SceneManager::getCurrent().getActiveCamera();
fm_vec3_t screenPos = cam.getScreenPos(obj.pos);
```

Raw `t3d_*` calls still work in render units. If you draw a model manually, or write into a t3d
vertex buffer, convert yourself:

```cpp
#include "renderer/renderScale.h"

// model matrix from a meter-space transform, incl. the model's vertex scale
Renderer::fillModelMatrixFP(mat, obj.scale, obj.rot, obj.pos,
  AssetManager::getVertexScale("myModel.t3dm"_asset));

// or scale positions by hand
auto posRender = pos * Renderer::getRenderScale();
```

### Base-Scale is gone

The **Base-Scale** import setting was removed from 3D models. The factor used to quantize vertex
positions to int16 is now computed from the model's bounds so it uses the full range, and is
divided back out when rendering. Models therefore appear at their real size and no longer need to
be scaled in the editor to compensate.

The asset inspector shows the resulting precision. **Manual Precision** overrides it, which is
only needed when animation moves vertices well outside the model's rest pose.

For fonts the same setting still means point size and is unchanged.

### Render Scale

The old `Visual Units Per Meter` setting lives on as **Render Scale** under *Advanced* in the
scene settings. It only controls how many fixed-point units the RSP gets per meter, i.e. render
precision - gameplay, physics and all authored values stay in meters.

The default of `100` gives roughly ±327m of usable world space around the origin. Lower it for
larger scenes, raise it for very small ones.

### Editor preferences

The viewport fly and zoom speeds are stored in meters now. Their preference keys were renamed, so
existing settings fall back to the new defaults. Re-adjust them under *Preferences* if you had
them customized.

### Changing colliders in C++

`Coll::Collider` now owns the geometry of a collider and is the only place that changes it.
Dimensions and the offset are in the object's local space (the same values the editor shows), the
object-scaled result the collision runs on is derived, read-only and named `world...`.

Writing to a shape used to leave the broadphase with the old size, so the non-const shape
accessors were replaced by setters that keep the world AABB, the mass properties of an attached
rigid body and the sleep state in sync:

```cpp
// Before:
auto &coll = obj.getComponent<Comp::CollBody>()->collider;
coll.cylinderShape().halfHeight = 2.0f;             // object-scaled, silently stale broadphase

// Now:
auto halfExtend = coll.halfExtend();                // local, like the editor
halfExtend.y = 2.0f;
coll.setHalfExtend(halfExtend);
// or, size and type in one call:
coll.setCylinderShape(coll.halfExtend().x, 2.0f);
```

Note that `set{SHAPE}Shape()` will also change the shape type if the collider was not of this
shape before.

Reading the object-scaled dimensions, e.g. inside a collision callback, gained a `world` prefix:
`sphereShape()` became `worldSphereShape()`, `boxShape()` became `worldBoxShape()`, and so on.

`Comp::CollBody` is a plain holder now, it creates the collider from the editor values and
registers it with the collision scene. `orgScale` and its half extend / shape / scale helpers are
gone, use the `collider` member for all of it. `Collider::setShapeType()` no longer resets the
dimensions to zero, it keeps the size and folds it into the new shape.

## v0.7.0

This version completely reworked the material system as well as the collision/physics system.<br>
tiny3d materials are no longer used, and a strict separation between the material in the model and the instance in the object was done.<br>
Existing projects should still look and run exactly the same, so for settings in fast64 or the editor no changes are needed.<br>
The C++ API did however introduce some breaking-changes.

As for the collision/physics system, a new `RigidBody` component was introduced that completely separates
collision detection and eventing from object separation and physics response.
The existing `Collision-Body` component that had a shared role got changed into a pure Collider and lost the "is Fixed" flag.<br>
The C++ API for collision and physics components and interactions with the collision-scene as well as raycasts have breaking-changes.<br>
A new fixedUpdate Callback was added that runs at the same frequency as the physics system step. This is now the go-to place for user-side scripts to interact with the collision and physics scene (e.g. applying velocities or handling collision events) and should be migrated for all existing scripts interacting with the collision scene.

### Material Instance
Overriding material properties is now done through a "material instance" each mesh component has:

```cpp
// Before:
model->material.colorPrim = {0xFF, 0xFF, 0xFF, 0xFF};

// Now:
model->getMatInstance().colorPrim = {0xFF, 0xFF, 0xFF, 0xFF};
```
This instance now also has additional members for e.g. tile scrolling and dynamic textures.<br>
Any attributes not declared as settable in the editor are ignored even if set on the C++ side. 

### Tiny3D API

Due to no longer using tiny3d materials, the builtin functions that may do so no longer work.<br>
To avoid accidental use, they will now throw a runtime error if used.<br>
This includes the following functions:
```
t3d_model_get_material 
t3d_model_draw_material
t3d_model_draw
t3d_model_draw_custom
t3d_model_draw_skinned
```
There is currently no (public) API replacement,<br>
instead the newly added material options should be used.

Those allow setting additional properties not settable in fast64, as well as
handling things like tile scrolling or dynamic textures.

If you did use those functions before and cannot replace them with the new system,
please open an issue on GitHub so that your use-case can be added officially.

### Colliders

The `isFixed` flag was removed from the collider component and is now part of the rigidbody component as `isKinematic`.<br>
Colliders now support additional shapes [Sphere, Box, Capsule, Cylinder, Cone, Pyramid] and all shaped may be arbitrarily oriented in space - this means they will follow their owning objects orientation offset by the parent-offset configuration. This change will affect previously configured box colliders which did not allow for rotation.

Collider components in existing projects will continue to work but may exhibit different scale & rotation properties than before.<br>
Similar to previous behaviour two colliders will only produce a contact and collision event if the read and write masks of either colliders overlap in any direction.

Collider components now expose settings for friction and bounce which can affect the behaviour of rigidbody components attached to the same object.

### Rigidbodies

The rigidbody component is responsible for taking over the physical simulation part that was previously also part of the old collision-body component.<br>
A rigidbody may react to contacts produced by colliders attached to the same object. It may receive impulses, velocities, gravity etc. and will be simulated accordingly. Objects that do not have a rigidbody component will not be simulated or separated on collision even if they have a collider with the matching read mask attached, in this case only a collision event will be triggered.

New import for the rigidbody component:
```
scene/components/rigidBody.h
```


Instead of interacting with the `CollBody` component as before for simulation you would now access the `RigidBody` component, so
```cpp
auto coll = obj.getComponent<Comp::CollBody>();
```
becomes:
```cpp
auto rbody = obj.getComponent<Comp::RigidBody>();
```

or if you want to access or modify collider properties during runtime you may continue to access the CollBody component as before.

Some APIs of the previous `BCS` struct no longer exist or have been replaces by either collider or rigidbodyx APIs and members.

e.g where you might have previously accessed the bodies center:
```cpp
auto coll_comp = obj.getComponent<Comp::CollBody>();
auto &bcs = coll_comp->bcs;
fm_vec3_t center = bcs->center;
```
you would now access either
```cpp
auto rbComp = obj.getComponent<Comp::RigidBody>();
auto &rb = rbComp->getBody();
fm_vec3_t center = rb.worldCenterOfMass();
```
for the compounded center of mass of all the colliders on an object, or
```cpp
auto coll_comp = obj.getComponent<Comp::CollBody>();
auto &coll = coll_comp->collider;
fm_vec3_t center = coll.worldCenter();
```
for a single colliders center.<br>

This pattern continues for the previous bcs halfExtends, velocity & offset, AABB information etc. The hitTriTypes member was fully removed as the collision system was made more unopinionated about the nature of the world.

### Raycasts

Raycasts were split into a `RaycastHit` and `Raycast` part.<br>
The Raycast holds the information about the ray, namely the origin of the ray, it's direction, the maximum distance (new) and more granular information about what it can hit (Colliders, Meshes, Trigger Colliders, ReadMask).
To create a raycast call
```cpp
Coll::Raycast ray = Coll::Raycast::create(origin, direction, maxDistance, colliderTypeFlags, hitTriggers, readMask);
```
To actually cast the ray into the collision scene do:
```cpp
auto &collScene = SceneManager::getCurrent().getCollision();
Coll::RaycastHit hit;
collScene.raycast(ray, hit);
```
Then the `hit` object will contain information about the raycast result.

### Object IDs

Object IDs are no longer set or stored in the editor.<br>
Previously every object had an editable numeric **ID** field, which could clash between objects and had to be de-duplicated on save.

IDs now only exist at runtime: they are assigned automatically during the project build and are **not** stable across builds.<br>
The **ID** field was removed from the object inspector, and `id` is no longer written to scene files - a legacy `id` in existing scenes is ignored and dropped on the next save.

No action is needed for the objects themselves, they get valid IDs assigned on the next build.<br>
References *between* objects (the `Constraint` component, `Object`-typed script arguments, etc.) already used UUIDs and keep working unchanged.

### Node-Graph object references

The **Send Event** and **Delete Object** nodes no longer contain an object-ID dropdown.<br>
Node-graphs are shared assets with no scene context, so they could never reliably reference a specific scene object by ID.

Object targets are now provided through the new **Object** node:
1. Add an **Object** node to the graph and give it a *slot* number.
2. Connect its output to the *Object* input of a **Send Event** / **Delete Object** node.
3. On each object that uses the graph, the **Node-Graph** component now shows a picker per slot where you select the actual scene object.

An unconnected *Object* input still targets the object running the graph (`<Self>`).

Existing graphs that referenced another object by ID fall back to `<Self>` (a warning is logged on load) and must be re-wired using the steps above.<br>
Graphs that only used `<Self>` are unaffected.




## v0.5.0

The object script `initDelete` function got split into `init` and `destroy`.\
Newly created scripts use the newer version, old scripts will fail on building the project.

To migrate existing scripts, split the existing function. For example:
```cpp
void initDelete(Object& obj, Data *data, bool isDelete)
{
  if(isDelete) {
    rspq_call_deferred((void(*)(void*))rspq_block_free, data->dplBg);
    return;
  }

  rspq_block_begin();
  ...
  data->dplBg = rspq_block_end();
}
```
Becomes:
```cpp
void init(Object& obj, Data *data)
{
  rspq_block_begin();
  ...
  data->dplBg = rspq_block_end();
}

void destroy(Object& obj, Data *data)
{
  rspq_call_deferred((void(*)(void*))rspq_block_free, data->dplBg);
}
```
