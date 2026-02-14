cmake_minimum_required(VERSION 3.28)

project(pyrite64 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(SDL_SHARED ON)
# set(SDL_STATIC ON)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O2")
# debug, disable LTO
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-lto")

set(SDL_SHARED_DEFAULT OFF)
set(BUILD_SHARED_LIBS off)
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

add_subdirectory(vendored/glm EXCLUDE_FROM_ALL)

add_compile_definitions(IMGUI_USE_WCHAR32)

# Create your game executable target as usual
add_executable(pyrite64 src/main.cpp
        # SHA256
        vendored/SHA256/src/SHA256.cpp
        vendored/SHA256/include/SHA256.h

        # tiny3d gltf converter
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

        # ImGuizmo
        vendored/ImGuizmo/ImGuizmo.cpp

        # Imgui node editor
        vendored/ImNodeFlow/src/ImNodeFlow.cpp

        # Regex
        vendored/tiny-regex-c/re.c

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
        src/editor/undoRedo.h
        src/editor/undoRedo.cpp
        src/editor/selectionUtils.h
        src/editor/selectionUtils.cpp
        src/utils/json.h
        src/utils/colors.h
        src/editor/imgui/theme.h
        src/editor/imgui/theme.cpp
        src/editor/imgui/helper.h
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
        src/project/scene/sceneManager.h
        src/project/scene/sceneManager.cpp
        src/project/scene/scene.h
        src/project/scene/scene.cpp
        src/editor/pages/parts/sceneInspector.cpp
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
        src/utils/logger.h
        src/utils/logger.cpp
        src/editor/pages/parts/logWindow.cpp
        src/editor/pages/parts/logWindow.h
        src/editor/pages/parts/projectSettings.h
        src/editor/pages/parts/projectSettings.cpp
        src/editor/pages/parts/sceneGraph.h
        src/editor/pages/parts/sceneGraph.cpp
        src/project/scene/object.h
        src/project/scene/object.cpp
        src/editor/pages/parts/objectInspector.cpp
        src/utils/jsonBuilder.h
        src/project/component/components.h
        src/project/component/components.cpp
        src/project/component/types/compCode.cpp
        src/cli.cpp
        src/cli.h
        src/build/scriptBuilder.cpp
        n64/engine/include/script/scriptTable.h
        src/utils/codeParser.h
        src/utils/codeParser.cpp
        src/renderer/uniforms.h
        src/renderer/camera.h
        src/renderer/camera.cpp
        src/renderer/mesh.h
        src/renderer/mesh.cpp
        src/renderer/object.h
        src/renderer/object.cpp
        src/utils/meshGen.h
        src/utils/meshGen.cpp
        src/project/component/types/compModel.cpp
        src/renderer/pipeline.h
        src/renderer/pipeline.cpp
        src/utils/aabb.h
        src/utils/container.h
        src/renderer/n64Mesh.h
        src/renderer/n64Mesh.cpp
        src/renderer/n64/n64Material.h
        src/renderer/n64/n64Material.cpp
        src/renderer/n64/ccMapping.h
        src/project/component/types/compLight.cpp
        src/project/component/types/compCamera.cpp
        src/editor/imgui/helper.cpp
        src/editor/imgui/helper.cpp
        n64/engine/include/scene/objectFlags.h
        src/utils/fs.cpp
        src/project/component/types/compCollMesh.cpp
        src/project/assets/collision.h
        src/project/assets/collision.cpp
        src/build/t3dmBuilder.cpp
        src/build/collisionBuilder.cpp
        src/project/component/types/compCollBody.cpp
        src/build/fontBuilder.cpp
        src/utils/prop.h
        src/project/scene/prefab.h
        src/project/scene/prefab.cpp
        src/utils/hash.cpp
        src/utils/prop.cpp
        src/build/textureBuilder.cpp
        src/build/tools/bci.cpp
        src/build/tools/bci.h
        src/build/audioBuilder.cpp
        src/project/component/types/compAudio2d.cpp
        src/editor/pages/parts/layerInspector.cpp
        src/build/prefabBuilder.cpp
        src/project/component/types/compConstraint.cpp
        src/project/component/types/compCulling.cpp
        src/editor/pages/parts/nodeEditor.cpp
        src/project/component/shared/material.h
        src/project/component/shared/material.cpp
        src/project/component/shared/meshFilter.cpp
        src/project/graph/nodes/baseNode.h
        src/project/graph/nodes/nodeWait.h
        src/project/graph/graph.h
        src/project/graph/graph.cpp
        src/build/nodeGraphBuilder.cpp
        src/project/component/types/compNodeGraph.cpp
        src/project/component/types/compAnimModel.cpp
        src/editor/pages/parts/createProjectOverlay.cpp
        src/editor/globalActions.cpp
        src/editor/pages/parts/toolchainOverlay.cpp
        src/utils/toolchain.cpp
        src/editor/imgui/notification.cpp
)

target_include_directories(pyrite64 PRIVATE
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


# target_link_libraries(pyrite64 PRIVATE SDL3::SDL3)

target_compile_definitions(pyrite64 PRIVATE "PYRITE_VERSION=\"${PROJECT_VERSION}\"")
target_compile_definitions(pyrite64 PRIVATE IMGUI_DEFINE_MATH_OPERATORS GLM_FORCE_QUAT_DATA_XYZW)

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
