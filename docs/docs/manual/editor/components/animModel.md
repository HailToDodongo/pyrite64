# Model (Animated)

```{image} /_static/img/ui_comp_model_anim.png
:align: center
```

Renders an animated (skeletal) 3D model. Works like the {doc}`static model <model>`
component, but the model can play skinned animations at runtime.

## Options

| Option | Description |
|--------|-------------|
| **Model** | The animated 3D model asset to render. |
| **Open Model Editor** | Opens the model editor for the selected asset. |
| **Draw-Layer** | Which 3D draw layer the model is rendered on (see scene settings). |
| **Preview Anim.** | Selects an animation to preview inside the editor viewport. This is an editor-only aid and does not affect what plays at runtime. |
| **Material Instance** | Per-object material overrides for this model. See {doc}`Material Instance <../materials/instance>` for the full list of options. |

## See also

- {doc}`Material Instance <../materials/instance>`: the embedded material sub-UI.
- {cpp:struct}`P64::Comp::AnimModel`: the runtime component in the C++ API.
