# Materials

This page explains how materials in Pyrite64 work and how to use them in your projects.\
Materials here refer to any draw settings (e.g.: colors, textures) used by 3D models. 

By default, 3D models will already contain some material information from fast64,\
which pyrite64 uses as a default.
The editor, and to some extent the runtime C++ API,\
allows you to further edit the materials.

For authoring 3D models itself, please check out the [Assets](../assets.rst) documentation.

## Material System

Before we go into any settings, let's first explain how the material system works in Pyrite64.

Materials come in two forms: a "Material" and a "Material Instance".\
The former is attached to a model, is immutable at runtime, and only exists once.\
A "Material Instance" is something that exists per component/object, is mutable at runtime,\
and allows for individual overrides of the material settings.

As an example, let's say you have a model of a box where you can control the color by changing the "Primitive Color" register.\
And in your scene you have three objects that draw said mesh.\
This may look like this in the editor and in-game:

```{image} /_static/img/mat_editor_intro.png
:align: center
```
On the left we can see three objects, all using the same model.\
The object settings on the right show an option to override, in this case "Prim Color".

At runtime the data looks something like this:

```{image} /_static/img/mat_example_data.png
:align: center
:width: 340px
```
The actual 3D-mesh and material definition only exists once, and is referenced by the object.\
Whereas the instance data is stored per object and sets additional data.

When it comes to rendering, the sequence of draw calls may look like this (simplified): 

```{image} /_static/img/mat_draw_flow.png
:align: center
:width: 340px
```

Each object is drawn one after another, first performing the object-specific calls,\
followed by the data from the model itself.

In the simplest case, this means first setting the matrix of the object, then the material instance.\
Afterward, the models material is applied and then the actual mesh data is drawn.

As a consequence, anything directly set in the material will be the same across all objects using it.
For example, if the material here would be setting prim-color, the object would never get a chance to override it.\
Note that a model can have multiple materials, so even doing it afterward would not always work.

This leaves two options to allow dynamic setting: not setting it, or embedding placeholders.
Depending on the setting, one or the other is used to do so.

Note that you have the choice between allowing an override or not, since either option is valid depending on the use-case.

## Materials

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

### Material Settings

Now for a list of all available settings in the material.

#### Color Combiner

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

##### Fixed-Point issues
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

##### Overflow / Underflow & Clamping
Related to the fact integers are used, over- or underflow can occur very easily.\
Internally the colors can temporarily go up to `+1.5` / `-0.5` before they are clamped in the end.\
If you exceed this value, they will roll over giving you weird color artifacts.\
Clamping only occours at the very end, so even in 2-cycle mode, the first cycle does not clamp in the middle.

As a practical example, here we overlay two gradient textures by adding them together.\
The prim-color determines the strength. As we change it, we can observe overflow occurring:

<video width="700" controls loop muted>
   <source src="/_static/img/cc_overflow.mp4" type="video/mp4">
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

##### Texture Issues
If you want to use two textures, 2-cylce mode must be enabled.\
Using the first texture in the second cycle also causes wrong pixels to be sampled.\
To be totally safe, try to only use textures in the first cycle.

#### Texture Inputs

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

If `Mirror` is enabled, the texture will flip every other repetition.\

All the settings below the texture can be set per axis, the left side is for the horizontal U axis, the right one for the vertical V axis.
(On the RDP those are also known as `ST` instead of `UV`).