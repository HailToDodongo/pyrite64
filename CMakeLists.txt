cmake_minimum_required(VERSION 3.28)

project(pyrite64 VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are disabled.\n"
        "Run: cmake --preset <name>")
endif()

# set(SDL_SHARED ON)
# set(SDL_STATIC ON)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O2")
# debug, disable LTO
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-lto")

#SDL AND SDL_image
set(SDL_SHARED_DEFAULT OFF)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)

set(SDLIMAGE_BMP ON)
set(SDLIMAGE_GIF ON)
set(SDLIMAGE_JPG ON)
set(SDLIMAGE_PNG ON)
set(SDLIMAGE_SVG ON)

# Not needed, some of those also break windows builds due to missing ASM compilers
set(SDLIMAGE_AVIF OFF)
set(SDLIMAGE_JXL OFF)
set(SDLIMAGE_LBM OFF)
set(SDLIMAGE_PCX OFF)
set(SDLIMAGE_PNM OFF)
set(SDLIMAGE_QOI OFF)
set(SDLIMAGE_TGA OFF)
set(SDLIMAGE_TIF OFF)
set(SDLIMAGE_WEBP OFF)
set(SDLIMAGE_XCF OFF)
set(SDLIMAGE_XPM OFF)
set(SDLIMAGE_XV OFF)
add_subdirectory(vendored/SDL_image EXCLUDE_FROM_ALL)

#vendored libs
add_subdirectory(vendored/glm EXCLUDE_FROM_ALL)

set(VENDORED_INCLUDES
    vendored
    vendored/imgui
    vendored/IconFontCppHeaders
    vendored/SHA256/include
    vendored/ImGuiTextSelect
    vendored/ImViewGuizmo
    vendored/ImNodeFlow/include
    vendored/glm
    vendored/ImGuizmo
    vendored/tiny3d/tools/gltf_importer/src/lib
    vendored/libdragon
    n64
)

add_compile_definitions(IMGUI_USE_WCHAR32)


set(SOURCES_SHA256
    vendored/SHA256/src/SHA256.cpp
    vendored/SHA256/include/SHA256.h
)

set(SOURCES_GLTF
    vendored/tiny3d/tools/gltf_importer/src/parser.cpp
    vendored/tiny3d/tools/gltf_importer/src/writer.cpp
    vendored/tiny3d/tools/gltf_importer/src/parser/materialParser.cpp
    vendored/tiny3d/tools/gltf_importer/src/parser/boneParser.cpp
    vendored/tiny3d/tools/gltf_importer/src/parser/nodeParser.cpp
    vendored/tiny3d/tools/gltf_importer/src/optimizer/meshOptimizer.cpp
    vendored/tiny3d/tools/gltf_importer/src/optimizer/meshBVH.cpp
    vendored/tiny3d/tools/gltf_importer/src/parser/animParser.cpp
    vendored/tiny3d/tools/gltf_importer/src/converter/meshConverter.cpp
    vendored/tiny3d/tools/gltf_importer/src/converter/animConverter.cpp
    vendored/tiny3d/tools/gltf_importer/src/lib/lodepng.cpp
    vendored/tiny3d/tools/gltf_importer/src/lib/meshopt/allocator.cpp
    vendored/tiny3d/tools/gltf_importer/src/lib/meshopt/indexcodec.cpp
    vendored/tiny3d/tools/gltf_importer/src/lib/meshopt/indexgenerator.cpp
    vendored/tiny3d/tools/gltf_importer/src/lib/meshopt/simplifier.cpp
    vendored/tiny3d/tools/gltf_importer/src/lib/meshopt/stripifier.cpp
    vendored/tiny3d/tools/gltf_importer/src/lib/meshopt/spatialorder.cpp
    vendored/tiny3d/tools/gltf_importer/src/lib/meshopt/vcacheanalyzer.cpp
    vendored/tiny3d/tools/gltf_importer/src/lib/meshopt/vcacheoptimizer.cpp
    vendored/tiny3d/tools/gltf_importer/src/lib/tristrip/connectivity_graph.cpp
    vendored/tiny3d/tools/gltf_importer/src/lib/tristrip/policy.cpp
    vendored/tiny3d/tools/gltf_importer/src/lib/tristrip/tri_stripper.cpp
)

set(SOURCES_IMGUI
    vendored/imgui/imgui.cpp
    vendored/imgui/imgui_draw.cpp
    vendored/imgui/imgui_tables.cpp
    vendored/imgui/imgui_widgets.cpp
    vendored/imgui/misc/cpp/imgui_stdlib.cpp
    vendored/imgui/backends/imgui_impl_sdl3.cpp
    vendored/imgui/backends/imgui_impl_sdlgpu3.cpp
    vendored/ImGuizmo/ImGuizmo.cpp
    vendored/ImNodeFlow/src/ImNodeFlow.cpp
)

set(SOURCES_REGEX
    vendored/tiny-regex-c/re.c
)

file(GLOB_RECURSE SOURCES_PROJECT src/*.cpp src/*.h n64/*.h)

add_executable(pyrite64 src/main.cpp
    ${SOURCES_SHA256}
    ${SOURCES_GLTF}
    ${SOURCES_IMGUI}
    ${SOURCES_REGEX}
    ${SOURCES_PROJECT}
)

target_include_directories(pyrite64 PRIVATE ${VENDORED_INCLUDES})

# target_link_libraries(pyrite64 PRIVATE SDL3::SDL3)

target_compile_definitions(pyrite64 PRIVATE "PYRITE_VERSION=\"${PROJECT_VERSION}\"" IMGUI_DEFINE_MATH_OPERATORS GLM_FORCE_QUAT_DATA_XYZW)

set_target_properties(pyrite64 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")

# Link to the actual SDL3 library.
target_link_libraries(pyrite64 PRIVATE SDL3::SDL3 SDL3_image::SDL3_image glm::glm)

if(MINGW)
    set(APP_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/icon.rc")
    target_sources(pyrite64 PRIVATE ${APP_ICON_RESOURCE})

    target_link_options(pyrite64 PRIVATE
        -static-libgcc
        -static-libstdc++
        --static
    )
endif()
