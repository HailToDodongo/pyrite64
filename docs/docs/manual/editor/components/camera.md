# Camera

```{image} /_static/img/ui_comp_camera.png
:align: center
```

Turns the object into a camera that renders the scene to a viewport.\
Multiple cameras can be active at once (e.g. for split-screen).

## Options

| Option | Description |
|--------|-------------|
| **Controlled** | How the camera transform is driven:<br>• **Manually**: you set the camera position/rotation yourself (e.g. from a script).<br>• **By Object**: the camera follows the object's transform. |
| **Offset** | The viewport's top-left offset on screen, in pixels. |
| **Size** | The viewport's size on screen, in pixels. |
| **FOV** | Vertical field of view, in degrees. |
| **Near** | Near clip plane distance. |
| **Far** | Far clip plane distance. |
| **Aspect** | Aspect ratio used for the projection. |

## See also

- {cpp:struct}`P64::Comp::Camera`: the runtime component in the C++ API.
