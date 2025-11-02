cmake_minimum_required(VERSION 4.0)
project(p64editor)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(SDL_SHARED OFF)
## set(SDL_STATIC ON)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O2")

add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
add_subdirectory(vendored/SDL_image EXCLUDE_FROM_ALL)
add_subdirectory(vendored/glm EXCLUDE_FROM_ALL)

add_compile_definitions(IMGUI_USE_WCHAR32)

# Create your game executable target as usual
add_executable(p64editor src/main.cpp
        # JSON
        vendored/simdjson/simdjson.cpp
        vendored/simdjson/simdjson.h

        # SHA256
        vendored/SHA256/src/SHA256.cpp
        vendored/SHA256/include/SHA256.h

        # tiny3d gltf converter
        vendored/tiny3d/tools/gltf_importer/src/parser.cpp
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

        vendored/ImGuizmo/ImGuizmo.cpp

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
)

target_include_directories(p64editor PRIVATE
    vendored
    vendored/imgui
    vendored/simdjson
    vendored/IconFontCppHeaders
    vendored/SHA256/include
    vendored/ImGuiTextSelect
    vendored/ImViewGuizmo
    vendored/glm
    vendored/ImGuizmo
    vendored/tiny3d/tools/gltf_importer/src/lib
)

# Link to the actual SDL3 library.
target_link_libraries(p64editor PRIVATE SDL3::SDL3 SDL3_image::SDL3_image glm::glm)
