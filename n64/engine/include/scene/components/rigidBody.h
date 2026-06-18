/**
 * @file rigidBody.h
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Defines the RigidBody component, which allows an Object to have a rigid body for physics simulation
 */
#pragma once
#include "assets/assetManager.h"
#include "scene/object.h"
#include "assets/assetManager.h"
#include <t3d/t3dmodel.h>
#include "collision/rigidBody.h"


namespace P64::Comp
{
  struct RigidBody
  {
    static constexpr uint32_t ID = 11;

    private:
      Coll::RigidBody rigidBody{};
    public:

    static uint32_t getAllocSize([[maybe_unused]] uint16_t* initData)
    {
      return sizeof(RigidBody);
    }

    static void initDelete([[maybe_unused]] Object& obj, RigidBody* data, void* initData);

    static void onEvent(Object& obj, RigidBody* data, const ObjectEvent& event);

    static void update(Object& obj, RigidBody* data, float deltaTime);

    Coll::RigidBody& getBody() { return rigidBody; }
  };
}
