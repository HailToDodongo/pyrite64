/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene/object.h"
#include "scene/components/camera.h"

#include "scene/scene.h"

void P64::Comp::Camera::initDelete(Object &obj, Camera* data, InitData* initData)
{
  if (initData == nullptr) {
    SceneManager::getCurrent().removeCamera(&data->camera);
    data->~Camera();
    return;
  }

  new(data) Camera();

  data->mode = initData->mode;
  auto &cam = data->camera;
  cam.setScreenArea(initData->vpOffset[0], initData->vpOffset[1], initData->vpSize[0], initData->vpSize[1]);
  cam.fov  = initData->fov;
  cam.near = initData->near;
  cam.far  = initData->far;
  cam.orthoSize = initData->orthoSize;
  cam.projection = initData->projection;
  cam.visMask = initData->visMask;
  cam.targetType = (P64::Camera::TargetType)initData->targetType;
  cam.targetObjId = initData->targetObjId;

  cam.aspectRatio = initData->aspectRatio;
  if(cam.aspectRatio <= 0) {
    cam.aspectRatio = (float)initData->vpSize[0] / (float)initData->vpSize[1];
  }

  cam.setPosRot(obj.pos, obj.rot);

  if(obj.isEnabled()) {
    SceneManager::getCurrent().addCamera(&cam);
  }
}

void P64::Comp::Camera::onEvent(Object &obj, Camera* data, const ObjectEvent &event)
{
  if(event.type == EVENT_TYPE_DISABLE) {
    return SceneManager::getCurrent().removeCamera(&data->camera);
  }
  if(event.type == EVENT_TYPE_ENABLE) {
    return SceneManager::getCurrent().addCamera(&data->camera);
  }
}

void P64::Comp::Camera::update(Object &obj, Camera* data, float deltaTime)
{
  if(data->mode == Mode::OBJECT) {
    data->camera.setPosRot(obj.pos, obj.rot);
  }
}
