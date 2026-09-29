# Scene

The **Scene** window holds the settings of the currently loaded scene as a whole, grouped into
collapsible sections. These settings affect how the whole scene is rendered and simulated, as
opposed to the per-object settings in the {doc}`Object <objectInspector>` inspector.

## Settings

| Option | Description |
|--------|-------------|
| **Name** | The scene's display name. |
| **Pipeline** | The render pipeline: Default, HDR-Bloom, or HiRes-Tex (256x). HDR-Bloom and HiRes-Tex force specific framebuffer settings. |
| **FPS-Limit** | Target frame rate cap (Unlimited, 30/25, 20/16.6, 15/12.5). |

## Framebuffer

| Option | Description |
|--------|-------------|
| **Width / Height** | Output resolution (for example 320x240). |
| **Format** | Color format, RGBA16 or RGBA32. |
| **Color** | The clear / background color. |
| **Clear Color** | Whether the framebuffer is cleared to the color each frame. |
| **Clear Depth** | Whether the depth buffer is cleared each frame. |
| **Filter** | Output filtering: None, Resample, Dedither, and combinations with AA. |

Some of these are locked when a pipeline other than Default is selected, because that pipeline
requires fixed values.

## Audio

| Option | Description |
|--------|-------------|
| **Mixer Freq.** | The audio mixer sample rate, from 8000 Hz up to 48000 Hz. |

## Physics

| Option | Description |
|--------|-------------|
| **Tick Rate** | Physics update rate. |
| **Interpolate Transforms** | Smooth object transforms between physics ticks. |
| **Gravity** | Global gravity vector, in m/s². |
| **Solver Vel. / Pos. Iterations** | Constraint solver iteration counts. |

## Advanced

| Option | Description |
|--------|-------------|
| **Render Scale** | How many fixed-point world units the RSP gets per meter, default `100`. This affects render precision only, everything else is authored and simulated in meters. |

The default gives roughly ±327m of usable space around the scene origin.
Lower it for scenes that reach further out, potentially raise it for very small ones.

## See also

- {doc}`Layers <layerInspector>`: the draw layers used to order and configure rendering.
- {doc}`Collision <../collision>`: the collision and physics systems these settings drive.
