/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "scene/camera.h"
#include "lib/logger.h"
#include "scene/globalState.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"
#include "scene/components/surface.h"
#include "renderer/pipeline.h"

namespace
{
  constexpr fm_vec3_t FORWARD{0,0,-1};
  constexpr fm_vec3_t UP{0,1,0};
}

P64::Camera::Camera() {
  viewports = t3d_viewport_create_buffered(3);
}

P64::Camera::~Camera() {
  t3d_viewport_destroy(&viewports);
}

void P64::Camera::setTargetScreen()
{
  targetType = TargetType::SCREEN;
  targetObjId = 0;
  targetSurfPtr = nullptr;
  targetDepthPtr = nullptr;
  if(screenArea[2] > 0) {
    t3d_viewport_set_area(viewports, screenArea[0], screenArea[1], screenArea[2], screenArea[3]);
  }
}

void P64::Camera::setTargetSurface(Object* obj)
{
  targetType = TargetType::SURFACE;
  targetSurfPtr = nullptr;
  targetDepthPtr = nullptr;
  targetObjId = (obj && obj->getComponent<Comp::Surface>()) ? obj->id : 0;
}

P64::Comp::Surface* P64::Camera::resolveTargetSurface() const
{
  if(targetType != TargetType::SURFACE || targetObjId == 0)return nullptr;
  auto obj = SceneManager::getCurrent().getObjectById(targetObjId);
  return obj ? obj->getComponent<Comp::Surface>() : nullptr;
}

void P64::Camera::adjustToSurface(const surface_t &surf)
{
  auto fmt = surface_get_format(&surf);
  assertf(fmt == FMT_RGBA16 || fmt == FMT_RGBA32 || fmt == FMT_I8 || fmt == FMT_CI8,
    "Camera target surface must use a renderable format (RGBA16/RGBA32/I8/CI8)");

  if(viewports.size[0] != surf.width || viewports.size[1] != surf.height) {
    t3d_viewport_set_area(viewports, 0, 0, surf.width, surf.height);
  }
}

void P64::Camera::update([[maybe_unused]] float deltaTime)
{
  if(projection == Projection::ORTHOGRAPHIC) {
    float halfHeight = orthoSize;
    float halfWidth = orthoSize * aspectRatio;
    t3d_viewport_set_ortho(&viewports, -halfWidth, halfWidth, -halfHeight, halfHeight, near, far);
  } else {
    t3d_viewport_set_perspective(&viewports, fov, aspectRatio, near, far);
  }
  t3d_viewport_set_view_matrix(&viewports, &viewMatrix);
}

bool P64::Camera::attach()
{
  currTgtSurf = nullptr;
  currTgtDepth = nullptr;

  if(hasSurfaceTarget())
  {
    if(targetSurfPtr) {
      currTgtSurf = targetSurfPtr;
      currTgtDepth = targetDepthPtr;
    } else if(auto surfComp = resolveTargetSurface()) {
      currTgtSurf = &surfComp->getSurface();
      currTgtDepth = surfComp->getDepthBuffer();
    }
    if(currTgtSurf == nullptr)return false; // no target set (or its owner was deleted)

    adjustToSurface(*currTgtSurf);

    if(currTgtDepth == nullptr) {
      auto mainDepth = SceneManager::getCurrent().getRenderPipeline<RenderPipeline>()->getCurrDepthSurf();
      assertf(currTgtSurf->width * 2 * currTgtSurf->height <= mainDepth->stride * mainDepth->height,
        "Surface target too big to re-use the main depth buffer, enable 'Depth Buffer' on the Surface component");
      depthView = surface_make(mainDepth->buffer, FMT_RGBA16, currTgtSurf->width, currTgtSurf->height, currTgtSurf->width * 2);
      currTgtDepth = &depthView;
    } else if(currTgtDepth->width != currTgtSurf->width || currTgtDepth->height != currTgtSurf->height) {
      // an over-sized depth buffer is allowed, view it at the color buffer's size
      depthView = surface_make(currTgtDepth->buffer, FMT_RGBA16, currTgtSurf->width, currTgtSurf->height, currTgtSurf->width * 2);
      currTgtDepth = &depthView;
    }

    rdpq_attach(currTgtSurf, currTgtDepth);
    rdpq_clear_z(ZBUF_MAX);
  }

  t3d_viewport_attach(viewports);
  return true;
}

void P64::Camera::detach()
{
  if(currTgtSurf) {
    rdpq_detach();
    currTgtSurf = nullptr;
    currTgtDepth = nullptr;
  }
}

void P64::Camera::applyTargetImages()
{
  if(currTgtSurf == nullptr)return;
  rdpq_sync_pipe();
  rdpq_set_color_image(currTgtSurf);
  rdpq_set_z_image(currTgtDepth);
}

void P64::Camera::restoreTargetImages()
{
  if(currTgtSurf == nullptr)return;
  auto pipeline = SceneManager::getCurrent().getRenderPipeline<RenderPipeline>();
  rdpq_sync_pipe();
  rdpq_set_color_image(pipeline->getCurrColorSurf());
  rdpq_set_z_image(pipeline->getCurrDepthSurf());
}

void P64::Camera::setScreenArea(int x, int y, int width, int height) {
  screenArea[0] = x;
  screenArea[1] = y;
  screenArea[2] = width;
  screenArea[3] = height;
  t3d_viewport_set_area(viewports, x,y, width, height);
}

void P64::Camera::setLookAt(const fm_vec3_t &newPos, const fm_vec3_t &newTarget, const fm_vec3_t &newUp) {
  target = newTarget;
  up = newUp;
  pos = newPos;
  t3d_mat4_look_at(&viewMatrix, &newPos, &newTarget, &newUp);
  needsProjUpdate = true;
}

void P64::Camera::setPosRot(const fm_vec3_t &newPos, const fm_quat_t&rot) {
  setLookAt(newPos, newPos + (rot * FORWARD), rot * UP);
}

fm_vec3_t P64::Camera::getScreenPos(const fm_vec3_t &worldPos)
{
  fm_vec3_t res{};
  t3d_viewport_calc_viewspace_pos(viewports, res, worldPos);
  return res;
}

