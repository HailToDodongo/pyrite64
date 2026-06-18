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
    uint8_t followFloor{};
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

    // Placement-new with the owning object, then apply editor settings
    data->body = Coll::CharacterBody{&obj};
    data->body.configure({
      .up               = initData->up,
      .centerOffset     = initData->centerOffset,
      .gravity          = initData->gravity,
      .maxFallSpeed     = initData->maxFallSpeed,
      .floorMaxAngle    = initData->floorMaxAngle,
      .stepHeight       = initData->stepHeight,
      .floorSnapDistance = initData->floorSnapDistance,
      .radius           = initData->radius,
      .height           = initData->height,
      .collTypes        = static_cast<Coll::RaycastColliderTypeFlags>(initData->collTypes),
      .maxSlides        = initData->maxSlides,
      .readMask         = initData->readMask,
      .followFloor      = static_cast<bool>(initData->followFloor),
    });
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
