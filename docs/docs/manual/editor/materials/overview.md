# Introduction

Before we go into any settings, let's first explain how the material system works in Pyrite64.

## Why are Materials needed?

### The Hardware

Rendering on the N64 is mostly happening on the RSP and RDP,\
which are two processors distinct from the CPU.\
It doesn't matter too much what exactly they are doing,\
but the important part is that they carry their own state.

This state contains things like the currently loaded textures(s),\
color registers, blending, if Anti-Alias is enabled or not and so on.

The CPU can then send commands to change this state over time.\
Once a draw command is sent, triangles will be drawn using the current settings.

As a consequence, a triangle draw command does not carry and define how it should be drawn.\
It will simply use whatever has been set before.\
For example, if it is a textured triangle, it uses whatever texture was loaded last.

### What Pyrite64 does

To offer a generic and easy to use system in pyrite64, this state management has to be organized.\
Which means defining who sets what, and at which point in time.\
To do so, pyrite64 abstracts all of this away into a few systems.

This will lead to some settings only being set once per frame, where nothing else touches it.\
Whereas others may set it on a per draw-layer, per 3D-model, or even per object basis.

You can visualize it as a hierarchy of settings, where each stage has to preserve what was set before.\
As an example this is a small section of settings different stages may set in a frame:

```{image} /_static/img/mat_state_overview.png
:align: center
:width: 550px
```

What we will focus on here are the last two stages, so objects and 3D models.\
Or in other words, anything you would expect to be bound to an 3D object in a scene.\
Those parts are managed by the material system.


## Material System

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
