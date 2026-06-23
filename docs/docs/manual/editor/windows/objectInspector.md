# Object

The **Object** inspector shows the properties of the currently selected object: its name,
transform, and the list of attached components. It updates to follow whatever is selected in
the {doc}`Graph <sceneGraph>` or {doc}`3D Viewport <viewport>`.

When nothing is selected it simply reads "No Object selected".

## General

The top section holds the object **Name**. For prefab instances it also shows the prefab it
comes from, with a button to switch between editing the instance and editing the prefab source
(see the Prefabs section below).

## Transform

The transform section edits the object's **Position**, **Rotation** and **Scale**. Values are
relative to the parent object. Editing a value here is equivalent to using the viewport gizmo,
and is recorded in the undo history.

## Components

Below the transform, each attached component is drawn in its own collapsible section with all of
its options. Components are what actually give an object behaviour or appearance (a model, a
light, a collider, a script, and so on).

See the {doc}`Components <../components>` section for a page on every component type and its
options.

## Multi-selection

When several objects are selected, the inspector switches to a multi-edit mode. It shows how
many objects are selected and lets you edit the shared **Name** and **Transform** fields at
once. Fields whose values differ across the selection are shown as mixed until you overwrite
them.

## Prefabs

For a prefab instance, the inspector works on the instance's overrides by default. Use the
**Edit** button in the General section to switch into editing the underlying prefab; changes
then apply to every instance of that prefab. Switch back to return to per-instance editing.

## See also

- {doc}`Components <../components>`: reference for every component and its options.
- {doc}`Graph <sceneGraph>`: select and organise the objects shown here.
