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
| **Projection** | How the scene is projected:<br>• **Perspective**: objects get smaller with distance, configured via **FOV**.<br>• **Orthographic**: no perspective distortion (e.g. for isometric or 2D games), configured via **Ortho Size**. |
| **Offset** | The viewport's top-left offset on screen, in pixels. |
| **Size** | The viewport's size on screen, in pixels. |
| **FOV** | Vertical field of view, in degrees. Only used in perspective mode. |
| **Ortho Size** | Vertical half-size of the visible area, in world units. The horizontal size is derived from it via the aspect ratio. Only used in orthographic mode. |
| **Near** | Near clip plane distance. |
| **Far** | Far clip plane distance. |
| **Aspect** | Aspect ratio used for the projection. |

## Switching the projection at runtime

The projection can be changed while the game is running:

```cpp
auto* cam = obj.getComponent<P64::Comp::Camera>();

cam->setOrthographic(300.0f);                 // switch to ortho with a given size
cam->setPerspective(T3D_DEG_TO_RAD(65.0f));   // switch to perspective with a given fov

// or toggle without touching the fov / ortho-size:
cam->setProjection(P64::Comp::Camera::Projection::ORTHOGRAPHIC);
```

## See also

- {cpp:struct}`P64::Comp::Camera`: the runtime component in the C++ API.
