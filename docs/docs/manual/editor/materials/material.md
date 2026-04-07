# Materials

## Editor

While materials are immutable at runtime, you can of course edit them in the editor.\
By default, a subset of settings from fast64 are used for all 3D models during import.\
If you wish to further edit those, you can open the builtin material-editor.\
In a static-mesh or animated-mesh component, click on the "Open Model Editor" button:

```{image} /_static/img/btn_model_editor.png
:align: center
:width: 340px
```

Which opens a new window listing all materials in the model:
```{image} /_static/img/model_edit_init.png
:align: center
:width: 340px
```

```{admonition} Tip:
:class: tip

You can drag & drop this window to turn it into a tab, the placement is remembered across sessions.
```

If you enable the "Override" checkbox, you can then edit the material,\
and the data from fast64 is no longer used.\
Note that materials are identified by their name inside a model,\
so changing it may lose those overrides.

The mask should now look something like this:
```{image} /_static/img/model_edit_override.png
:align: center
:width: 340px
```

Any values changed will also update in real-time in the viewport.
The number of settings you see may vary, since some settings depend on others.\
For example, if you never use a texture in the color-combiner, the UI for it does not show up.

Now for a list of all available settings in the material.

## Color Combiner

```{image} /_static/img/mat_cc.png
:align: center
:width: 250px
```

The color combiner can be seen as the "fragment shader" of the RDP.\
It is a fixed function that runs per pixel, allowing you to plug in different sources for each variable.\
Note that this is set in hardware, so the following applies in general for all N64 games.

The formula is fixed to `(A - B) * C + D`, the left side of the UI refers to the color (RGB), the right side to the alpha (A).\
If you enable "2-Cycle" mode, you also get a second round of this formula with different variables.

Each variable allows only for a subset of values, however, the selecbox limits you to those.

As a very simple example, if you want a material with a texture that is affected by lighting,\
you would set the CC to `(TEX0 - 0) * SHADE + 0`.\
Which would effectively just multiply the texture color with the vertex/lighting color.
 
Here is a list of all the available sources across variables:

| Source         | Description                                               |
|----------------|-----------------------------------------------------------|
| 0              | Fixed Value of `0.0`                                      |
| 1              | Fixed Value of `1.0` (actually `0x100`, see notes below)  |
| TEX0           | RGB Values of first texture                               |
| TEX0 Alpha     | Alpha of first Texture                                    |
| TEX1           | RGB Value of second texture                               |
| TEX1 Alpha     | Alpha of second Texture                                   |
| Prim Color     | Generic color register (RGB)                              |
| Prim Alpha     | Generic color register (A)                                |
| Env Color      | Generic color register (RGB)                              |
| Env Alpha      | Generic color register (A)                                |
| Prim LOD       | Generic Scalar register                                   |
| K4             | Generic Scalar register                                   |
| K5             | Generic Scalar register                                   |
| Noise          | Random scalar noise (screen-space)                        |
| Combined       | Result from the first cycle                               |
| Combined Alpha | Alpha-Result form first cycle, using it in the color part |
| Shade          | Interpolated per-vertex shading (vert. color * lighting)  |
| Shade Alpha    | Same as `Shade`, but alpha                                |
| LOD            | LOD level, used in mip-mapping CC setups                  |
| Center         | Chroma key center, used in YUV CC setups                  |
| Scale          | Chroma key scale, used in YUV CC setups                   |

As usual on the RDP, there are some hardware-issues you have to consider.

### Fixed-Point issues
The first is that values are handled as fixed point internally.\
So a full white color would not be `1.0` as a float, but a 8bit integer at `255`.\
At a first glance this may seem fine, but it causes issues during multiplication.\
For example, consider multiplying two white colors together:
```
Expected        : 1.0 * 1.0 = 1.0
Value to integer: 1.0 -> 255
Multiplication  : (255 * 255) << 8 = 254
Back to float   : 254 -> 0.99609375  
```
The issue is that a true 1.0 in fixed point is `256` (`0x100`), but all inputs only go up to `255`.\
So multiplying ever so slightly darkens the color.\
The only expection is the fixed `1` input in the CC which does in fact use `0x100`.\
Normally this does not matter at all (given the usual RGBA16 output), but if you need very exact colors be aware of this.

