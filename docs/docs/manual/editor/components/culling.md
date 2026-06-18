# Culling

```{image} /_static/img/ui_comp_culling.png
:align: center
```

Defines a bounding volume used to cull the object:\
when the volume is outside the camera view, the object's per-camera draw work is skipped.\
Use it to avoid drawing objects that can't be seen.

This component is checked during the drawing phase,\
and will therefore work correctly with multiple cameras.

Note that the volume is always in world-space and will **not** rotate with the object.

## Options

| Option | Description |
|--------|-------------|
| **Type** | The bounding volume shape:<br>• **Box**: an axis-aligned box defined by a half-extent.<br>• **Sphere**: a sphere defined by a radius. |
| **Size** | The half-extent (box) or radius (sphere) of the volume. |
| **Offset** | Offset of the volume's center relative to the object's origin. |

## See also

- {cpp:struct}`P64::Comp::Culling`: the runtime component in the C++ API.
