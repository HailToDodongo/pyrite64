/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "scene/object.h"
#include "scene/components/culling.h"

#include "renderer/renderScale.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"

namespace
{
  struct InitData
  {
    fm_vec3_t halfExtend{};
    fm_vec3_t offset{};
    uint8_t type{};
  };
}

void P64::Comp::Culling::initDelete(Object &obj, Culling* data, void* initData)
{
  if(initData == nullptr)return;
  memcpy(data, initData, sizeof(InitData));
}

void P64::Comp::Culling::draw(Object &obj, Culling* data, float deltaTime)
{
  auto vp = t3d_viewport_get();
  // extents/offsets are in meters, the frustum is in render units
  float renderScale = Renderer::getRenderScale();
  auto pos = ((data->offset * obj.scale) + obj.pos) * renderScale;

  if(data->type == 0)
  {
    auto scaledSize = data->halfExtend * obj.scale * renderScale;
    auto min = pos - scaledSize;
    auto max = pos + scaledSize;
    if(!t3d_frustum_vs_aabb(&vp->viewFrustum, &min, &max)) {
      obj.setFlag(ObjectFlags::IS_CULLED, true);
    }
  } else {
    float maxSize = fmaxf(fmaxf(obj.scale.x, obj.scale.y), obj.scale.z);
    if(!t3d_frustum_vs_sphere(&vp->viewFrustum, &pos, data->halfExtend.x * maxSize * renderScale)) {
      obj.setFlag(ObjectFlags::IS_CULLED, true);
    }
  }
}
