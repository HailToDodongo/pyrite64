# Changelog

## v0.8.0

### Node-Graph
* Node Graph Rewrite + User defined nodes by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/256
  * Node Docs: https://hailtododongo.github.io/pyrite64/docs/manual/script/nodeGraph.html
  * Custom Node Docs: https://hailtododongo.github.io/pyrite64/docs/manual/script/nodeGraphCustom.html

### Features & Engine changes
* Prefab support for nested objects / other prefabs by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/268
  * Docs: https://hailtododongo.github.io/pyrite64/docs/manual/editor/prefabs.html

### Physics System
* Physics API: Add swept-sphere function by @Aaronmac64 in https://github.com/HailToDodongo/pyrite64/pull/276
* improved collision system heap traffic, introduced better bookkeeping… by @Byterset in https://github.com/HailToDodongo/pyrite64/pull/288
* big collision optimizations by @Byterset in https://github.com/HailToDodongo/pyrite64/pull/293
* Auto fit colliders by @MoisesMlg in https://github.com/HailToDodongo/pyrite64/pull/295
* Change VEC3_FORWARD direction in math.h by @Byterset in https://github.com/HailToDodongo/pyrite64/pull/300
* fix: let rigidbodies adopt changes from externally changed transforms, prevent visual interpolation leaking into physics by @Byterset in https://github.com/HailToDodongo/pyrite64/pull/301
* feat: Allow to set the collider colour in the preferences by @MoisesMlg in https://github.com/HailToDodongo/pyrite64/pull/302
* correctly scale the velocity of the rigidbody by the graphicsscale wh… by @Byterset in https://github.com/HailToDodongo/pyrite64/pull/303

### Editor
* Editor Themes (dark, light, high-contrast, red & blue dark) by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/261
* Save and check editor version in Project by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/258
* Editor for ROM Metadata + Homebrew-Header by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/259
* Remember and show recently opened Projects by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/263
* Multiple Viewports + Game-Camera preview by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/264
* Fix not opening Ares emulator after the second usage of "Build & Run" by @MoisesMlg in https://github.com/HailToDodongo/pyrite64/pull/269
* Allow to drag&drop a Script to add a Code component by @MoisesMlg in https://github.com/HailToDodongo/pyrite64/pull/272
* Allow to open the selected Script from the Code component by @MoisesMlg in https://github.com/HailToDodongo/pyrite64/pull/273
* Project setting to disable Debug-Menu hotkey by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/275
* Fixed editing parent transforms by @MoisesMlg in https://github.com/HailToDodongo/pyrite64/pull/274
* Allow to reorder the object components by @MoisesMlg in https://github.com/HailToDodongo/pyrite64/pull/277
* Implement proper wayland session restoration by @Lilaa3 in https://github.com/HailToDodongo/pyrite64/pull/279
* Add viewport lock mode for touchpad/laptop navigation by @LlanerasJ in https://github.com/HailToDodongo/pyrite64/pull/278
* toolchain: Show N64_INST location on Linux to help validate the environment by @thekovic in https://github.com/HailToDodongo/pyrite64/pull/286
* Add audio preview for sound assets in the asset inspector by @LlanerasJ in https://github.com/HailToDodongo/pyrite64/pull/285
* Change UI Pos/Rot/Scale order to make it consistent by @MoisesMlg in https://github.com/HailToDodongo/pyrite64/pull/291
* Fix quote emulator and ROM paths so paths with spaces work by @LlanerasJ in https://github.com/HailToDodongo/pyrite64/pull/290
* Add projection settings camera component (Ortho / Perspective) by @Byterset in https://github.com/HailToDodongo/pyrite64/pull/304

### Docs & Build
* Mention MacOS by @seacat17 in https://github.com/HailToDodongo/pyrite64/pull/254
* Fix typo in docs by @seacat17 in https://github.com/HailToDodongo/pyrite64/pull/260

## v0.7.0


### Material System
* New Material System / Placeholder and dynamic textures by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/179
* Additive-Light option in Draw-Layer / Fix point lights by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/226

 Docs: https://hailtododongo.github.io/pyrite64/docs/manual/editor/materials.html

### Physics System
* New Physics-System + Rigid-Body simulation by @Byterset in https://github.com/HailToDodongo/pyrite64/pull/207
  * Make applyForceAtPoint actually accumulate force by @andmatand in https://github.com/HailToDodongo/pyrite64/pull/225
  * Decouple physics scale from visual scale by @andmatand in https://github.com/HailToDodongo/pyrite64/pull/228
  * Add an applyLinearForce method by @andmatand in https://github.com/HailToDodongo/pyrite64/pull/231
  * Don't change velocity on hit during swept substep detection by @andmatand in https://github.com/HailToDodongo/pyrite64/pull/232
  * Add a localCenterOfMassOffset property to RigidBody by @andmatand in https://github.com/HailToDodongo/pyrite64/pull/233
  * Add method for creating custom collision mesh by @andmatand in https://github.com/HailToDodongo/pyrite64/pull/234 
  * Fix raycast box hit_face tracking and report origin when starting inside by @andmatand in https://github.com/HailToDodongo/pyrite64/pull/227
  * Allow a MeshCollider to have no owner by @andmatand in https://github.com/HailToDodongo/pyrite64/pull/238
  * Add an AABB tree of MeshColliders to speed up several checks by @andmatand in https://github.com/HailToDodongo/pyrite64/pull/239

