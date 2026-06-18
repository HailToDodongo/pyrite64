/**
 * @file rigidBody.cpp
 * @author Kevin Reier <https://github.com/Byterset>
 * @brief Defines the RigidBody component, which allows an Object to have a rigid body for physics simulation (see rigidBody.h)
 */
#include "scene/object.h"
#include "scene/components/rigidBody.h"
#include "lib/logger.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"
#include <cmath>

namespace
{
  struct InitData
  {
    float mass{};
    bool isKinematic{};
    bool constrainPosX{};
    bool constrainPosY{};
    bool constrainPosZ{};
    bool constrainRotX{};
    bool constrainRotY{};
    bool constrainRotZ{};
    bool hasGravity{};
    float gravityScalar{};
    float timeScalar{};
    float angularDamping{};
  };
}

namespace P64::Comp
{
  void RigidBody::initDelete([[maybe_unused]] Object& obj, RigidBody* data, void* initData_)
  {
    InitData* initData = static_cast<InitData*>(initData_);
    auto &coll = SceneManager::getCurrent().getCollision();

    if (initData == nullptr) {
      coll.removeRigidBody(&data->rigidBody);
      // TODO: add collider to new collision scene
      data->~RigidBody();
      return;
    }

    //existing rigidBodies for this object in the scene? If yes don't add another one and log an error
    auto existing = coll.findRigidBodyByObjectId(obj.id);
    if(existing) {
      P64::Log::error("Object '%d' already has a Rigidbody component, cannot add another one", obj.id);
      return;
    }

    new (data) RigidBody();

    data->rigidBody.init(&obj, initData->mass);
    data->rigidBody.setKinematic(initData->isKinematic);
    data->rigidBody.setHasGravity(initData->hasGravity);
    data->rigidBody.setGravityScale(initData->gravityScalar);
    data->rigidBody.setTimeScale(initData->timeScalar);
    data->rigidBody.setAngularDamping(initData->angularDamping);
    Coll::Constraint constraints = Coll::Constraint::None;
    if(initData->constrainPosX) constraints = constraints | Coll::Constraint::FreezePosX;
    if(initData->constrainPosY) constraints = constraints | Coll::Constraint::FreezePosY;
    if(initData->constrainPosZ) constraints = constraints | Coll::Constraint::FreezePosZ;
    if(initData->constrainRotX) constraints = constraints | Coll::Constraint::FreezeRotX;
    if(initData->constrainRotY) constraints = constraints | Coll::Constraint::FreezeRotY;
    if(initData->constrainRotZ) constraints = constraints | Coll::Constraint::FreezeRotZ;
    data->rigidBody.setConstraints(constraints);

    if(obj.isEnabled()) {
      coll.addRigidBody(&data->rigidBody);
    }
  }

  void RigidBody::onEvent(Object &obj, RigidBody* data, const ObjectEvent &event)
  {
    if(event.type == EVENT_TYPE_DISABLE) {
      return obj.getScene().getCollision().removeRigidBody(&data->rigidBody);
    }
    if(event.type == EVENT_TYPE_ENABLE) {
      return obj.getScene().getCollision().addRigidBody(&data->rigidBody);
    }
  }

  void RigidBody::update(Object &obj, RigidBody* data, float deltaTime)
  {
  }
}
