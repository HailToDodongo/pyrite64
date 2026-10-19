/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>
#include <type_traits>
#include "script/scriptType.h"

// some generic includes most scripts will need
#include "scene/scene.h"
#include "scene/sceneManager.h"
#include "assets/assetManager.h"
#include "audio/audioManager.h"
#include "lib/math.h"
#include "collision/shapes.h"
#include "renderer/drawLayer.h"

#include "p64/assetTable.h"
#include "p64/sceneTable.h"

#define P64_DATA_COMMON_(TYPE) \
  static_assert(sizeof(Data) < 0xFFFF); \
  constinit uint16_t DATA_SIZE = static_cast<uint16_t>( \
    std::is_empty_v<Data> ? 0 : sizeof(Data) \
  ); \
  constinit const void* DATA_TYPE = &::P64::Script::TYPE_TAG<TYPE>;

/**
 * Defines the data of a script inline.
 */
#define P64_DATA(...) struct Data { __VA_ARGS__ }; \
  P64_DATA_COMMON_(Data)

/**
 * Uses an existing struct (e.g. from a shared header) as the data of a script.
 */
#define P64_DATA_TYPE(T) struct Data : T {}; \
  static_assert(std::is_trivially_copyable_v<T>, "P64_DATA_TYPE: '" #T "' must be trivially copyable"); \
  static_assert(std::is_standard_layout_v<T>, "P64_DATA_TYPE: '" #T "' must be standard-layout"); \
  P64_DATA_COMMON_(T)
