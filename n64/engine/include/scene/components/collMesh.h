/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "scene/object.h"
#include <t3d/t3dmodel.h>

#include "collision/meshCollider.h"

namespace P64::Comp
{
  struct CollMesh
  {
    static constexpr uint32_t ID = 4;

    Coll::MeshCollider *meshCollider{nullptr};
    uint8_t flags;

    static uint32_t getAllocSize([[maybe_unused]] uint16_t* initData);

    static void initDelete([[maybe_unused]] Object& obj, CollMesh* data, uint16_t* initData);

    static void onEvent(Object &obj, CollMesh* data, const ObjectEvent &event);

    static void update(Object& obj, CollMesh* data, float deltaTime);
  };
}
