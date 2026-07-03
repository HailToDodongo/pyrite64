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
#include "../shared/materialInstance.h"
#include "../../../editor/pages/editorScene.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "../../../editor/pages/parts/assets/matInstanceEditor.h"
#include "../../../renderer/animation.h"
#include "../../../renderer/skeleton.h"
#include "glm/gtx/matrix_decompose.hpp"

#include "../shared/meshFilter.h"

namespace Project::Component::AnimModel
{
  constexpr size_t LAYER_COUNT = 4; // layer 0 = base
  constexpr uint8_t ANIM_FLAG_LOOP     = 1 << 0;
  constexpr uint8_t ANIM_FLAG_AUTOPLAY = 1 << 1;

  // One animation layer: an animation, optionally blended with a second one.
  // 'active' marks overlay layers the user has created (layer 0 is always shown).
  struct AnimEntry
  {
    std::string anim{};   // animation played on this layer
    std::string blend{};  // optional second animation blended over the first
    float blendFactor{0.0f};
    bool loop{true};
    bool autoplay{true};
    float speed{1.0f};
    bool active{false};
  };

  struct Data
  {
    PROP_U64(model);
    PROP_S32(layerIdx);
    PROP_STRING(previewAnimName); // legacy, migrated into animEntries[0]

    std::array<AnimEntry, LAYER_COUNT> animEntries{};

    Shared::MaterialInstance material{};
    std::shared_ptr<Renderer::Skeleton> skeleton{nullptr};
    Renderer::Animation anim{};

    Renderer::Object obj3D{};
    Utils::AABB aabb{};
  };

  std::shared_ptr<void> init(Object &obj) {
    return std::make_shared<Data>();
  }

  nlohmann::json serialize(const Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    // write layer 0 (base) plus every active overlay, contiguous so their
    // array position maps back to a layer index on load
    size_t count = 0;
    for (size_t i = 1; i < LAYER_COUNT; ++i) {
      if (data.animEntries[i].active) count = i + 1;
    }
    if (count == 0 && (!data.animEntries[0].anim.empty() || !data.animEntries[0].blend.empty())) {
      count = 1;
    }

    nlohmann::json anims = nlohmann::json::array();
    for (size_t i = 0; i < count; ++i) {
      const auto &e = data.animEntries[i];
      anims.push_back({
        {"anim", e.anim},
        {"blend", e.blend},
        {"factor", e.blendFactor},
        {"loop", e.loop},
        {"autoplay", e.autoplay},
        {"speed", e.speed},
      });
    }

    return Utils::JSON::Builder{}
      .set(data.model)
      .set(data.layerIdx)
      .set("animations", anims)
      .set("material", data.material.serialize())
      .doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->layerIdx);
    Utils::JSON::readProp(doc, data->previewAnimName);
    Utils::JSON::readProp(doc, data->model);

    if (doc.contains("animations")) {
      size_t i = 0;
      for (auto &e : doc["animations"]) {
        if (i >= LAYER_COUNT) break;
        auto &entry = data->animEntries[i++];
        entry.anim = e.value("anim", e.value("name", std::string{})); // "name" = legacy key
        entry.blend = e.value("blend", std::string{});
        entry.blendFactor = e.value("factor", 0.0f);
        entry.loop = e.value("loop", true);
        entry.autoplay = e.value("autoplay", true);
        entry.speed = e.value("speed", 1.0f);
        entry.active = true; // present in the saved array => this layer exists
      }
    } else if (!data->previewAnimName.value.empty() && data->previewAnimName.value != "<Default Pose>") {
      // migrate the old preview-only setting into the base animation layer
      data->animEntries[0].anim = data->previewAnimName.value;
    }

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

    // animation layers, names resolved to indices in the model's anim list
    auto asset = ctx.project->getAssets().getEntryByUUID(data.model.value);
    auto resolveAnim = [&](const std::string &name) -> uint8_t {
      if (name.empty() || !asset) return 0xFF;
      const auto &anims = asset->model.t3dm.animations;
      for (size_t i = 0; i < anims.size(); ++i) {
        if (anims[i].name == name) return (uint8_t)i;
      }
      Utils::Logger::log(
        "Component AnimModel: animation '" + name + "' not found in model",
        Utils::Logger::LEVEL_ERROR
      );
      return 0xFF;
    };

