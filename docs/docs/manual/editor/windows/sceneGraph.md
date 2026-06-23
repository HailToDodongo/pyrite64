# Graph (Scene Hierarchy)

The **Graph** window shows the object hierarchy of the currently loaded scene as a tree.
It is the main place to organise objects, parent them to each other, and manage the scene
structure. Selection is shared with the {doc}`3D Viewport <viewport>` and the
{doc}`Object <objectInspector>` inspector.

## The tree

Each row represents one object. The icon in front of the name hints at its role: a prefab
marker for prefab instances, the icon of its first component, or a generic cube / scene-root
icon when it has none.

Clicking a row selects the object. Selecting in the viewport highlights the matching row here,
and vice versa.

## Row controls

On the right of each row are two toggles:

| Toggle | Description |
|--------|-------------|
| **Selection** | Whether the object can be picked in the viewport. |
| **Enabled** | Whether the object (and its children) is active in the scene. |

## Editing the hierarchy

| Action | Result |
|--------|--------|
| **Double-click a name** | Rename the object inline. |
| **Drag a row onto another** | Re-parent the object under the target. |
| **Drag between rows** | Re-order, or insert at that position. |
| **Right-click a row** | Open the context menu. |

The context menu offers:

- **Add Object**: add a new empty child object.
- **To Prefab**: turn the object into a prefab asset (not shown for prefab instances).
- **Delete**: remove the object and its children.

All of these are recorded in the undo history.

## See also

- {doc}`Object <objectInspector>`: edit the selected object's properties and components.
- {doc}`Object Lifecycle <../objLifecycle>`: how objects are created and updated at runtime.
