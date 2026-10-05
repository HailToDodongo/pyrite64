# Model (Animated)

```{image} /_static/img/ui_comp_model_anim.png
:align: center
```

Renders an animated (skeletal) 3D model. Works like the {doc}`static model <model>`
component, but the model can play skinned animations at runtime.

Playback is organized in layers, each shown as its own collapsible section.
There is always a **Base Layer**, and you can add up to three overlay layers
with **Add Layer** (remove one with the trash button in its header). All layers
play on the same skeleton. Overlay layers are meant for
animations that move different bones than the base (e.g. a wave on top of a
walk). When they overlap, higher layers win. Each layer can hold up to two
animations that are blended together with a factor, which is also what
crossfades use.

## Options

| Option | Description |
|--------|-------------|
| **Model** | The animated 3D model asset to render. |
| **Open Model Editor** | Opens the model editor for the selected asset. |
| **Draw-Layer** | Which 3D draw layer the model is rendered on (see scene settings). |

Per layer section (**Base Layer** and each added layer):

| Option | Description |
|--------|-------------|
| **Animation** | The layer's main animation. The base layer's is also used as the preview inside the editor viewport. |
| **Blend With** | Optional second animation, blended over the main one. |
| **Blend Factor** | Blend amount between the two animations (0 = main only, 1 = blend only). |
| **Loop** | Restarts the layer's animations when they end. |
| **Play** | Automatically plays the layer when the object spawns. If unset, the layer stays paused until a script starts it. |
| **Speed** | Playback speed factor of the layer (1.0 = normal). |

| Option | Description |
|--------|-------------|
| **Material Instance** | Per-object material overrides for this model. See {doc}`Material Instance <../materials/instance>` for the full list of options. |

## Runtime control

Scripts reference animations by name-hash, which is resolved at compile time
and looked up per model instance. The same code therefore works across
different models:

Layers are addressed by index through `getLayer()` (defaults to `0`, the base
layer), which returns a lightweight handle you call methods on:

```cpp
auto anim = obj.getComponent<Comp::AnimModel>();
anim->getLayer().play("Run"_hash);            // base layer
anim->getLayer(1).play("Wave"_hash);          // overlay layer 1
anim->getLayer().crossfade("Idle"_hash, 0.25f);

auto base = anim->getLayer();
base.blend("Run"_hash, 0.5f);                 // blend Run over the base animation
base.setFactor(walkRunMix);                   // e.g. driven by movement speed
```

For per-frame hot paths, resolve the index once and use the index based calls:

```cpp
int16_t runIdx = anim->findAnim("Run"_hash); // -1 if the model lacks it
if(runIdx >= 0) anim->getLayer().playByIdx(runIdx);
```

## See also

- {doc}`Material Instance <../materials/instance>`: the embedded material sub-UI.
- {cpp:struct}`P64::Comp::AnimModel`: the runtime component in the C++ API.