    for (const auto &e : data.animEntries) {
      uint8_t animFlags = (e.loop ? ANIM_FLAG_LOOP : 0)
                        | (e.autoplay ? ANIM_FLAG_AUTOPLAY : 0);
      ctx.fileObj.write<uint8_t>(resolveAnim(e.anim));
      ctx.fileObj.write<uint8_t>(resolveAnim(e.blend));
      ctx.fileObj.write<uint8_t>(animFlags);
      ctx.fileObj.write<uint8_t>(0); // padding
      ctx.fileObj.write<float>(e.speed);
      ctx.fileObj.write<float>(e.blendFactor);
    }

    data.material.validateWithModel(
      ctx.project->getAssets().getEntryByUUID(data.model.value)->model
    );
    data.material.build(ctx.fileObj, ctx, obj);
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    auto &assets = ctx.project->getAssets();
    auto &modelList = assets.getTypeEntries(FileType::MODEL_3D);
    auto scene = ctx.project->getScenes().getLoadedScene();

    if (ImTable::start("Comp", &obj)) {
      ImTable::add("Name", entry.name);
      ImTable::addAssetVecComboBox("Model", modelList, data.model.value, [&data](auto) { data.obj3D.removeMesh(); });

      ImTable::add("");
      if(ImGui::Button(ICON_MDI_PENCIL " Open Model Editor")) {
        ctx.editorScene->openModelEditor(data.model.value);
      }

      std::vector<const char*> layerNames{};
      for (auto &layer : scene->conf.layers3D) {
        layerNames.push_back(layer.name.value.c_str());
      }

      ImTable::addObjProp<int32_t>("Draw-Layer", data.layerIdx, [&layerNames](int32_t *layer)
        {
          return ImGui::Combo("##", layer, layerNames.data(), layerNames.size());
        }, nullptr);

      ImTable::end();

      auto asset = ctx.project->getAssets().getEntryByUUID(data.model.value);
      if (asset && asset->mesh3D)
      {
          const auto &animList = asset->model.t3dm.animations;
          std::vector<const char*> animNames{};
          animNames.push_back("<None>");
          for(auto &anim : animList) {
            animNames.push_back(anim.name.c_str());
          }

          auto drawAnimCombo = [&](std::string &name) {
            int selIdx = 0;
            for(size_t a = 0; a < animList.size(); ++a) {
              if(animList[a].name == name) { selIdx = (int)a + 1; break; }
            }
            if(ImGui::Combo("##anim", &selIdx, animNames.data(), animNames.size())) {
              name = (selIdx <= 0) ? "" : animList[selIdx - 1].name;
              return true;
            }
            return false;
          };

          // one collapsible section per layer, each holding a main + optional
          // blend animation. Layer 0 is the always-present base layer.
          auto drawLayerBody = [&](AnimEntry &e) {
            ImTable::add("Animation");
            drawAnimCombo(e.anim);

            ImTable::add("Blend With");
            ImGui::PushID("blend");
            drawAnimCombo(e.blend);
            ImGui::PopID();
            if(!e.blend.empty()) {
              ImTable::add("Blend Factor");
              ImGui::SliderFloat("##blendFactor", &e.blendFactor, 0.0f, 1.0f, "%.2f");
            }

            ImTable::add("Playback");
            ImGui::Checkbox("Loop", &e.loop);
            ImGui::SameLine();
            ImGui::Checkbox("Play", &e.autoplay);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(52_px);
            ImGui::DragFloat("##speed", &e.speed, 0.01f, 0.0f, 10.0f, "%.2f");
            ImGui::SetItemTooltip("Playback speed");
          };

          if(ImGui::CollapsingSubHeader("Base Layer", ImGuiTreeNodeFlags_DefaultOpen)) {
            if(ImTable::start("animBase", &obj)) {
              drawLayerBody(data.animEntries[0]);
              ImTable::end();
            }
          }

          int removeIdx = -1;
          for(size_t i = 1; i < LAYER_COUNT; ++i) {
            auto &e = data.animEntries[i];
            if(!e.active) continue;

            ImGui::PushID((int)i);
            std::string title = "Layer " + std::to_string(i);
            ImGui::SetNextItemAllowOverlap();
            bool open = ImGui::CollapsingSubHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

            const float btnSize = 19_px;
            ImGui::SameLine(ImGui::GetContentRegionMax().x - btnSize - 4_px);
            if(ImGui::Button(ICON_MDI_TRASH_CAN "##rm", {btnSize, btnSize})) {
              removeIdx = (int)i;
            }
            ImGui::SetItemTooltip("Remove layer");

            if(open) {
              if(ImTable::start(("animL" + std::to_string(i)).c_str(), &obj)) {
                drawLayerBody(e);
                ImTable::end();
              }
            }
            ImGui::PopID();
          }

          // remove: shift overlay layers down so they stay contiguous
          if(removeIdx >= 1) {
            for(size_t i = (size_t)removeIdx; i + 1 < LAYER_COUNT; ++i) {
              data.animEntries[i] = data.animEntries[i + 1];
            }
            data.animEntries[LAYER_COUNT - 1] = {};
          }

          int freeSlot = -1;
          for(size_t i = 1; i < LAYER_COUNT; ++i) {
            if(!data.animEntries[i].active) { freeSlot = (int)i; break; }
          }
          if(freeSlot >= 1) {
            if(ImGui::Button(ICON_MDI_PLUS " Add Layer")) {
              data.animEntries[freeSlot] = {};
              data.animEntries[freeSlot].active = true;
            }
          }
      }

      Editor::MatInstanceEditor::draw(data.material, obj, data.model.value);
      ImGui::Dummy({0,4});
    }
  }

  void drawCopyPass(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPUCopyPass* pass)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    if(data.skeleton) {
      data.skeleton->update(*pass);
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
        data.skeleton = std::make_shared<Renderer::Skeleton>(ctx.gpu, asset->model, asset->conf.baseScale);
      }
    }

    if(ctx.project->getScenes().getLoadedScene()->conf.renderPipeline.value == 2)
    {
      data.obj3D.uniform.mat.flags = 0;
      if(data.layerIdx.value == 0)data.obj3D.uniform.mat.flags |= T3D_FLAG_NO_LIGHT;
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

    for(auto &anim : asset->model.t3dm.animations)
    {
      if(anim.name == data.animEntries[0].anim) {
        float deltaTime = ImGui::GetIO().DeltaTime * data.animEntries[0].speed;
        data.anim.update(anim, data.skeleton, deltaTime);
        break;
      }
    }

    data.skeleton->use(pass);
    data.obj3D.draw(pass, cmdBuff, {
      .partsIndices = {},
      .model = &asset->model,
      .matInstance = &data.material,
      .obj = obj
    });

    bool isSelected = ctx.isObjectSelected(obj.uuid);
    if (isSelected)
    {
      Utils::AABB aabb = data.aabb;
      auto center = obj.pos.resolve(obj.propOverrides) + (aabb.getCenter() * obj.scale.resolve(obj.propOverrides) * (float)0xFFFF);
      auto halfExt = aabb.getHalfExtend() * obj.scale.resolve(obj.propOverrides) * (float)0xFFFF;

      glm::u8vec4 aabbCol{0xAA,0xAA,0xAA,0xFF};
      if (isSelected) {
        aabbCol = {0xFF,0xAA,0x00,0xFF};
      }

      auto rot = obj.rot.resolve(obj.propOverrides);
      Utils::Mesh::addLineBox(*vp.getLines(), center, halfExt, aabbCol, rot);
      Utils::Mesh::addLineBox(*vp.getLines(), center, halfExt + 0.002f, aabbCol, rot);
    }
  }

  Utils::AABB getAABB(Object &obj, Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    Utils::AABB aabb = data.aabb;
    aabb.min *= (float)0xFFFF;
    aabb.max *= (float)0xFFFF;
    return aabb;
  }
}
