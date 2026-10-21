/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>

#include "scene/components/collBody.h"
#include "lib/types.h"

namespace P64::DrawLayer
{
  struct Conf
  {
    static constexpr uint32_t FLAG_Z_WRITE = 1 << 0;
    static constexpr uint32_t FLAG_Z_COMPARE = 1 << 1;

    enum class FogMode : uint8_t
    {
      NONE = 0,
      CLEAR_COLOR,
      CUSTOM_COLOR,
      UNCHANGED_COLOR,
    };

    uint32_t flags{};
    uint32_t blender{};

    color_t fogColor{};
    float fogMin{};
    float fogMax{};
    FogMode fogMode{};
    uint8_t lightMode{};

    uint8_t padding1{};
    uint8_t padding2{};

    uint32_t key{}; // hash of the layer name, used by objects to reference it
  };

  struct Setup
  {
    uint8_t layerCount3D{};
    uint8_t layerCountPtx{};
    uint8_t layerCount2D{};
    uint8_t padding{};
    Conf layerConf[16]{};
  };

  /**
   * Layers are referenced either by index (0 to MAX_INDEX_REF),
   * or by the hash of their name (everything above that number).
   */
  constexpr uint32_t MAX_INDEX_REF = 64;

  void init(Setup &setup);

  /// switches to a layer by its index across all lists
  void use(uint32_t idx);

  /**
   * Returns the index within the list of the referenced layer, without switching to it.
   * References that don't exist fall back to the first layer of the list.
   */
  uint8_t resolve3D(uint32_t ref);
  uint8_t resolvePtx(uint32_t ref);
  uint8_t resolve2D(uint32_t ref);

  /**
   * Switches to a layer of the given list.
   * References that don't exist fall back to the first layer of the list.
   * @return index within the list, can be stored and passed in again to skip searching for a name hash
   */
  uint8_t use3D(uint32_t ref = 0);
  uint8_t usePtx(uint32_t ref = 0);
  uint8_t use2D(uint32_t ref = 0);

  inline void useDefault() { use(0); }

  /// @brief applies layer modes and runs commands recorded since last run.
  /// Layers can be recorded and drawn multiple times per frame (e.g. multiple cameras). 
  /// @param layerIdx 
  void draw(uint32_t layerIdx);

  void draw3D();
  void drawPtx();
  void draw2D();

  void nextFrame();
  void reset();
}
