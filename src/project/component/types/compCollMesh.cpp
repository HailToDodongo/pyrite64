/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "../components.h"
#include "../../../context.h"
#include "../../../editor/imgui/helper.h"
#include "../../../utils/json.h"
#include "../../../utils/jsonBuilder.h"
#include "../../../utils/binaryFile.h"
#include "../../../utils/logger.h"
#include "../../assetManager.h"
#include "../../../editor/pages/parts/viewport3D.h"
#include "../../../renderer/scene.h"
#include "../../../utils/meshGen.h"
#include "../../../shader/defines.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "../../../build/projectBuilder.h"
#include "../shared/meshFilter.h"
#include "glm/gtx/matrix_decompose.hpp"

namespace Project::Component::CollMesh
{
  struct Data
  {
    PROP_U64(modelUUID);
    Shared::MeshFilter filter{};
    Renderer::Object obj3D{};
    Utils::AABB aabb{};
  };

  std::shared_ptr<void> init(Object &obj) {
    auto data = std::make_shared<Data>();
    return data;
  }

  nlohmann::json serialize(const Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    return Utils::JSON::Builder{}
      .set(data.modelUUID)
      .set(data.filter.meshFilter)
      .doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->modelUUID);
    Utils::JSON::readProp(doc, data->filter.meshFilter);
    return data;
  }

  void build(Object& obj, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    auto modelUUID = data.modelUUID.resolve(obj.propOverrides);
    auto t3dm = ctx.project->getAssets().getEntryByUUID(modelUUID);
    if(!t3dm) {
      throw std::runtime_error("Component Model: Model UUID not found: " + std::to_string(entry.uuid));
    }

    uint16_t id = 0xDEAD;
    uint8_t flags = 0;

    // we need a part of a collision mesh, by default (and for perf. reasons)
    // the entire mesh is converted by default.
    // generate a unique file for that instance (and do so via a hash to allow sharing)
    auto &meshes = data.filter.filterT3DM(t3dm->t3dmData.models, obj, false);
    if(!meshes.empty())
    {
      flags |= 1;
      modelUUID = t3dm->getId();
      char* meshIdxData = (char*)meshes.data();
      modelUUID ^= Utils::Hash::crc64(std::string_view{meshIdxData, meshes.size() * sizeof(uint32_t)});

      auto res = ctx.assetUUIDToIdx.find(modelUUID);
      if (res == ctx.assetUUIDToIdx.end())
      {
        std::unordered_set<std::string> meshNames{};
        for(auto meshIdx : meshes) {
          meshNames.insert(t3dm->t3dmData.models[meshIdx].name);
        }

        Build::buildT3DCollision(*ctx.project, ctx, meshNames, t3dm->getId(), modelUUID);
      }
    }

    auto res = ctx.assetUUIDToIdx.find(modelUUID);
    if (res == ctx.assetUUIDToIdx.end()) {
      Utils::Logger::log("Component Model: Model UUID not found: " + std::to_string(entry.uuid), Utils::Logger::LEVEL_ERROR);
    } else {
      id = res->second;
    }

    ctx.fileObj.write<uint16_t>(id);
    ctx.fileObj.write<uint8_t>(flags);
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    auto &assets = ctx.project->getAssets();
    auto &modelList = assets.getTypeEntries(FileType::MODEL_3D);

    if (ImTable::start("Comp", &obj)) {
      ImTable::add("Name", entry.name);
      ImTable::add("Model");
      //ImGui::InputScalar("##UUID", ImGuiDataType_U64, &data.scriptUUID);

      int idx = modelList.size();
      for (int i=0; i<modelList.size(); ++i) {
        if (modelList[i].getUUID() == data.modelUUID.resolve(obj.propOverrides)) {
          idx = i;
          break;
        }
      }

      auto getter = [](void*, int idx) -> const char*
      {
        auto &scriptList = ctx.project->getAssets().getTypeEntries(FileType::MODEL_3D);
        if (idx < 0 || idx >= scriptList.size())return "<Select Model>";
        return scriptList[idx].name.c_str();
      };

      if (ImGui::Combo("##UUID", &idx, getter, nullptr, modelList.size()+1)) {
        data.obj3D.removeMesh();
      }

      const AssetManagerEntry* selModel = nullptr;
      if (idx < (int)modelList.size()) {
        selModel = &modelList[idx];
        data.modelUUID.value = selModel->getUUID();
      }

      ImTable::end();

      if(selModel && ImGui::CollapsingSubHeader("Mesh Filter", ImGuiTreeNodeFlags_DefaultOpen) && ImTable::start("Filter", &obj))
      {
        bool changed = ImTable::addObjProp("Filter", data.filter.meshFilter);

        if(changed || data.filter.cache.empty()) {
          data.filter.filterT3DM(selModel->t3dmData.models, obj, false);
        }

        for(auto idx : data.filter.cache) {
          ImGui::Text("%s@%s",
            selModel->t3dmData.models[idx].name.c_str(),
            selModel->t3dmData.models[idx].material.name.c_str()
          );
        }

        ImTable::end();
      }
    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    if (!data.obj3D.isMeshLoaded()) {
      auto asset = ctx.project->getAssets().getEntryByUUID(data.modelUUID.resolve(obj.propOverrides));
      if (asset && asset->mesh3D) {
        if (!asset->mesh3D->isLoaded()) {
          asset->mesh3D->recreate(*ctx.scene);
        }
        data.aabb = asset->mesh3D->getAABB();
        data.obj3D.setMesh(asset->mesh3D);
      }
    }

    data.obj3D.setObjectID(obj.uuid);
    //data.obj3D.setPos(obj.pos);

    // @TODO: tidy-up
    glm::vec3 skew{0,0,0};
    glm::vec4 persp{0,0,0,1};
    data.obj3D.uniform.modelMat = glm::recompose(
      obj.scale.resolve(obj.propOverrides),
      obj.rot.resolve(obj.propOverrides),
      obj.pos.resolve(obj.propOverrides),
      skew, persp);
    data.obj3D.uniform.mat.flags |= DRAW_SHADER_COLLISION;

    auto asset = ctx.project->getAssets().getEntryByUUID(data.modelUUID.value);
    if (!asset || !asset->mesh3D) {
      return;
    }
    auto &meshes = data.filter.filterT3DM(asset->t3dmData.models, obj, false);

    data.obj3D.draw(pass, cmdBuff, meshes);

    bool isSelected = ctx.isObjectSelected(obj.uuid);
    if (isSelected)
    {
      auto center = obj.pos.resolve(obj.propOverrides) + (data.aabb.getCenter() * obj.scale.resolve(obj.propOverrides) * (float)0xFFFF);
      auto halfExt = data.aabb.getHalfExtend() * obj.scale.resolve(obj.propOverrides) * (float)0xFFFF;

      glm::u8vec4 aabbCol{0xAA,0xAA,0xAA,0xFF};
      if (isSelected) {
        aabbCol = {0xFF,0xAA,0x00,0xFF};
      }

      Utils::Mesh::addLineBox(*vp.getLines(), center, halfExt, aabbCol);
      Utils::Mesh::addLineBox(*vp.getLines(), center, halfExt + 0.002f, aabbCol);
    }
  }
}
