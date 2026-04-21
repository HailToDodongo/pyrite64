/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene/object.h"

#include "scene/componentTable.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"

P64::Object::~Object()
{
  auto compRefs = getCompRefs();
  for (uint32_t i=0; i<compCount; ++i) {
    const auto &compDef = COMP_TABLE[compRefs[i].type];
    char* dataPtr = (char*)this + compRefs[i].offset;
    compDef.initDel(*this, dataPtr, nullptr);
  }
}

void P64::Object::setEnabled(bool isEnabled)
{
  if(isEnabled != this->isSelfEnabled()) {
    flags |= ObjectFlags::PENDING_ACTIVE_CHG;
    SceneManager::getCurrent().needsObjStateUpdate = true;
  } else {
    flags &= ~ObjectFlags::PENDING_ACTIVE_CHG;
  }
}

void P64::Object::remove(bool keepChildren)
{
  if(flags & ObjectFlags::PENDING_REMOVE)return;
  flags |= ObjectFlags::PENDING_REMOVE;
  flags &= ~(ObjectFlags::ACTIVE | ObjectFlags::PENDING_ACTIVE_CHG);
  SceneManager::getCurrent().removeObject(*this);

  if(!keepChildren)
  {
    iterChildren([keepChildren](Object* child)
    {
        if(child) child->remove(keepChildren);
    });
  }
}

fm_vec3_t P64::Object::intoLocalSpace(const fm_vec3_t &p) const
{
  fm_quat_t invRot;
  fm_quat_inverse(&invRot, &rot);

  auto res = (p  - pos);
  return invRot * res / scale;
}

fm_vec3_t P64::Object::outOfLocalSpace(const fm_vec3_t &p) const
{
  return rot * (p * scale) + pos;
}

P64::Object* P64::ObjectRef::get() const
{
  return SceneManager::getCurrent().getObjectById((uint16_t)id);
}
