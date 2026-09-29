# Collider

```{image} /_static/img/ui_comp_coll_body.png
:align: center
```

A primitive-shape collider attached to the object.\
Unlike a {doc}`Collision-Mesh <collMesh>` this uses a simple analytic shape,\
which is cheap and suitable for moving/dynamic objects.\
Pair it with a {doc}`Rigid-Body <rigidBody>` for full physics simulation.

## Options

| Option | Description |
|--------|-------------|
| **Type** | The collider shape:<br>• **Box**<br>• **Sphere**<br>• **Cylinder**<br>• **Capsule**<br>• **Cone**<br>• **Pyramid** |
| **Shape size** | The dimensions for the chosen shape in meters, e.g. *Half Size* for a box, *Radius* for a sphere, or *Radius* plus *Half Height* for cylinders/capsules/cones. Scaled by the object's scale. |
| **Offset** | Offset of the shape's center relative to the object's origin, in meters. Scaled by the object's scale. |
| **Trigger** | When enabled, the collider reports overlaps as events but produces no physical (push-back) response. |
| **Reacts to** | The collision layers this body reads (which layers it collides with). |
| **Is Affecting** | The collision layers this body writes (which layers see it). |
| **Friction** | Surface friction, `0` to `1`. |
| **Bounce** | Restitution / bounciness, `0` to `1`. |

## Changing a collider at runtime

Every value on this page is set on the collider itself, the component only creates it and
registers it with the collision scene:

```cpp
auto &coll = obj.getComponent<Comp::CollBody>()->collider;

auto halfExtend = coll.halfExtend();
halfExtend.y += 0.5f;                        // e.g. the half height of a cylinder/capsule/cone
coll.setHalfExtend(halfExtend);

coll.setCylinderShape(0.5f, 2.0f);           // size and shape type in one go
coll.setShapeType(Coll::ShapeType::Capsule); // keeps the size, folds it into the new shape
coll.setParentOffset({0.0f, 1.0f, 0.0f});    // moves the shape's center
```

Sizes and offsets are in the object's local space, exactly like the values in the editor: the
object scale is applied on top of them and stays applied when the object is scaled later on.
The object-scaled result the collision detection runs on is read-only and named `world...`,
e.g. {cpp:struct}`P64::Coll::Collider`'s `worldCylinderShape()` or `worldAabb()`.

`setHalfExtend()` works for every shape. Shapes that don't use all three axes fold them in, e.g.
a cylinder takes its radius from `max(x, z)` and its half height from `y`, and a sphere takes its
radius from the largest component.

All setters keep the AABB (and with it the broadphase) plus the mass properties of an attached
{doc}`Rigid-Body <rigidBody>` in sync, and wake up sleeping bodies the change may touch. That is
why the dimensions cannot be written directly.

## See also

- {doc}`Collision & Physics <../collision>`: general collision & physics docs.
- {doc}`Rigid-Body <rigidBody>`: add physics simulation to a collider.
- {cpp:struct}`P64::Comp::CollBody`: the runtime component in the C++ API.
