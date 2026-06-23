# Layers

The **Layers** window manages the draw layers of the scene. Layers control the order things are
drawn in and the render state used for each group of draws. They come in three lists:

- **3D Layers**: for 3D geometry.
- **Particle Layers**: for particle effects.
- **2D Layers**: for 2D / UI drawing.

Objects reference a layer (for example through the Draw-Layer option of the
{doc}`Model <../components/model>` component), so layers are where shared render settings live.

## Managing layers

Use the **Add** button next to a list to append a new layer. Right-click a layer header to
**Duplicate** or **Delete** it (the last remaining layer in a list cannot be deleted). The
**Reset** button at the bottom restores the default layer set.

## Layer options

Each layer expands to its settings:

| Option | Description |
|--------|-------------|
| **Name** | The layer's display name. |
| **Z-Compare** | Whether draws test against the depth buffer. |
| **Z-Write** | Whether draws write to the depth buffer. |
| **Blending** | None (opaque), Multiply (alpha), or Additive. |
| **Light-Mode** | Multiply (default) or Add (for baked light). |
| **Fog** | Enables fog for the layer, with the options below. |
| **Fog-Mode** | Use the clear color, a custom color, or leave the fog color unchanged. |
| **Fog Color** | The custom fog color (when Fog-Mode is set to custom). |
| **Fog Min / Max** | The near and far fog distances. |

## See also

- {doc}`Scene <sceneInspector>`: the scene-wide framebuffer and pipeline settings.
- {doc}`Materials <../materials/overview>`: how per-material state interacts with layer state.
