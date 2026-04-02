# Changelog

## v0.7.0

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
