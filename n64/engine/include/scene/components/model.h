/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <t3d/t3dmodel.h>

#include "assets/assetManager.h"
#include "lib/matrixManager.h"
#include "renderer/drawLayer.h"
#include "renderer/material.h"
#include "scene/object.h"
#include "script/scriptTable.h"

namespace P64::Comp
{
  struct Model
  {
    static constexpr uint32_t ID = 1;

    // performs culling of indiviudal objects
    static constexpr uint8_t FLAG_CULLING = 1 << 0;

    T3DModel *model{};
    RingMat4FP matFP{};
    Renderer::MaterialInstance material{};
    uint8_t layerIdx{0};
    uint8_t flags{0};
    uint8_t meshIdxCount{0};
    uint8_t meshIndices[];

    static uint32_t getAllocSize([[maybe_unused]] uint16_t* initData);

    static void initDelete([[maybe_unused]] Object& obj, Model* data, void* initData);

    static void update(Object& obj, Model* data, [[maybe_unused]] float deltaTime) {}

    static void draw([[maybe_unused]] Object& obj, Model* data, [[maybe_unused]] float deltaTime);
  };
}