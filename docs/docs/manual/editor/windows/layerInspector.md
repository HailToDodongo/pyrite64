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
**Duplicate** or **Delete** it. Drag a layer header onto another one to move it there, which changes
the order layers are drawn in. A list always keeps at least one layer.
The **Reset** button at the bottom restores the default layer set.

## Using layers in scripts

`DrawLayer::use3D`, `DrawLayer::usePtx` and `DrawLayer::use2D` accept a position in the list,
or the hash of a layer name with the `_hash` suffix.
Positions change when layers are reordered, names keep working:

```cpp
DrawLayer::use3D("3D Transp."_hash);
// ... draw something ...
DrawLayer::useDefault();
```

If you want to cache the hash-lookup, you can remember the returned index:
```cpp
uint32_t layer = "3D Transp."_hash;
...
layer = DrawLayer::use3D(layer);
```
This will lazily resolve and remember the index.

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
| **Fog Min / Max** | The near and far fog distances, in meters. |

## See also

- {doc}`Scene <sceneInspector>`: the scene-wide framebuffer and pipeline settings.
- {doc}`Materials <../materials/overview>`: how per-material state interacts with layer state.
