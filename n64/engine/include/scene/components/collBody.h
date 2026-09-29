/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "assets/assetManager.h"
#include "scene/object.h"
#include "assets/assetManager.h"
#include <t3d/t3dmodel.h>

#include "collision/colliderShape.h"

namespace P64::Comp
{
  /// Component that attaches a primitive-shape collider to an object and registers it with the collision scene.
  /// Everything about the collider itself (size, shape, offset, masks, ...) is set on
  /// 'collider' directly, see 'Coll::Collider'.
  struct CollBody
  {
    static constexpr uint32_t ID = 5;

    Coll::Collider collider{};

    static uint32_t getAllocSize([[maybe_unused]] uint16_t* initData)
    {
      return sizeof(CollBody);
    }

    static void initDelete([[maybe_unused]] Object& obj, CollBody* data, void* initData);

    static void onEvent(Object& obj, CollBody* data, const ObjectEvent& event);
  };
}
