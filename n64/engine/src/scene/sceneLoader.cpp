/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include <libdragon.h>
#include <cstdint>
#include <malloc.h>
#include "scene/scene.h"
#include "lib/math.h"
#include "scene/componentTable.h"

namespace {
  constexpr uint32_t DATA_ALIGN = 8;

  struct ObjectEntry {
    uint16_t flags;
    uint16_t id;
    uint16_t group;
    uint16_t _padding;
    fm_vec3_t pos;
    fm_vec3_t scale;
    uint32_t packedRot;
    // data follows
  };

  struct __attribute__((packed)) ObjectEntryCamera : public ObjectEntry {
    uint16_t _padding;
    fm_vec3_t pos{};
    fm_quat_t rot{};
    float fov{};
    float near{};
    float far{};
    int16_t vpOffset[2]{};
    int16_t vpSize[2]{};
  };

  // to avoid any allocations for file names,
  // the path is stored here and changed by each load
  char scenePath[] = "rom:/p64/s0000_";

  inline void updateScenePath(uint16_t id)
  {
    scenePath[sizeof(scenePath)-5] = '0' + ((id/100) % 10);
    scenePath[sizeof(scenePath)-4] = '0' + ((id/10) % 10);
    scenePath[sizeof(scenePath)-3] = '0' + (id % 10);
  }

  inline void* loadSubFile(char type) {
    scenePath[sizeof(scenePath)-2] = type;
    scenePath[sizeof(scenePath)-1] = '\0';
    return asset_load(scenePath, nullptr);
  }
}

void P64::Scene::loadSceneConfig()
{
  updateScenePath(id);
  scenePath[sizeof(scenePath)-2] = '\0';

  {
    auto *tmp = (SceneConf*)asset_load(scenePath, nullptr);
    conf = *tmp;
    free(tmp);
  }
}

P64::Object* P64::Scene::loadObject(uint8_t* &objFile, std::function<void(Object&)> callback, bool deferComponentInit)
{
  ObjectEntry* objEntry = (ObjectEntry*)objFile;

  // pre-scan components to get total allocation size
  uint32_t allocSize = sizeof(Object);

  // some alignment logic below relies on an at a minimum 4-byte size
  static_assert(sizeof(Object) % 4 == 0);
  static_assert(sizeof(Object::CompRef) % 4 == 0);

  auto ptrIn = objFile + sizeof(ObjectEntry);
  uint32_t compCount = 0;
  uint32_t compDataSize = 0;
  while(ptrIn[1] != 0) {
    auto compId = ptrIn[0];
    auto argSize = ptrIn[1] * 4;

    assertf(compId < COMP_TABLE_SIZE, "Invalid component ID %d!", compId);
    const auto &compDef = COMP_TABLE[compId];
    assertf(compDef.getAllocSize != nullptr, "Component %d unknown!", compId);
    compDataSize += Math::alignUp(compDef.getAllocSize(ptrIn + 4), DATA_ALIGN);
    allocSize += sizeof(Object::CompRef);

    ptrIn += argSize;
    ++compCount;
  }

  // component data must be 8-byte aligned, GCC tries to be smart
  // and some structs cuse 64-bit writes to members.
  // if it is misaligned, add spacing after the comp table
  uint32_t offsetData = (sizeof(Object::CompRef) * compCount);
  if(allocSize % 8 != 0) {
    compDataSize += 4;
    offsetData += 4;
  }

  allocSize += compDataSize;

  //debugf("Allocating object %d | comps: %d | size: %lu bytes\n", objEntry->id, compCount, allocSize);

  void* objMem = memalign(DATA_ALIGN, allocSize); // @TODO: custom allocator
  if(allocSize < 16) {
    memset(objMem, 0, allocSize);
  } else {
    sys_hw_memset(objMem, 0, allocSize);
  }

  auto objCompTablePtr = (Object::CompRef*)((char*)objMem + sizeof(Object));
  auto objCompDataPtr = (char*)(objCompTablePtr) + offsetData;

  Object* obj = new(objMem) Object();
  obj->id = objEntry->id;
  obj->group = objEntry->group;
  obj->flags = objEntry->flags;
  obj->compCount = compCount;
  obj->pos = objEntry->pos;
  obj->scale = objEntry->scale;
  obj->rot = Math::unpackQuat(objEntry->packedRot);

  if(callback)callback(*obj);

  ptrIn = objFile + sizeof(ObjectEntry);
  while(ptrIn[1] != 0)
  {
    uint8_t compId = ptrIn[0];
    uint8_t argSize = ptrIn[1] * 4;

    const auto &compDef = COMP_TABLE[compId];
    // debugf("Alloc: comp %d (arg: %d)\n", compId, argSize);

    objCompTablePtr->type = compId;
    objCompTablePtr->flags = 0;
    objCompTablePtr->offset = objCompDataPtr - (char*)obj;
    ++objCompTablePtr;

    if(deferComponentInit)
    {
      auto &pending = pendingCompInit.emplace_back();
      pending.obj = obj;
      pending.dataPtr = objCompDataPtr;
      pending.compId = compId;
      pending.initData = ptrIn + 4;
    }
    else
    {
      compDef.initDel(*obj, objCompDataPtr, ptrIn + 4);
    }

    objCompDataPtr += Math::alignUp(compDef.getAllocSize(ptrIn + 4), 8);
    ptrIn += argSize;
  }

  /*debugf("Object: id=%d | group=%d | flags=0x%04X | pos=(%f,%f,%f) | comp: %d\n",
    obj->id, obj->group, obj->flags,
    (double)obj->pos.x, (double)obj->pos.y, (double)obj->pos.z,
    compCount
  );*/

  objFile = ptrIn + 4;

  objects.push_back(obj);
  if(obj->id < idLookup.size()) {
    idLookup[obj->id] = obj;
  }

  return obj;
}

void P64::Scene::runPendingComponentInit()
{
  for(auto &pending : pendingCompInit)
  {
    const auto &compDef = COMP_TABLE[pending.compId];
    compDef.initDel(*pending.obj, pending.dataPtr, pending.initData);
  }
  pendingCompInit.clear();
}

void P64::Scene::loadScene() {
  updateScenePath(id);
  scenePath[sizeof(scenePath)-2] = '\0';

  cameras.clear();

  //debugf("Objects: %lu\n", conf.objectCount);
  if(conf.objectCount)
  {
    auto *objFileStart = (uint8_t*)(loadSubFile('o'));

    // now process all other objects
    auto objFile = objFileStart;
    for(uint32_t i=0; i<conf.objectCount; ++i) {
      loadObject(objFile, {}, true);
    }

    // run component init only after all objects are registered in the scene
    runPendingComponentInit();

    free(objFileStart);
  }

  // update groups
  for(auto obj : objects)
  {
    if(obj->hasChildren())
    {
      bool groupActive = obj->isSelfEnabled();
      debugf("Updating group %d | a:%d\n", obj->id, groupActive);
      setGroupEnabled(obj->id, groupActive);
    }
  }
}
