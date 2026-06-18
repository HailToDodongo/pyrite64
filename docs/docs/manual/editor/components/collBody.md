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
| **Shape size** | The dimensions for the chosen shape, e.g. *Half Size* for a box, *Radius* for a sphere, or *Radius* plus *Half Height* for cylinders/capsules/cones. |
| **Offset** | Offset of the shape's center relative to the object's origin. |
| **Trigger** | When enabled, the collider reports overlaps as events but produces no physical (push-back) response. |
| **Reacts to** | The collision layers this body reads (which layers it collides with). |
| **Is Affecting** | The collision layers this body writes (which layers see it). |
| **Friction** | Surface friction, `0` to `1`. |
| **Bounce** | Restitution / bounciness, `0` to `1`. |

## See also

- {doc}`Collision & Physics <../collision>`: general collision & physics docs.
- {doc}`Rigid-Body <rigidBody>`: add physics simulation to a collider.
- {cpp:struct}`P64::Comp::CollBody`: the runtime component in the C++ API.
