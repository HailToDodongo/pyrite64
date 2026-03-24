/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

namespace P64
{
  class Object;
}

namespace P64::Renderer
{
  struct MaterialState {
    uint16_t lastTextureIdxA{0xFFFF};
    uint16_t lastTextureIdxB{0xFFFF};
    //uint16_t lastUvGenParams[2]{0,0};
    //uint8_t lastVertFXFunc{0};
  };

  struct Material
  {
    struct TileAxis {
      uint16_t offset;
      uint16_t repeat;
      int8_t scale;
      int8_t mirror;
    };

    struct Tile {
      uint16_t texAssetIdx;
      uint16_t texReference;
      TileAxis s;
      TileAxis t;
    };

    // flags which settings to set: (NOTE: keep in sync with the builder!)
    constexpr static uint32_t FLAG_AA         = 1 << 0;
    constexpr static uint32_t FLAG_FOG        = 1 << 1;
    constexpr static uint32_t FLAG_DITHER     = 1 << 2;
    constexpr static uint32_t FLAG_FILTER     = 1 << 3;
    constexpr static uint32_t FLAG_ZMODE      = 1 << 4;
    constexpr static uint32_t FLAG_ZPRIM      = 1 << 5;
    constexpr static uint32_t FLAG_PERSP      = 1 << 6;
    constexpr static uint32_t FLAG_ALPHA_COMP = 1 << 7;

    constexpr static uint32_t FLAG_TEX0       = 1 << 8;
    constexpr static uint32_t FLAG_TEX1       = 1 << 9;
    constexpr static uint32_t FLAG_CC         = 1 << 10;
    constexpr static uint32_t FLAG_BLENDER    = 1 << 11;
    constexpr static uint32_t FLAG_K4K5       = 1 << 12;
    constexpr static uint32_t FLAG_PRIMLOD    = 1 << 13;
    constexpr static uint32_t FLAG_PRIM       = 1 << 14;
    constexpr static uint32_t FLAG_ENV        = 1 << 15;

    constexpr static uint32_t FLAG_T3D_VERT_FX = 1 << 16;
    constexpr static uint32_t FLAG_T3D_        = 1 << 17;

    constexpr static uint32_t FLAG_OVERRIDE    = 1 << 18;

    // some data is stored directly in the flag value:

    [[nodiscard]] constexpr rdpq_antialias_t getAA() const {
      return static_cast<rdpq_antialias_t>(flagsData >> 19 & 0b11);
    }

    [[nodiscard]] constexpr rdpq_filter_t getFilter() const {
      return static_cast<rdpq_filter_t>(flagsData >> 21 & 0b11);
    }

    [[nodiscard]] constexpr bool getZRead() const {
      return flagsData & (1 << 23);
    }
    [[nodiscard]] constexpr bool getZWrite() const {
      return flagsData & (1 << 24);
    }
    [[nodiscard]] constexpr bool getModePersp() const {
      return flagsData & (1 << 25);
    }
    [[nodiscard]] constexpr rdpq_dither_t getDither() const {
      return static_cast<rdpq_dither_t>(flagsData >> 26 & 0b1111);
    }

    uint32_t flagsData;
    uint32_t t3dDrawFlags;

    [[nodiscard]] constexpr bool sets(uint32_t flag) const {
      return (flagsData & flag) != 0;
    }

    // data values follow here
    char data[];

    void begin(MaterialState &state);
    void end(MaterialState &state);

    void destroy() {
      flagsData = 0;
    }

    const Tile* getTile(uint8_t idx);
  };

  struct MaterialInstance
  {
    constexpr static uint16_t MASK_DEPTH  = 1 << 0;
    constexpr static uint16_t MASK_PRIM   = 1 << 1;
    constexpr static uint16_t MASK_ENV    = 1 << 2;
    constexpr static uint16_t MASK_LIGHT  = 1 << 3;

    constexpr static uint16_t MASK_SLOT0  = 1 << 8;
    constexpr static uint16_t MASK_SLOT1  = 1 << 9;
    constexpr static uint16_t MASK_SLOT2  = 1 << 10;
    constexpr static uint16_t MASK_SLOT3  = 1 << 11;
    constexpr static uint16_t MASK_SLOT4  = 1 << 12;
    constexpr static uint16_t MASK_SLOT5  = 1 << 13;
    constexpr static uint16_t MASK_SLOT6  = 1 << 14;
    constexpr static uint16_t MASK_SLOT7  = 1 << 15;

    uint32_t dataSize{}; // dynamic, @TODO: optmize space / alignemnt
    uint16_t setMask{};
    uint8_t fresnel{};
    uint8_t valFlags{};
    color_t colorPrim{};
    color_t colorEnv{};
    color_t colorFresnel{};

    private:
      Material::Tile texSlots[];

    public:
      [[nodiscard]] constexpr bool doesAnything() const {
        return setMask != 0;
      }

      [[nodiscard]] constexpr bool setsSlots() const {
        return (setMask & 0xFF00) != 0;
      }

      constexpr uint16_t getDepthRead() const {
        return valFlags & 0b01;
      }

      constexpr uint16_t getDepthWrite() const {
        return valFlags & 0b10;
      }

      void begin(Object &obj);
      void end();

  };
}
