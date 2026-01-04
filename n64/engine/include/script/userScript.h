/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>
#include <type_traits>

// some generic includes most scripts will need
#include "scene/scene.h"
#include "scene/sceneManager.h"
#include "assets/assetManager.h"
#include "audio/audioManager.h"
#include "lib/math.h"
#include "collision/shapes.h"
#include "renderer/drawLayer.h"

#define P64_DATA(...) struct Data { __VA_ARGS__ }; \
  static_assert(sizeof(Data) < 0xFFFF); \
  constinit uint16_t DATA_SIZE = static_cast<uint16_t>( \
    std::is_empty_v<Data> ? 0 : sizeof(Data) \
  );
