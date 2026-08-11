# Surface

Allocates one or more surfaces (`surface_t`) at runtime in a given pixel format and size.\
These can be used as offscreen render-targets or as CPU/RDP drawn textures.

To render the scene into a surface, point a {doc}`Camera <camera>` component at it
via its **Target** option.

The component itself does not draw anything,\
fetch the surface from a script or other component to make use of it:

```cpp
auto surfComp = obj.getComponent<P64::Comp::Surface>();
surface_t &surf = surfComp->getSurface();
```

With **Double** or **Triple** buffering, the component cycles through its buffers each frame:\
`getSurface()` always returns the current frame's surface,\
while `getPrevSurface()` returns the last frame's one.\
Use this to safely write to a surface while the RDP may still read the previous one,\
or for feedback effects that need last frame's result.

## Options

| Option | Description |
|--------|-------------|
| **Size** | Width and height of the surface in pixels. |
| **Format** | Pixel format of the surface (`tex_format_t`), e.g. `RGBA16` or `CI8`.<br>Note that the RDP can only render to `RGBA16`, `RGBA32` and `I8`/`CI8` targets. |
| **Buffering** | Number of buffers to allocate:<br>• **Single**: one surface, returned every frame.<br>• **Double** / **Triple**: cycles through 2 or 3 surfaces each frame. |
| **Clear** | If set, the current surface is cleared automatically each frame. |
| **Clear Color** | Color used for clearing. For intensity formats (`I4`-`IA16`) the red and alpha channels are used, for `CI4`/`CI8` the red channel serves as the palette index. |
| **Depth Buffer** | Allocates a matching depth buffer, shared by all buffers. Cameras targeting this surface will use it, without one they re-use the main depth buffer instead. |

Independent of the **Clear** setting, all buffers are cleared once on creation.

## See also

- {cpp:struct}`P64::Comp::Surface`: the runtime component in the C++ API.
