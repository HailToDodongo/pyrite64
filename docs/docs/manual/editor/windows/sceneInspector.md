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

These are start values applied once when the scene loads.\
At runtime scripts can change everything through the tinySQ API (e.g.
`Audio::engine().setReverbBus(...)` or `setMasterGain(...)`).

| Option | Description |
|--------|-------------|
| **Mixer Freq.** | The audio output sample rate, from 8000 Hz up to 48000 Hz. |
| **Master Volume** | Initial master volume of the whole mix. |
| **Reverb** | Enables the shared reverb bus. Sounds decide how much they send into it (see the Audio components). |
| **Rev. Gain** | Return gain of the reverb bus. |
| **Rev. Feedback** | Reverb feedback, higher values give a longer tail. |
| **Rev. Cross** | Left/right cross-feed, widens the reverb tail. |
| **Rev. Send Scale** | Global scale on every sound's reverb send. |

## Physics

| Option | Description |
|--------|-------------|
| **Tick Rate** | Physics update rate. |
| **Interpolate Transforms** | Smooth object transforms between physics ticks. |
| **Gravity** | Global gravity vector. |
| **Visual Units Per Meter** | Conversion between physics meters and visual units. |
| **Solver Vel. / Pos. Iterations** | Constraint solver iteration counts. |

## See also

- {doc}`Layers <layerInspector>`: the draw layers used to order and configure rendering.
- {doc}`Collision <../collision>`: the collision and physics systems these settings drive.
