Editor Windows
====================================

| This section covers the editor interface and every window you work with inside it.
| Once a project is open, the editor is a set of dockable windows arranged around a central 3D viewport.

Layout & Docking
~~~~~~~~~~~~~~~~~~

| Every panel is a dockable window. You can drag a window by its tab to re-dock it,
| split a region, or pull it out as a floating window. The default layout is:

- **Left**: Graph, Scene, Layers
- **Center**: 3D Viewport
- **Right**: Object, Asset, Model
- **Bottom**: Files, Log, ROM

| Use ``View`` > ``Reset Layout`` to return to this default arrangement at any time.

Menu Bar
~~~~~~~~

| The menu bar at the very top holds the global actions:

- **Project**: Save, open the :doc:`Project Settings <../launcher>` dialog, or close the project.
- **Edit**: Undo / Redo (with a description of the next step), and Preferences (``Ctrl+.``).
- **Build**: Build, Build & Run, or Clean the project.
- **View**: New Viewport, zoom the UI in/out, pick a Theme, or Reset Layout.

| On the far right is the green play button which builds and runs the game (``F12``).

Status Bar
~~~~~~~~~~

| The bar at the bottom shows the current FPS, the undo/redo history size and its memory
| usage, the CPU time per frame, and the editor version. If a newer editor version is
| available, an update button appears here.

Themes & Zoom
~~~~~~~~~~~~~~

| Themes are switched live under ``View`` > ``Theme`` and the choice is remembered.
| The whole UI can be scaled with ``View`` > ``Zoom In`` / ``Zoom Out`` for high-DPI displays.

The Windows
~~~~~~~~~~~

.. toctree::
  :maxdepth: 1

  windows/viewport
  windows/sceneGraph
  windows/objectInspector
  windows/sceneInspector
  windows/layerInspector
  windows/assetBrowser
  windows/assetInspector
  windows/log
  windows/rom
  windows/nodeEditor
  windows/modelEditor
