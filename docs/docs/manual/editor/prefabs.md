# Prefabs

A **prefab** is a reusable object template. You build an object once, turn it into a prefab,
and then place as many copies of it as you like. Each copy is called an **instance**.

An object on its own is just a container with a position. Its
{doc}`components <components>` are what actually give it a look and behaviour: a 3D model,
collision, a script, and so on. A prefab saves an object together with all of its components,
plus any child objects, so placing a prefab brings all of that back at once.

The strength of prefabs is the link between an instance and its source. When you change the
prefab, every instance updates to match. At the same time, each instance can override
individual values for itself, so copies can differ when needed while still sharing
everything else.

Prefabs are stored as assets and appear in the {doc}`Files <windows/assetBrowser>` browser.

## Creating a prefab

Right-click an object in the {doc}`Graph <windows/sceneGraph>` and choose **To Prefab**.
This saves the object, together with all of its components and child objects, as a new prefab
asset. The original object in the scene is automatically turned into an instance of that new
prefab, so it keeps its place in the scene but now follows the prefab from here on.

```{image} /_static/img/pf_create.png
:align: center
:width: 340px
```

## What a prefab can contain

A prefab is built from whatever the original object was, so it can range from a single object to
a whole hierarchy. The common combinations are:

- **Just an object.** A single object with its components, for example a coin or a pickup.
- **An object with children.** A small tree of objects that belong together, such as a cart with
  its wheels.
- **An object made of prefabs.** Its children can themselves be prefabs. A wall could place a few
  torch prefabs, reusing the torch instead of rebuilding it.
- **Nested prefabs.** A prefab can contain a prefab that itself contains another prefab, to any
  depth. A torch contains a flame, a room contains torches, and so on.
- **Any depth, freely mixed.** All of the above combine. Object trees can run many levels deep,
  and prefabs can sit at any level among plain objects.

Whichever shape a prefab has, it still behaves as a single unit: you place it as one instance,
and it stays linked to its source. The sections below explain how that link, and the per-instance
overrides, behave for each of these cases.

## Placing an instance

Drag a prefab from the {doc}`Files <windows/assetBrowser>` browser into the
{doc}`3D Viewport <windows/viewport>`. A new instance is spawned in front of the camera and
selected, ready to be positioned. You can place as many instances of the same prefab as you
want, and they will all stay linked to the same source.

<video width="640" controls loop muted>
   <source src="../../../../_static/img/pf_instance.mp4" type="video/mp4">
</video>

## Instances and their source

An instance does not store its own copy of the prefab's contents. It only remembers which
prefab it points to, plus any values you have overridden on it. Every other setting, including
all of the values inside its components, is pulled from the prefab.

Because of this:

- Changing the prefab updates every instance that has not overridden the changed value.
- The {doc}`Object <windows/objectInspector>` inspector shows which prefab an instance comes
  from, along with a button to edit the prefab source.

## Overriding values per instance

When an instance is selected, editing any value (its position, a setting inside one of its
components, a color, and so on) creates an **override**. This stores a per-instance copy of just
that one value on the instance. From then on, that value stops following the prefab, while every
setting you have not touched keeps being read from the prefab as before.

Overridden properties are marked in the inspector by having an unlocked lock icon before it. 

<video width="640" controls loop muted>
   <source src="../../../../_static/img/pf_instance_edit.mp4" type="video/mp4">
</video>

### Resetting an override

To drop an override and let the value follow the prefab again, right-click the value and choose
**Reset to prefab**. The lock icon next to a property does the same thing: toggling it off
removes the override and returns the value to the prefab's.

<video width="640" controls loop muted>
   <source src="../../../../_static/img/pf_instance_reset.mp4" type="video/mp4">
</video>

## Editing the prefab itself

Sometimes you want to change the prefab for all instances at once rather than override a single
copy. Select an instance and press the **Edit** button in the inspector's top section. This
enters prefab-edit mode, where your changes are written to the prefab source and therefore
apply to every instance.

While in this mode:

- The edited object is highlighted in red in the {doc}`Graph <windows/sceneGraph>`.
- A red **Exit Prefab Edit** button appears in the center of the top menu bar.
- Only the prefab's own objects can be selected and changed. Everything else in the scene is
  greyed out, so you cannot accidentally edit unrelated objects.

Press **Exit Prefab Edit** (or the **Back to Instance** button in the inspector) to leave the
mode and return to normal per-instance editing.

<video width="640" controls loop muted>
   <source src="../../../../_static/img/pf_prefab_edit.mp4" type="video/mp4">
</video>


## Nested objects

When a prefab has children or contains other prefabs (the last two cases above), those contents
appear beneath the instance in the {doc}`Graph <windows/sceneGraph>`. You can expand the instance
and click any nested object to select it, just like a normal object.

### Overriding nested objects

Selecting a nested object inside an instance lets you override its values for that instance
only, exactly like overriding the top-level instance. For example, if a "Torch" prefab contains
a "Flame", you can place several torches and give one of them a different flame color or
position, while the rest keep following the prefab. Resetting works the same way.

<video width="640" controls loop muted>
   <source src="../../../../_static/img/pf_nested_override.mp4" type="video/mp4">
</video>

### How editing reaches into nested prefabs

There is one rule worth understanding when a prefab contains other prefabs:

- A **placed instance** in your scene can override any value at any depth, including values deep
  inside the prefabs it contains. Those overrides only affect that one placement.
- When you are **editing a prefab** itself, you can change its own objects and decide how it
  places and configures the prefabs it contains, but you cannot reach inside one of those
  contained prefabs to change its internals. To change what is inside a contained prefab, edit
  that prefab directly.

This keeps each prefab responsible for its own contents, while still letting a final placement
in the scene fine-tune anything it needs.

<video width="640" controls loop muted>
   <source src="../../../../_static/img/pf_nested_edit.mp4" type="video/mp4">
</video>

## Unpacking an instance

If you no longer want an object to be linked to its prefab, right-click it in the
{doc}`Graph <windows/sceneGraph>` and choose **Unpack Prefab**. The instance becomes plain
scene objects with the prefab's current contents baked in, and it stops following the source.

Unpacking only affects that one level. Any prefabs nested inside it stay as instances and remain
linked to their own sources.

## Prefabs at Runtime

When a scene is being built for runtime use, all prefabs are resolved and baked.\
This means in the scene-graph, prefabs no longer exist at runtime, and there is no extra cost in using them.

### Spawning Prefabs

Prefabs definitions still exist and can be dynamically spawned at runtime though.\
To do so, call the `addObject` methond of the scene:

```c++
obj.getScene().addObject("Torch"_prefab, data->camTargetCur);
```

The value in the string is the 1:1 name of the prefab file.\
When trying to reference a prefab that doesn't exist, compilation will fail.

Note that nested prefabs spawn correctly,\
and transforms of children are relative to the main objects position.