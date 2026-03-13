/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

#include "scene/scene.h"

namespace P64::Renderer
{
  struct Material
  {
    constexpr static uint16_t MASK_DEPTH  = 1 << 0;
    constexpr static uint16_t MASK_PRIM   = 1 << 1;
    constexpr static uint16_t MASK_ENV    = 1 << 2;
    constexpr static uint16_t MASK_LIGHT  = 1 << 3;

    uint16_t setMask{};
    uint8_t fresnel{};
    uint8_t valFlags{};
    color_t colorPrim{};
    color_t colorEnv{};
    color_t colorFresnel{};

    [[nodiscard]] constexpr bool doesAnything() const {
      return setMask != 0;
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

  // @TODO: temporary hack to migrate t3d materials:
  struct WIP_T3DModelState {
    uint64_t lastCC{0};
    uint64_t lastOtherMode{0};
    uint32_t lastRenderFlags{0};

    uint16_t lastTextureIdxA{0xFFFF};
    uint16_t lastTextureIdxB{0xFFFF};
    uint16_t lastUvGenParams[2]{0,0};

    color_t lastPrimColor{};
    color_t lastEnvColor{};
    color_t lastBlendColor{};
    uint8_t lastVertFXFunc{0};
    uint8_t lastFogMode{0xFF};
  };

  // @TODO: temporary hack to migrate t3d materials:
  typedef struct {
    float low;
    float height;
    int8_t mask;
    int8_t shift;
    uint8_t mirror;
    uint8_t clamp;
  } WIP_T3DMaterialAxis;

  // @TODO: temporary hack to migrate t3d materials:
  struct WIP_T3DMaterial
  {
    typedef struct {
      sprite_t* texture;
      uint16_t texReference; // dynamic/offscreen texture if non-zero, can be set in fast64
      uint16_t texAssetIdx;
      uint16_t texWidth;
      uint16_t texHeight;

      WIP_T3DMaterialAxis s;
      WIP_T3DMaterialAxis t;
    } WIP_T3DMaterialTexture;

    uint64_t colorCombiner;
    uint64_t otherModeValue;
    uint64_t otherModeMask;
    uint32_t blendMode;
    uint32_t renderFlags;

    uint8_t _unused00_; // see: T3D_ALPHA_MODE_xxx
    uint8_t fogMode; // see: T3D_FOG_MODE_xxx
    uint8_t setColorFlags;
    uint8_t vertexFxFunc;

    color_t primColor;
    color_t envColor;
    color_t blendColor;

    WIP_T3DMaterialTexture textureA;
    WIP_T3DMaterialTexture textureB;

    void begin(WIP_T3DModelState &state);
    void end(WIP_T3DModelState &state);
  };
}
