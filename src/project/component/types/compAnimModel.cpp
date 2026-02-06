/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "../components.h"
#include "../../../context.h"
#include "../../../editor/imgui/helper.h"
#include "../../../editor/imgui/lang.h"
#include "../../../utils/json.h"
#include "../../../utils/jsonBuilder.h"
#include "../../../utils/binaryFile.h"
#include "../../../utils/logger.h"
#include "../../assetManager.h"
#include "../../../editor/pages/parts/viewport3D.h"
#include "../../../renderer/scene.h"
#include "../../../utils/meshGen.h"
#include "../../../shader/defines.h"
#include "../shared/material.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"

#include "../shared/meshFilter.h"

namespace Project::Component::AnimModel
{
  struct Data
  {
    PROP_U64(model);
    PROP_S32(layerIdx);

    Shared::Material material{};

    Renderer::Object obj3D{};
    Utils::AABB aabb{};
  };

  std::shared_ptr<void> init(Object &obj) {
    return std::make_shared<Data>();
  }

  nlohmann::json serialize(const Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    return Utils::JSON::Builder{}
      .set(data.model)
      .set(data.layerIdx)
      .set("material", data.material.serialize())
      .doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->layerIdx);
    Utils::JSON::readProp(doc, data->model);

    data->material.deserialize(
      doc.value("material", nlohmann::json::object())
    );
    return data;
  }

  void build(Object& obj, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    auto res = ctx.assetUUIDToIdx.find(data.model.value);
    uint16_t id = 0xDEAD;
    if (res == ctx.assetUUIDToIdx.end()) {
      Utils::Logger::log("Component Model: Model UUID not found: " + std::to_string(entry.uuid), Utils::Logger::LEVEL_ERROR);
    } else {
      id = res->second;
    }

    ctx.fileObj.write<uint16_t>(id);
    ctx.fileObj.write<uint8_t>(data.layerIdx.resolve(obj));
    ctx.fileObj.write<uint8_t>(0); // flags, unused
    data.material.build(ctx.fileObj, obj);
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    auto &assets = ctx.project->getAssets();
    auto &modelList = assets.getTypeEntries(FileType::MODEL_3D);
    auto scene = ctx.project->getScenes().getLoadedScene();

    if (ImTable::start("Comp", &obj)) {
      ImTable::add(Editor::message(MSG_OBJECT_COMPONENT_ANIMATED_MODEL_NAME), entry.name);
      ImTable::add(Editor::message(MSG_OBJECT_COMPONENT_ANIMATED_MODEL_MODEL));

      if (ImGui::VectorComboBox(Editor::message(MSG_OBJECT_COMPONENT_ANIMATED_MODEL_MODEL), modelList, data.model.value)) {
        data.obj3D.removeMesh();
      }

      std::vector<const char*> layerNames{};
      for (auto &layer : scene->conf.layers3D) {
        layerNames.push_back(layer.name.value.c_str());
      }

      ImTable::addObjProp<int32_t>(Editor::message(MSG_OBJECT_COMPONENT_ANIMATED_MODEL_DRAW_LAYER), data.layerIdx, [&layerNames](int32_t *layer)
        {
          return ImGui::Combo("##", layer, layerNames.data(), layerNames.size());
        }, nullptr);


      ImTable::end();

      if(ImGui::CollapsingSubHeader(Editor::message(MSG_OBJECT_COMPONENT_ANIMATED_MODEL_MATERIAL_SETS), ImGuiTreeNodeFlags_DefaultOpen) && ImTable::start("Mat", &obj))
      {
        ImTable::addObjProp<int32_t>(Editor::message(MSG_OBJECT_COMPONENT_ANIMATED_MODEL_DEPTH), data.material.depth, [](int32_t *depth)
        {
          std::array<const char*, 4> items = {
            Editor::message(MSG_OBJECT_COMPONENT_ANIMATED_MODEL_DEPTHS_NONE),
            Editor::message(MSG_OBJECT_COMPONENT_ANIMATED_MODEL_DEPTHS_READ),
            Editor::message(MSG_OBJECT_COMPONENT_ANIMATED_MODEL_DEPTHS_WRITE),
            Editor::message(MSG_OBJECT_COMPONENT_ANIMATED_MODEL_DEPTHS_RW)
          };
          return ImGui::Combo("##", depth, items.data(), items.size());
        }, &data.material.setDepth);

        ImTable::addObjProp(Editor::message(MSG_OBJECT_COMPONENT_ANIMATED_MODEL_PRIM_COLOR), data.material.prim, &data.material.setPrim);
        ImTable::addObjProp(Editor::message(MSG_OBJECT_COMPONENT_ANIMATED_MODEL_ENV_COLOR), data.material.env, &data.material.setEnv);
        ImTable::addObjProp(Editor::message(MSG_OBJECT_COMPONENT_ANIMATED_MODEL_FRESNEL), data.material.fresnel, &data.material.setFresnel);
        if(data.material.fresnel.resolve(obj.propOverrides) != 0)
        {
          ImTable::addObjProp(Editor::message(MSG_OBJECT_COMPONENT_ANIMATED_MODEL_FRES_COLOR), data.material.fresnelColor);
        }
        // ImTable::addObjProp("Lighting", data.material.lighting, &data.material.setLighting);

        ImTable::end();
      }

      ImGui::Dummy({0,4});

    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    if (!data.obj3D.isMeshLoaded()) {
      auto asset = ctx.project->getAssets().getEntryByUUID(data.model.value);
      if (asset && asset->mesh3D) {
        if (!asset->mesh3D->isLoaded()) {
          asset->mesh3D->recreate(*ctx.scene);
        }
        data.aabb = asset->mesh3D->getAABB();
        data.obj3D.setMesh(asset->mesh3D);
      }
    }

    if(ctx.project->getScenes().getLoadedScene()->conf.renderPipeline.value == 2)
    {
      data.obj3D.uniform.mat.flags = 0;
      if(data.layerIdx.value == 0)data.obj3D.uniform.mat.flags |= T3D_FLAG_NO_LIGHT;
    }


    data.obj3D.overrides.setPrim = data.material.setPrim.resolve(obj.propOverrides);
    data.obj3D.overrides.setEnv = data.material.setEnv.resolve(obj.propOverrides);

    if(data.obj3D.overrides.setPrim) {
      data.obj3D.overrides.colPrim = data.material.prim.resolve(obj.propOverrides);
    }
    if(data.obj3D.overrides.setEnv) {
      data.obj3D.overrides.colEnv = data.material.env.resolve(obj.propOverrides);
    }

    data.obj3D.setObjectID(obj.uuid);

    // @TODO: tidy-up
    glm::vec3 skew{0,0,0};
    glm::vec4 persp{0,0,0,1};
    data.obj3D.uniform.modelMat = glm::recompose(
      obj.scale.resolve(obj.propOverrides),
      obj.rot.resolve(obj.propOverrides),
      obj.pos.resolve(obj.propOverrides),
      skew, persp);

    auto asset = ctx.project->getAssets().getEntryByUUID(data.model.value);
    if (!asset || !asset->mesh3D) {
      return;
    }

    data.obj3D.draw(pass, cmdBuff);

    bool isSelected = ctx.selObjectUUID == obj.uuid;
    if (isSelected)
    {
      Utils::AABB aabb = data.aabb;
      auto center = obj.pos.resolve(obj.propOverrides) + (aabb.getCenter() * obj.scale.resolve(obj.propOverrides) * (float)0xFFFF);
      auto halfExt = aabb.getHalfExtend() * obj.scale.resolve(obj.propOverrides) * (float)0xFFFF;

      glm::u8vec4 aabbCol{0xAA,0xAA,0xAA,0xFF};
      if (isSelected) {
        aabbCol = {0xFF,0xAA,0x00,0xFF};
      }

      Utils::Mesh::addLineBox(*vp.getLines(), center, halfExt, aabbCol);
      Utils::Mesh::addLineBox(*vp.getLines(), center, halfExt + 0.002f, aabbCol);
    }
  }
}
