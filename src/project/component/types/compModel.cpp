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

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"

namespace Project::Component::Model
{
  struct Data
  {
    uint64_t modelUUID{0};
    Renderer::Object obj3D{};
    Utils::AABB aabb{};
  };

  std::shared_ptr<void> init(Object &obj) {
    auto data = std::make_shared<Data>();
    return data;
  }

  std::string serialize(Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    Utils::JSON::Builder builder{};
    builder.set("model", data.modelUUID);
    return builder.toString();
  }

  std::shared_ptr<void> deserialize(simdjson::simdjson_result<simdjson::dom::object> &doc) {
    auto data = std::make_shared<Data>();
    data->modelUUID = doc["model"].get<uint64_t>();
    return data;
  }

  void build(Object&, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    auto res = ctx.assetUUIDToIdx.find(data.modelUUID);
    uint16_t id = 0xDEAD;
    if (res == ctx.assetUUIDToIdx.end()) {
      Utils::Logger::log("Component Model: Model UUID not found: " + std::to_string(entry.uuid), Utils::Logger::LEVEL_ERROR);
    } else {
      id = res->second;
    }

    ctx.fileObj.write<uint16_t>(id);
    ctx.fileObj.write<uint16_t>(0);
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    auto &assets = ctx.project->getAssets();
    auto &modelList = assets.getTypeEntries(AssetManager::FileType::MODEL_3D);

    if (ImGui::InpTable::start("Comp")) {
      ImGui::InpTable::addString("Name", entry.name);
      ImGui::InpTable::add("Model");
      //ImGui::InputScalar("##UUID", ImGuiDataType_U64, &data.scriptUUID);

      int idx = modelList.size();
      for (int i=0; i<modelList.size(); ++i) {
        if (modelList[i].uuid == data.modelUUID) {
          idx = i;
          break;
        }
      }

      auto getter = [](void*, int idx) -> const char*
      {
        auto &scriptList = ctx.project->getAssets().getTypeEntries(AssetManager::FileType::MODEL_3D);
        if (idx < 0 || idx >= scriptList.size())return "<Select Model>";
        return scriptList[idx].name.c_str();
      };

      if (ImGui::Combo("##UUID", &idx, getter, nullptr, modelList.size()+1)) {
        data.obj3D.removeMesh();
      }

      if (idx < modelList.size()) {
        const auto &script = modelList[idx];
        data.modelUUID = script.uuid;
      }

      ImGui::InpTable::end();
    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    if (!data.obj3D.isMeshLoaded()) {
      auto asset = ctx.project->getAssets().getEntryByUUID(data.modelUUID);
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
    data.obj3D.uniform.modelMat = glm::recompose(obj.scale, obj.rot, obj.pos, skew, persp);

    data.obj3D.draw(pass, cmdBuff);

    bool isSelected = ctx.selObjectUUID == obj.uuid;
    if (isSelected)
    {
      auto center = obj.pos + (data.aabb.getCenter() * obj.scale * (float)0xFFFF);
      auto halfExt = data.aabb.getHalfExtend() * obj.scale * (float)0xFFFF;

      glm::u8vec4 aabbCol{0xAA,0xAA,0xAA,0xFF};
      if (isSelected) {
        aabbCol = {0xFF,0xAA,0x00,0xFF};
      }

      Utils::Mesh::addLineBox(*vp.getLines(), center, halfExt, aabbCol);
      Utils::Mesh::addLineBox(*vp.getLines(), center, halfExt + 0.002f, aabbCol);
    }
  }
}