* Character Body Component by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/247
  * Docs: https://hailtododongo.github.io/pyrite64/docs/manual/editor/collision/characterBody.html

### Features & Engine changes
* New Billboard constraint type (Y or XYZ)  by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/200
* Support for .XM audio files by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/202
* New Debug Overlay + Allow User defined entries by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/213
* enable objectRefs, prefabs, and sprite asset refs to be instanced when using a prefab in a scene by @RetroZelda in https://github.com/HailToDodongo/pyrite64/pull/215
* object will recursivly remove children by default by @RetroZelda in https://github.com/HailToDodongo/pyrite64/pull/217
* Add ability to temporarily disable a RigidBody by @andmatand in https://github.com/HailToDodongo/pyrite64/pull/242
* Added check to clamp diagonal movement input length by @Aaronmac64 in https://github.com/HailToDodongo/pyrite64/pull/243
* Add ability to toggle visibility of Object by @andmatand in https://github.com/HailToDodongo/pyrite64/pull/244
* Bitmask support in C++ Script props by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/251
* Refactor Object Events / Lifecycle + Docs by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/223
  * Docs: https://hailtododongo.github.io/pyrite64/docs/manual/editor/objLifecycle.html

### Editor
* Enable setting Prefabs in the editor / code-component arguments by @RetroZelda in https://github.com/HailToDodongo/pyrite64/pull/210
* Disallow dropping an object into a descendant by @MoisesMlg in https://github.com/HailToDodongo/pyrite64/pull/219
* Move selected object to the position of the 3D viewport camera by @MoisesMlg in https://github.com/HailToDodongo/pyrite64/pull/224
* Allow to edit the name of an object on the scene tree by @MoisesMlg in https://github.com/HailToDodongo/pyrite64/pull/218
* Show per-row icons in scene-graph hierarchy, remove lines for cool clean UI by @Prazon in https://github.com/HailToDodongo/pyrite64/pull/237
* Fixes the start position of the TextBox to edit a tree node by @MoisesMlg in https://github.com/HailToDodongo/pyrite64/pull/240
* Consistent AABB rendering across component types by @floridaman in https://github.com/HailToDodongo/pyrite64/pull/241
* 3D Model Thumbnails / Preview in asset browser by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/249
* Allow to filter in the "Script" ComboBox of the "Code" component by @MoisesMlg in https://github.com/HailToDodongo/pyrite64/pull/245
* Remove Object ID in editor by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/248
* Adding option to modify camera move speed while in flycam mode in viewport by @DekeDev in https://github.com/HailToDodongo/pyrite64/pull/204

### Docs & Build
* Docs: instruct MYSYS2 users install UCRT Python3 by @JordanMajd in https://github.com/HailToDodongo/pyrite64/pull/208
* fix: update libdragon for Linux build by @cyphercodes in https://github.com/HailToDodongo/pyrite64/pull/230
* Linux AppImage build by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/250

## v0.6.0

- Editor zoom (by default: Ctrl + Scroll) by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/159
- Editor: Add anti-alias option by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/161
- Editor: Add VSync option and custom FPS Limit by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/162
- Rotation can be edited as euler angles, scaling can be proportionally edited
  - Fixes value jumps when editing rotation as euler angles by @Q-Bert-Reynolds in https://github.com/HailToDodongo/pyrite64/pull/171
  - Replaces uniform scale with proportional scale by @Q-Bert-Reynolds in https://github.com/HailToDodongo/pyrite64/pull/174
