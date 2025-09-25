cmake_minimum_required(VERSION 4.0)
project(p64editor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(SDL_SHARED OFF)
## set(SDL_STATIC ON)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O2")

add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
add_subdirectory(vendored/SDL_image EXCLUDE_FROM_ALL)

# Create your game executable target as usual
add_executable(p64editor src/main.cpp
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

        # editor
        src/editor/editorScene.h
        src/editor/editorScene.cpp
        src/renderer/shader.h
        src/renderer/shader.cpp
        src/renderer/vertBuffer.h
        src/renderer/vertBuffer.cpp
        src/renderer/vertex.h
        src/context.h
        src/project/project.h
        src/editor/editorMain.cpp
        src/renderer/texture.cpp
        src/renderer/texture.h
)

target_include_directories(p64editor PRIVATE vendored/imgui)

# Link to the actual SDL3 library.
target_link_libraries(p64editor PRIVATE SDL3::SDL3 SDL3_image::SDL3_image)