### Overflow / Underflow & Clamping
Related to the fact integers are used, over- or underflow can occur very easily.\
Internally the colors can temporarily go up to `+1.5` / `-0.5` before they are clamped in the end.\
If you exceed this value, they will roll over giving you weird color artifacts.\
Clamping only occours at the very end, so even in 2-cycle mode, the first cycle does not clamp in the middle.

As a practical example, here we overlay two gradient textures by adding them together.\
The prim-color determines the strength. As we change it, we can observe overflow occurring:

<video width="700" controls loop muted>
   <source src="../../../../_static/img/cc_overflow.mp4" type="video/mp4">
</video>

As a graph, this looks like this:

```{image} /_static/img/cc_overflow_graph.png
:align: center
:width: 450px
```
The blue line is the result, the dotted green one the intermediate value.

To complicate things further, if a value is used in the `C`-slot of the CC,
it recieves a smaller overflow of `1.0` causing it to roll over earlier.\
This can only happen in a 2-cycle setup though,\
specifcally when going above `1.0` in the first cycle,\
and then using that result via `Combined` in the `C` slot of the second cycle.

The editors viewport will correctly preview both cases, so you see when it happens directly.

### Texture Issues
If you want to use two textures, 2-cylce mode must be enabled.\
Using the first texture in the second cycle also causes wrong pixels to be sampled.\
To be safe, only use TEX0/TEX1 in the first cycle, and only TEX1 in the second.

## Texture Inputs

If the CC uses a texture, you will see a UI to set settings for it.\
The RDP allows multiple textures to be loaded. Ignoring special cases, the CC can reference two of them at once.

On the RDP side, textures are handled by setting up so-called "Tiles".\
This is similar to a modern image-sampler that defines things like dimension, offset, repeat-mode and so on.

So while the triangles in a mesh contain UVs, they will go through the tiles logic to determine the final pixel to be sampled.
Tiles are always applied when sampling a texture, so all settings made there have no effect on performance.

Now for all the settings available: 
```{image} /_static/img/model_edit_tex.png
:align: center
:width: 350px
```
`Placeholder` lets you select how dynamic the texture should be.\
By default it is `None`, meaning the material fully sets it, and nothing can override it.\
Using `Tile` means the texture itself is still fixed in the material,\
but applying an offset is now possible dynamically.\
This can be used to scroll textures (even TEX0 and TEX1 differently) in objects.\
Setting it to `Texture + Tile` gives control to the object, and the material sets nothing.\
The use-case for this can be e.g.: texture-animations like blinking eyes.

`Size` defines the dimensions of the texture.\
This can only be changed if a placeholder is used.

`Offset` allows shifting the texture sampler aka texture-scrolling.\
Be aware that the minimum step size is `0.25`, and it overflows at `1024`.

`Scale` is factor in powers of two that can scale a texture up or down.\
This can be especially useful when combining two textures.\
For example, by having a grass texture, and multiplying a lower-res noise texture on top of it at a large scale.

`Repeat` determines the number of repetitions, if UVs exceed 1.0.\
The upper limit is `2048`, which can be set to repeat "infinitely."\
A value of `1.0` is equivalent to clamping.\
Note that any value inbetween, even fractions, are valid.

If `Mirror` is enabled, the texture will flip every other repetition.

All the settings below the texture can be set per axis, the left side is for the horizontal U axis, the right one for the vertical V axis.
(On the RDP those are also known as `ST` instead of `UV`).

## Sampling

This section defines how textures are sampled, which in contrast to tiles, is a global setting.  
```{image} /_static/img/model_edit_sampling.png
:align: center
:width: 350px
```

`Perspective` sets if persective correction should be applied to the texture.\
This is enabled by default, and unless you want a PS1 look, there is little reason to disable it.

`Dither` enables dithering, which can reduce color-banding if combined with de-dithering in the output settings.\
(See "Framebuffer -> Filter" in the scene-settings for that).\
The two types shown refer to color and alpha respectively, and can be set independently.

`Filtering` allows to turn texture filtering on or off.
Note that the N64 uses a very distinct 3-point filter, which gives a diamond shape.

As an example you can see different filtering here in this image:  
```{image} /_static/img/tex_filter.png
:align: center
:width: 400px
```

