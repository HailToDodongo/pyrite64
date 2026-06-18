# Rigid-Body

```{image} /_static/img/ui_comp_rigid_body.png
:align: center
```

Gives the object a full rigid-body physics simulation:\
it is moved by forces, gravity and collisions.\
A {doc}`Collider <collBody>` must be attached to define the body's shape.

## Options

| Option | Description |
|--------|-------------|
| **Mass** | The body's mass. |
| **Is Kinematic** | When enabled, the body is not moved by the simulation (no forces/gravity), but still pushes other bodies. Use it for moving platforms etc. |
| **Constr. Pos X / Y / Z** | Lock translation along the given world axis. |
| **Constr. Rot X / Y / Z** | Lock rotation around the given world axis. |
| **Has Gravity** | Whether scene gravity is applied to this body. |
| **Gravity Scalar** | Multiplier on the gravity applied to this body. |
| **Time Scalar** | Multiplier on simulation time for this body (slow-mo / speed-up). |
| **Angular Damping** | Damping applied to angular velocity, to settle spinning. |

## See also

- {doc}`Collider <collBody>`: required to define the body's shape.
- {cpp:struct}`P64::Comp::RigidBody`: the runtime component in the C++ API.
