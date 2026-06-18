# Light

```{image} /_static/img/ui_comp_light.png
:align: center
```

Adds a light to the scene. 
A directional light's direction is taken from the object's rotation,\
and a point light's position from the object's position.\
Ambient lights only use the color specified.

If an object gets disabled, the light attached to it is no longer applied.

For performing temporary overrides of lighting,\
checkout the scene function for it: 
- {cpp:func}`P64::Scene::startLightingOverride`.
- {cpp:func}`P64::Scene::endLightingOverride`.

## Options

| Option | Description |
|--------|-------------|
| **Type** | The kind of light:<br>• **Ambient**: a flat, omnidirectional base light.<br>• **Directional**: a light coming from a fixed direction (the object's facing).<br>• **Point**: a light that radiates from the object's position outward. |
| **Index** | The light slot this light occupies. The engine supports a limited number of simultaneous lights; the index selects which slot to write to. |
| **Color** | The light's color (RGBA). |
| **Size** | *(Point lights only)* the radius/range of the point light's falloff. |

## See also

- {cpp:struct}`P64::Comp::Light`: the runtime component in the C++ API.
