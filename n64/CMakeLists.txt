#################################################################################
# NOTE:
# This is a dummy file to make CLion happy when editing the engine code.
# Actual compilation happens via Makefiles / Pyrite64's build system.
#
# The (actually used) cmake file for the editor itself is in the main directory.
#
#################################################################################

cmake_minimum_required(VERSION 3.30)
project(n64_engine)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)

include_directories(~/Documents/projects/tiny3d/src)
include_directories(~/Documents/projects/libdragon/include)

include_directories(engine/include)

add_executable(n64_engine
        engine/include/lib/fifo.h
        engine/include/lib/logger.h
        engine/include/lib/memory.h
        engine/include/scene/scene.h
        engine/include/vi/swapChain.h
        engine/src/lib/memory.cpp
        engine/src/scene/scene.cpp
        engine/src/vi/swapChain.cpp
        engine/src/main.cpp
        engine/src/scene/camera.cpp
        engine/src/scene/sceneManager.cpp

        engine/src/audio/audioManager.cpp
        engine/src/scene/sceneLoader.cpp
        engine/include/scene/object.h
        engine/include/scene/components/code.h
        engine/include/scene/componentTable.h

        engine/include/assets/assetManager.h
        engine/src/assets/assetManager.cpp
        engine/include/assets/assetTypes.h
        engine/src/scene/object.cpp
        engine/include/scene/components/light.h
        engine/src/scene/lighting.cpp
        engine/include/scene/components/camera.h
        engine/include/lib/math.h
        engine/src/debug/debugDraw.cpp
        engine/include/lib/ringBuffer.h
        engine/src/vi/vi.cpp
        engine/include/script/userScript.h
        engine/include/script/globalScript.h
        engine/include/scene/event.h
        engine/src/scene/components/collMesh.cpp
        engine/src/libdragon/rspq.h
        engine/include/renderer/drawLayer.h
        engine/src/renderer/drawLayer.cpp
        engine/src/renderer/pipelineDefault.cpp
        engine/src/renderer/pipelineHDRBloom.cpp
        engine/include/renderer/pipeline.h
        engine/include/renderer/pipelineHDRBloom.h
        engine/src/libdragon/utils.h
        engine/src/libdragon/utils.cpp
        engine/include/lib/mips.h
        engine/src/renderer/pipelineBigTex.cpp
        engine/src/scene/components/model.cpp
        engine/src/renderer/bigtex/bigtex.h
        engine/src/renderer/bigtex/bigtex.cpp
        engine/src/renderer/bigtex/textures.cpp
        engine/src/renderer/bigtex/uvTexture.cpp
        engine/src/renderer/bigtex/memory.cpp
        engine/src/renderer/bigtex/fbCpuDraw.cpp
        engine/src/renderer/bigtex/rspBigTex.cpp
        engine/include/scene/components/audio2d.h
        engine/src/scene/components/audio2d.cpp
        engine/src/scene/components/collBody.cpp
        engine/include/collision/flags.h
        engine/src/renderer/hdr/rspHDR.cpp
        engine/include/renderer/particles/ptxSprites.h
        engine/src/renderer/particles/ptxSystem.cpp
        engine/src/renderer/particles/ptxSprites.cpp
        engine/src/collision/resolver.cpp
)
