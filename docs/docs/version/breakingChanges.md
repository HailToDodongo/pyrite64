# Breaking Changes

Breaking Changes by version they were introduced in.

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
auto rb_comp = obj.getComponent<Comp::RigidBody>();
auto &rb = rb_comp->rigid_body;
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
auto collScene = SceneManager::getCurrent().getCollision();
Coll:RaycastHit hit;
collScene.raycast(ray, hit);
```
Then the `hit` object will contain information about the raycast result.




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
