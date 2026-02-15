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

  SceneManager::getCurrent().addCamera(&data->camera);
  auto &cam = data->camera;
  cam.setScreenArea(initData->vpOffset[0], initData->vpOffset[1], initData->vpSize[0], initData->vpSize[1]);
  cam.fov  = initData->fov;
  cam.near = initData->near;
  cam.far  = initData->far;

  cam.aspectRatio = initData->aspectRatio;
  if(cam.aspectRatio <= 0) {
    cam.aspectRatio = (float)initData->vpSize[0] / (float)initData->vpSize[1];
  }

  cam.setPosRot(obj.pos, obj.rot);
}
