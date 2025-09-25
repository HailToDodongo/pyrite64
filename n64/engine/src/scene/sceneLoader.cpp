/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>

#include "scene/scene.h"
#include "scene/globalState.h"

#include "lib/memory.h"
#include "lib/logger.h"
#include "lib/matrixManager.h"
#include "lib/assetManager.h"

namespace {
  constexpr uint16_t OBJ_TYPE_ACTOR_MASK  = 1 << 15;

  constexpr uint16_t OBJ_TYPE_ACTOR  = 0;
  constexpr uint16_t OBJ_TYPE_MESH   = 1;
  constexpr uint16_t OBJ_TYPE_CAMERA = 2;

  struct ObjectEntry {
    uint16_t type;
    uint16_t size;
    // data follows
  };

  struct __attribute__((packed)) ObjectEntryMesh : public ObjectEntry {
    uint32_t t3dmHash;
    char* t3dmPath;
  };

  struct __attribute__((packed)) ObjectEntryCamera : public ObjectEntry {
    uint16_t id;
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

  inline void* loadSubFile(char type) {
    scenePath[sizeof(scenePath)-2] = type;
    scenePath[sizeof(scenePath)-1] = '\0';
    return asset_load(scenePath, nullptr);
  }
}

void P64::Scene::loadScene() {
  scenePath[sizeof(scenePath)-3] = '0' + id;
  scenePath[sizeof(scenePath)-2] = '\0';

  {
    // @TODO: load into
    auto *tmp = (SceneConf*)asset_load(scenePath, nullptr);
    conf = *tmp;
    free(tmp);
  }

  stringTable = (char*)loadSubFile('s');

  if(conf.objectCount)
  {
    objStaticMats = (T3DMat4FP*)(loadSubFile('m'));
    auto currStaticMats = objStaticMats;
    data_cache_hit_writeback(objStaticMats, conf.objectCount * sizeof(T3DMat4FP));

    auto *objFileStart = (char*)(loadSubFile('o'));
    uint32_t camIdx = 0;
    uint32_t camCount = 0;

    // pre-scan objects to get counts
    auto *objFile = objFileStart;
    for(uint32_t i=0; i<conf.objectCount; ++i) {
      ObjectEntry* obj = (ObjectEntry*)objFile;
      if(obj->type == OBJ_TYPE_CAMERA)++camCount;
      objFile += obj->size;
    }

    cameras.resize(camCount);

    // only process static meshes, record all into one block
    objFile = objFileStart;
    rspq_block_begin();
    for(uint32_t i=0; i<conf.objectCount; ++i)
    {
      ObjectEntry* obj = (ObjectEntry*)objFile;
      if(obj->type == OBJ_TYPE_MESH) {
        auto* objMesh = (ObjectEntryMesh*)obj;
        objMesh->t3dmPath = (char*)((uint32_t)objMesh->t3dmPath + (uint32_t)stringTable);
        auto model = AssetManager::getT3DM(objMesh->t3dmHash, objMesh->t3dmPath);

        t3d_matrix_set(currStaticMats++, true);
        t3d_model_draw(model);
      }
      objFile += obj->size;
    }
    dplObjects = rspq_block_end();

    // now process all other objects
    objFile = objFileStart;
    for(uint32_t i=0; i<conf.objectCount; ++i)
    {
      ObjectEntry* obj = (ObjectEntry*)objFile;
      uint32_t objType = (obj->type & OBJ_TYPE_ACTOR_MASK) ? OBJ_TYPE_ACTOR : obj->type;

      switch (objType)
      {
        case OBJ_TYPE_ACTOR: {
          uint16_t actorType = obj->type & ~OBJ_TYPE_ACTOR_MASK;
          addActor(actorType, (Actor::Base::Args*)((char*)objFile + 4));
        } break;

        // ignore, was processed before already
        case OBJ_TYPE_MESH: break;

        case OBJ_TYPE_CAMERA: {
          auto* objCam = (ObjectEntryCamera*)obj;
          auto &cam = cameras[camIdx++];
          cam.setPos(objCam->pos);
          cam.fov  = objCam->fov;
          cam.near = objCam->near;
          cam.far  = objCam->far;
          for(auto &vp : cam.viewports)
          {
            vp = t3d_viewport_create();
            t3d_viewport_set_area(vp,
              objCam->vpOffset[0], objCam->vpOffset[1],
              objCam->vpSize[0], objCam->vpSize[1]
            );
          }
        } break;

        default:
          debugf("Unknown object type: %04X\n", obj->type);
          break;
      }

      objFile += obj->size;
    }

    free(objFileStart);
  }
}
