# Character-Body

```{image} /_static/img/ui_comp_char_body.png
:align: center
```

A kinematic capsule controller for objects that need non-physical movement with
manual control over collision response, typically the player or certain enemies.\
It handles gravity, capsule sweeps, sliding along walls, stair stepping and floor
snapping.\
For the runtime workflow (setting velocity and calling `moveAndSlide()`),\
see the dedicated {doc}`Character Body <../collision/characterBody>` physics page.

## Options

| Option | Description |
|--------|-------------|
| **Radius** | The capsule radius. |
| **Height** | The total capsule height (clamped to at least twice the radius). |
| **Offset** | Offset of the capsule center relative to the object's origin. |
| **Step Height** | Maximum obstacle height the body can step over automatically. |
| **Floor Snap Dist.** | How far below the feet the body probes to stay attached to the ground when walking down slopes/steps. |
| **Gravity** | Downward acceleration applied along the (negated) up direction. |
| **Max Fall Speed** | Terminal velocity for falling. |
| **Floor Max Angle** | Steepest slope (in degrees) still treated as walkable floor. |
| **Max Slides** | How many times per move the body may slide against surfaces before stopping (controls how well it resolves tight corners). |
| **Follow Floor** | When enabled, the body sticks to and moves with the floor. |
| **Up Direction** | The local "up" vector defining gravity and floor orientation. |
| **Read Mask** | The collision layers the body collides against. |
| **Collider Types** | What the body collides with:<br>• **Mesh Colliders**<br>• **Collider Bodies**<br>• **All** |

```{note}
The editor viewport visualizes the capsule, the step-height band, and the
floor-snap probe. The band whose field you are editing blinks to help you tune it.
```

## See also

- {doc}`Character Body <../collision/characterBody>`: physics behavior and C++ usage.
- {cpp:struct}`P64::Comp::CharBody`: the runtime component in the C++ API.
