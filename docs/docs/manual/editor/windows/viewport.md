# 3D Viewport

The 3D Viewport is the central window where you see and edit the scene. It renders the
currently loaded scene in real time, lets you navigate with a free-fly camera, and is where
objects are selected and transformed.

## Navigation

The camera is a free-fly / orbit camera. Movement only applies to the viewport your mouse
started the drag in, so multiple viewports never move in lockstep.

| Action | Control |
|--------|---------|
| **Look around (fly)** | Hold right mouse and move |
| **Move while flying** | Hold a mouse button and use `W` `A` `S` `D`, with `E` / `Q` for up / down |
| **Orbit** | `Alt` + left mouse drag |
| **Pan** | Middle mouse drag |
| **Zoom** | Mouse wheel (or two-finger scroll) |
| **Faster movement** | Hold `Shift` while moving |
| **Adjust fly speed** | Hold right mouse and scroll (when enabled in Preferences) |
| **Focus selection** | Frame the selected object(s) with the focus key |

The movement and gizmo keys depend on the active keymap preset (Blender by default) and can be
remapped in {doc}`Preferences <../../launcher>`. The current fly speed multiplier is shown on
the right of the toolbar.

### Orientation cube

The cube in the top-right corner shows the current view orientation. Click an axis to snap
the camera to that side, or drag the cube to rotate the view. Each viewport has its own cube;
it is hidden while a viewport is locked to a scene camera.

## Selecting objects

- **Click** an object to select it.
- **Click-drag** on empty space to box-select multiple objects.
- Hold **`Ctrl`** while clicking or box-selecting to add to the current selection.

Selection is shared across all viewports and with the {doc}`Graph <sceneGraph>` window.
Only objects flagged as selectable can be picked.

## Transform gizmo

When an object is selected, a transform gizmo is drawn on it. The mode is chosen from the
toolbar (or with the gizmo keys):

| Tool | Description |
|------|-------------|
| **Translate** | Move the object along an axis or plane. |
| **Rotate** | Rotate around an axis. |
| **Scale** | Scale along an axis or uniformly. |
| **World / Local** | Toggle whether the gizmo aligns to world or to the object's local axes. |

Holding `Ctrl` while dragging snaps to fixed increments. Holding `Shift` transforms only the
object itself without carrying its children along. Each viewport keeps its own gizmo state.

## Toolbar

The toolbar across the top of the viewport holds, from left to right:

| Control | Description |
|---------|-------------|
| **Translate / Rotate / Scale** | The active transform gizmo mode. |
| **World / Local** | Gizmo space toggle. |
| **Grid** | Show or hide the ground grid. |
| **Collision Mesh** | Show or hide collision-mesh wireframes. |
| **Collision Bodies** | Show or hide collider shapes. |
| **Icons** | Show or hide the object billboards (camera, light, audio and generic object icons). |
| **Camera** | Mirror the view onto a scene camera (see below). |
| **Resolution** | Switch between free and camera resolution (see below). |
| **Cam Speed** | The current fly-speed multiplier. |

These overlay toggles are local to each viewport, so different viewports can show different
helpers at the same time.

## Placing prefabs

Drag a prefab asset from the {doc}`Files <assetBrowser>` window into the viewport to spawn an
instance. It is placed in front of the camera and selected so you can position it right away.

## Camera preview

A viewport can mirror a {doc}`Camera <../components/camera>` from the scene to preview exactly
what that camera frames.

- Pick a camera from the **Camera** dropdown in the toolbar (or "Free Camera" to fly freely).
- While locked, the view continuously matches the camera object's position, rotation, FOV and
  aspect ratio. Free-fly controls and the orientation cube are disabled.
- The bound camera's own icon and frustum are hidden in that viewport so they don't block the view.

### Resolution mode

The **Resolution** toggle (enabled once a camera is bound) chooses how the preview is rendered:

| Mode | Description |
|------|-------------|
| **Free** | Renders at the viewport's own pixel density, letterboxed to the camera's aspect ratio. |
| **Camera** | Renders at the camera's native resolution (for example 320x240) for an authentic, low-res preview. |

In **Camera** resolution mode the viewport becomes a clean preview: the grid, collision
overlays and icons are all forced off (and their toolbar toggles greyed out) so you see only
the rendered image, just like the game would output it.

## Multiple viewports

You can open any number of viewports, each with its own camera, overlay toggles and camera
binding.

- Open a new one with `View` > `New Viewport`, or by right-clicking a viewport tab and
  choosing **New Viewport**.
- Close a viewport with the `x` on its tab, like any other window.
- Open viewports are remembered between sessions; the default layout always has one.
- `View` > `Reset Layout` collapses back to a single default viewport.

A typical use is to keep one free-fly viewport for editing and a second one locked to a
gameplay camera in camera-resolution mode to preview the final framing.

## See also

- {doc}`Camera component <../components/camera>`: the scene camera that a viewport can mirror.
- {doc}`Graph <sceneGraph>`: the scene hierarchy, which shares the selection with the viewport.
- {doc}`Object <objectInspector>`: edit the selected object's transform and components.
