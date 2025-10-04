cmake_minimum_required(VERSION 4.0)
project(p64editor)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(SDL_SHARED OFF)
## set(SDL_STATIC ON)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O2")

add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
add_subdirectory(vendored/SDL_image EXCLUDE_FROM_ALL)

# Create your game executable target as usual
add_executable(p64editor src/main.cpp
        # JSON
        vendored/simdjson/simdjson.cpp
        vendored/simdjson/simdjson.h

        # SHA256
        vendored/SHA256/src/SHA256.cpp
        vendored/SHA256/include/SHA256.h

        # imgui
        vendored/imgui/backends/imgui_impl_sdl3.cpp
        vendored/imgui/backends/imgui_impl_sdl3.h
        vendored/imgui/backends/imgui_impl_sdlgpu3.cpp
        vendored/imgui/backends/imgui_impl_sdlgpu3.h
        vendored/imgui/imgui.h
        vendored/imgui/imgui.cpp
        vendored/imgui/imgui_draw.cpp
        vendored/imgui/imgui_tables.cpp
        vendored/imgui/imgui_widgets.cpp
        vendored/imgui/misc/cpp/imgui_stdlib.cpp
        vendored/imgui/misc/cpp/imgui_stdlib.h

        # editor
        src/editor/pages/editorScene.h
        src/editor/pages/editorScene.cpp
        src/renderer/shader.h
        src/renderer/shader.cpp
        src/renderer/vertBuffer.h
        src/renderer/vertBuffer.cpp
        src/renderer/vertex.h
        src/context.h
        src/project/project.h
        src/editor/pages/editorMain.cpp
        src/renderer/texture.cpp
        src/renderer/texture.h
        src/project/project.cpp
        src/editor/actions.h
        src/editor/actions.cpp
        src/utils/json.h
        src/editor/imgui/theme.h
        src/editor/imgui/theme.cpp
        src/editor/imgui/helper.h
        vendored/IconFontCppHeaders/IconsFontAwesome4.h
        src/utils/filePicker.h
        src/utils/filePicker.cpp
        src/editor/pages/parts/viewport3D.h
        src/editor/pages/parts/viewport3D.cpp
        src/renderer/scene.h
        src/renderer/scene.cpp
        src/renderer/framebuffer.h
        src/renderer/framebuffer.cpp
        src/editor/pages/parts/assetsBrowser.h
        src/editor/pages/parts/assetsBrowser.cpp
        src/project/assetManager.h
        src/project/assetManager.cpp
        src/utils/hash.h
        src/editor/pages/parts/assetInspector.cpp
        src/editor/pages/parts/assetInspector.h
        src/utils/textureFormats.h
        src/editor/pages/parts/sceneBrowser.cpp
        src/project/scene/sceneManager.h
        src/project/scene/sceneManager.cpp
        src/project/scene/scene.h
        src/project/scene/scene.cpp
        src/editor/pages/parts/sceneInspector.cpp
        src/utils/color.h
        src/build/projectBuilder.h
        src/build/projectBuilder.cpp
        src/utils/fs.h
        src/utils/string.h
        src/utils/proc.h
        src/utils/proc.cpp
        src/build/sceneBuilder.cpp
        src/utils/binaryFile.h
        src/build/sceneContext.h
        src/build/stringTable.h
)

target_include_directories(p64editor PRIVATE
    vendored/imgui
    vendored/simdjson
    vendored/IconFontCppHeaders
    vendored/SHA256/include
)

# Link to the actual SDL3 library.
target_link_libraries(p64editor PRIVATE SDL3::SDL3 SDL3_image::SDL3_image)
