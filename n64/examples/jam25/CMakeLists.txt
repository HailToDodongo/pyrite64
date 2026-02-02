#################################################################################
# NOTE:
# This is a dummy file to make CLion happy when editing the engine code.
# Actual compilation happens via Makefiles / Pyrite64's build system.
#
# The (actually used) cmake file for the editor itself is in the main directory.
#
#################################################################################

cmake_minimum_required(VERSION 3.30)
project(gj25)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)

include_directories(~/Documents/projects/pyrite64/vendored/tiny3d/src)
include_directories(~/Documents/projects/pyrite64/vendored/libdragon/include)
include_directories(~/Documents/projects/pyrite64/n64/engine/include)

add_executable(gj25
  src/user/ObjBot.cpp
        src/user/globalSetup.cpp
        src/user/systems/marker.h
        src/user/systems/marker.cpp
        src/user/systems/context.h
        src/user/TitleScreen.cpp
        src/user/TitleCard.cpp
        src/user/SkyBox.cpp
        src/user/systems/screenFade.h
        src/user/systems/screenFade.cpp
        src/user/globals.h
        src/user/LoadingZone.cpp
        src/user/MiniMap.cpp
        src/user/TestRot.cpp
        src/user/Player.cpp
        src/user/systems/dropShadows.cpp
        src/user/systems/dropShadows.h
        src/user/systems/sprites.h
        src/user/systems/sprites.cpp
        src/user/Coin.cpp
        src/user/HUD.cpp
        src/user/systems/fonts.h
        src/user/systems/fonts.cpp
        src/p64/assetTable.h
        src/user/Particles.cpp
        src/user/ObjVoid.cpp
        src/user/systems/dialog.h
        src/user/systems/dialog.cpp
        src/user/systems/dialogMessages.h
        src/user/Seesaw.cpp
        src/p64/CA20630FE536B54B.cpp
        src/p64/scriptTable.cpp
        src/user/Credits.cpp
        src/user/DebugCam.cpp
        src/user/BootChecks.cpp
        src/user/BootLogos.cpp
        src/user/ObjHead.cpp
        src/user/Goal.cpp
)
