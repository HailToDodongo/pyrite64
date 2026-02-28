# v0.4.0
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

# v0.3.0
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

# v0.2.0
- Toolchain-Manager:
  - fixed build failure if MSYS2 home path contains spaces
  - check for existing N64_INST / toolchain installation (windows)
  - auto-update old MSYS2 installations 
  - keep installer terminal open in case of errors 
- Editor:
  - Fix DPI scaling issues
  - Show error-popup if Vulkan is not supported 

# v0.1.0
Initial release