- mingw: Add useful toolchain location log by @thekovic in https://github.com/HailToDodongo/pyrite64/pull/173
- defer the object initialization that might use a later object reference (fixes #177) by @Byterset in https://github.com/HailToDodongo/pyrite64/pull/178
- Create global-scripts and node-graphs from the asset-browser by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/180
- Fix scene name not updating in the scene browser by @MessyComposer in https://github.com/HailToDodongo/pyrite64/pull/191
- Delete scenes via right-click context menu by @MessyComposer in https://github.com/HailToDodongo/pyrite64/pull/192

## v0.5.0

- Migrated documentation to sphinx, added new docs
  - Available online here  https://hailtododongo.github.io/pyrite64/index.html
- fix: keymap was only getting applied if it failed to load from file by @Q-Bert-Reynolds in https://github.com/HailToDodongo/pyrite64/pull/126
- Better handling of unsaved assets and settings, ask before closing node-graph by @Byterset in https://github.com/HailToDodongo/pyrite64/pull/111
- Trackpad pan and orbit by @Q-Bert-Reynolds in https://github.com/HailToDodongo/pyrite64/pull/124
- fix: handle dragging objects into/near the root node in the scene-graph by @Byterset in https://github.com/HailToDodongo/pyrite64/pull/131
- Collision mesh now generated on demand, remove "Collision" asset setting by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/139
- editor: Lose focus from InputText when using 3D viewport by @thekovic in https://github.com/HailToDodongo/pyrite64/pull/137
- Transforming Object now trans. attached camera + new option to toggle this behaviour by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/142
- Asset browser context popup by @Q-Bert-Reynolds in https://github.com/HailToDodongo/pyrite64/pull/132
  - Right click on assets to show more options (open file, open directory, rename, delete)
- Project wide shortcuts now use reassignable keymap by @Q-Bert-Reynolds in https://github.com/HailToDodongo/pyrite64/pull/140
- Refactor C++ Scripts to have separate init / destroy function + add scripting docs by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/145
- editor: Fix 'Show in Explorer' not opening correct folder sometimes by @thekovic in https://github.com/HailToDodongo/pyrite64/pull/150
- Remember window layout between sessions by @Q-Bert-Reynolds in https://github.com/HailToDodongo/pyrite64/pull/148
- Remember window size and position across sessions by @Q-Bert-Reynolds in https://github.com/HailToDodongo/pyrite64/pull/147
- Handle WSL in Asset Browser by @Byterset in https://github.com/HailToDodongo/pyrite64/pull/154
- Open application directly from project file on Linux by @Q-Bert-Reynolds in https://github.com/HailToDodongo/pyrite64/pull/146
- Viewport navigation preferences, fixes Linux pinch zoom by @Q-Bert-Reynolds in https://github.com/HailToDodongo/pyrite64/pull/156
- Add Tooltips to Toggle Buttons by @Byterset in https://github.com/HailToDodongo/pyrite64/pull/157
- Check for Pyrite64 updates + Link to new release page by @HailToDodongo in https://github.com/HailToDodongo/pyrite64/pull/158
- Show error if assets have duplicate UUIDs.
- Show error model-components have missing models.
- Fix temp. broken model references at runtime in prefabs during asset changes

**Full Changelog**: https://github.com/HailToDodongo/pyrite64/compare/v0.4.0...v0.5.0

```{admonition} This version introduced breaking changes!
:class: warning

Checkout [Breaking Changes](./breakingChanges) for more information.
```

## v0.4.0
- MacOS / Metal support (by [rasky](https://github.com/rasky), #106)
- Runtime-Engine
  - Fix rendering issue where the camera region / scissor is not set for all draw-layers
- Editor - General
  - Fix clean-build under windows
  - Automatically force a clean-build if engine code changed
  - Configurable keybindings and editor preferences (by [@Q-Bert-Reynolds](https://www.github.com/Q-Bert-Reynolds), #95)
  - Fix issue where snapping during scaling would collapse objects to zero size
  - Improved performance of asset browser  
- Toolchain manager:
  - Existing installations can now be updated too (by [@thekovic](https://www.github.com/thekovic), #11)
- CLI
  - New command to clean a project (`--cmd clean`)

## v0.3.0
- Editor - General
  - Auto-Save before build & run
  - Instantiating prefabs now places them in front of camera
  - Fix issue where adding new scripts could temp. mess up asset associations during build
  - Opus audio now working
  - New ROM Inspector for asset sizes (by [@proverbiallemon](https://www.github.com/proverbiallemon), #100)
  - Show Project state in title and ask before exiting with unsaved changes (by  [@Byterset](https://www.github.com/Byterset), #103)
- Editor - Viewport:
  - Multi-Selection support: (by  [@Byterset](https://www.github.com/Byterset), #7)
    - Click and drag left mouse to multi-select objects
    - Hold "CTRL" to add to selection
    - Transform tools can be used on multiple objects at once
  - Camera improvements: (by [@Q-Bert-Reynolds](https://www.github.com/Q-Bert-Reynolds), #40)
    - Focus objects by pressing "F" 
    - Orbit objects by holding "ALT" and left-clicking
    - 3D axis-gizmo label and orientation fixes
- Editor - Log Window:
  - Buttons for clear / copy to clipboard / save to file
  - Properly strip ANSI codes
- Editor - Scene:
  - New scene setting for audio-mixer frequency (default: 32kHz)
- Model Converter (tiny3d):
  - fix issue where multiple animations with partially matching names could lead to them being ignored
- Various toolchain and build-setup improvements (by [@thekovic](https://www.github.com/thekovic))

## v0.2.0
- Toolchain-Manager:
  - fixed build failure if MSYS2 home path contains spaces
  - check for existing N64_INST / toolchain installation (windows)
  - auto-update old MSYS2 installations 
  - keep installer terminal open in case of errors 
- Editor:
  - Fix DPI scaling issues
  - Show error-popup if Vulkan is not supported 

## v0.1.0
Initial release
