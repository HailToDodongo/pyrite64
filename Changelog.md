# v0.3.0
- Editor - General
  - Auto-Save before build & run
  - Instantiating prefabs now places them in front of camera
  - Fix issue where adding new scripts could temp. mess up asset associations during build
  - Opus audio now working 
- Editor - Viewport:
  - Multi-Selection support:
    - Click and drag left mouse to multi-select objects
    - Hold "CTRL" to add to selection
    - Transform tools can be used on multiple objects at once
  - Focus objects by pressing "F"
  - Orbit objects by holding "ALT" and left-clicking
  - 3D axis-gizmo label and orientation fixes
- Editor - Log Window:
  - Buttons for clear / copy to clipboard / save to file
  - Properly strip ANSI codes
- Editor - Scene:
  - New scene setting for audio-mixer frequency (default: 32kHz)

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