In the middle is the original texture (16x16 pixel).\
If you use point-filtering, it will continue to look exactly the same, just bigger.\
On the left would be a modern day bilinear filter, which gives a blurry result.\
One the right is what the N64 produces, which is almost the same, expect that one edge stays sharper.\
This is due to the fact only three pixels are used for interpolation.\
This detail will most like not matter, but can be intentionally used to reduce blurring.

## Values

If the CC uses a generic register, you can set the values here.\
If defined here, they are fixed for all objects that draw it, otherwise they can be set per-object.

```{image} /_static/img/model_edit_values.png
:align: center
:width: 400px
```

`Prim` / `Env` / `K4` / `K5` refer to the generic registers defined in the CC.\
Please be aware that registers stay unless something changes them.\
So if neither the material nor the instance actually sets them, but they are used in the CC,\
they will use whatever value was set last.\
This situation is often referred to as "Material bleed".


## Geometry Modes

The settings here affect how geometry is processed before it hits the RDP.\
This can influence the Transform and Lighting (T&L) stage of the tiny3d ucode.

```{image} /_static/img/model_edit_geo.png
:align: center
:width: 400px
```

`VertexFX`, enables one of the effects available in tiny3d.\
Most commonly you would use `Spherical UV` for a simple environment mapping effect, which uses the vertex normals to calculate UVs on the fly.\
The rest are most specialized options that may also not be fully supported in pyrite64 yet.

`Unlit`, by default vertex color is always affected by lighting.\
Enabling this only uses the vertex color and ignores any lighting.\
This can be useful for "glow" effects in lower lighting conditions, or for a more stylized look.

`Fog to Alpha` is required if you want to use fog in a scene.\
The N64 hardware has no fixed builtin fog support, instead it uses a blending setup.\
This will cause the color output to be interpolated towards a fog color based on the shading-alpha.\
Meaning that by default, translucent vertices in a mesh would now fade towards that color instead of being transparent.\
To actually get fog, something need to calulate and store the fog strength in the alpha channel.\
Enabling this setting does so, and uses the fog values defined in the draw-layer.
 
`Cull-Front` / `Cull-Back` enables face culling.\
By default `Cull-Back` is always active, only disable this if you need a double-sided material.

## Render Modes

Render-modes are global settings in the RDP affecting rendering and blending.

```{image} /_static/img/model_edit_render.png
:align: center
:width: 400px
```    

`Alpha-Clip`, defines a threshold for alpha values.\
If enabled, all pixels with alpha smaller than the defined value will get rejected.\
This can be useful to get cutout-materials without paying the cost for actual transparency and blending.\
Even with translucent materials, it can improve performance since (almost) fully transparent pixels are no longer drawn.

`Depth`, set if a depth compare and write to the depth buffer should be done.\
By default, this is never set, and whatever is defined in the draw-layer is used.\
However, in some cases it can be useful to temporarily override this here for specific materials.
For example, if you have a translucent shell around another mesh and want to avoid a depth-write.
This setting is correctly reverted after the material is done drawing.

`Anti-Alias`, allows setting AA to reduce aliasing / jagged edges.\
If enabled, please make sure to also enable AA in the framebuffer settings.\
(See "Framebuffer -> Filter" in the scene-settings for that).\
Enabling AA has a big impact on performance, so only use it when necessary.

`Blending`, allows overriding the blending mode.\
By default, the blending mode is set in the draw-layer, but you can override it here.\
This setting is correctly reverted after the material is done drawing.\
Using anything other than "None" will have an impact on performance, so only use it when necessary.

`Fog`, overrides fog.
Once again, this is defined in the draw-layer by default.\
Since just enabling fog on its own does not give any meaningful results, the draw-layer should always be used to enable it instead.\
However it can be useful to temporarily disable fog here for certain effects.\
Note that this only changes blending, you most likely also need to change "Fog to Alpha".\
This setting is correctly reverted after the material is done drawing.

`Fixed-Z`, allows using a fixed depth value.\
By default, depth values are taken and interpolated across the triangle that is drawn.\
You can however force a single fixed value to be used intead.\
Enabling this lets you specify that value, as well as the delta (change per pixel).\
This is a rather specific feature and you most likely never have to use it.
