/**
 * @copyright 2026 - Max Bebök
 * @license MIT
 */
#include "scene/components/charBody.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"

namespace
{
  struct InitData
  {
    fm_vec3_t up{};
    fm_vec3_t centerOffset{};
    float gravity{};
    float maxFallSpeed{};
    float floorMaxAngle{};
    float stepHeight{};
    float floorSnapDistance{};
    float radius{};
    float height{};
    uint8_t collTypes{};
    uint8_t maxSlides{};
    uint8_t readMask{};
  };
}

namespace P64::Comp
{
  void CharBody::initDelete(Object& obj, CharBody* data, void* initData_)
  {
    if(initData_ == nullptr) {
      data->getBody().~CharacterBody();
      return;
    }

    InitData* initData = static_cast<InitData*>(initData_);

    // Placement-new with the owning object
    data->body = Coll::CharacterBody{&obj};

    // Apply settings from the editor
    data->body.settings.up               = initData->up;
    data->body.settings.centerOffset     = initData->centerOffset;
    data->body.settings.gravity          = initData->gravity;
    data->body.settings.maxFallSpeed     = initData->maxFallSpeed;
    data->body.settings.floorMaxAngle    = initData->floorMaxAngle;
    data->body.settings.stepHeight       = initData->stepHeight;
    data->body.settings.floorSnapDistance = initData->floorSnapDistance;
    data->body.settings.radius           = initData->radius;
    data->body.settings.height           = initData->height;
    data->body.settings.collTypes        = static_cast<Coll::RaycastColliderTypeFlags>(initData->collTypes);
    data->body.settings.maxSlides        = initData->maxSlides;
    data->body.settings.readMask         = initData->readMask;
  }

  void CharBody::update(Object& obj, CharBody* data, float deltaTime)
  {
    /*
    if(!obj.isEnabled()) return;
    auto &scene = SceneManager::getCurrent().getCollision();
    data->body().moveAndSlide(deltaTime, scene);
    */
  }
}
