/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "collision/attach.h"
#include "scene/components/collMesh.h"
#include "scene/scene.h"

fm_vec3_t P64::Coll::Attach::update(const fm_vec3_t &ownPos)
{
  auto trackedObj = SceneManager::getCurrent().getObjectById(refId);
  auto trackedColl = trackedObj ? trackedObj->getComponent<Comp::CollMesh>() : nullptr;

  fm_vec3_t diff{};
  if(trackedColl && trackedColl->meshCollider)
  {
    auto nextRefPos = ownPos;
    if(lastRefId == refId) {
      nextRefPos = trackedColl->meshCollider->toWorldSpace(refPosLocal);
      diff = refPos - nextRefPos;
    }

    lastRefId = refId;
    refPos = ownPos - diff;
    refPosLocal = trackedColl->meshCollider->toLocalSpace(refPos);
  } else {
    lastRefId = 0;
  }
  refId = 0;
  return diff;
}

void P64::Coll::Attach::setReference(const Coll::MeshCollider *meshCollider)
{
  if(meshCollider) {
    refId = meshCollider->ownerObject()->id;
  }
}
