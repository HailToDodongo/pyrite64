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
#include "../../../editor/actions.h"
#include "../../../editor/pages/parts/viewport3D.h"
#include "../../../renderer/scene.h"
#include "../../../utils/meshGen.h"

namespace Project::Component::NodeGraph
{
  struct Data
  {
    PROP_U64(asset);
    PROP_BOOL(autoRun);
    PROP_BOOL(repeatable);
  };

  std::shared_ptr<void> init(Object &obj) {
    auto data = std::make_shared<Data>();
    data->autoRun.value = true;
    return data;
  }

  nlohmann::json serialize(const Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    return Utils::JSON::Builder{}
      .set(data.asset)
      .set(data.autoRun)
      .set(data.repeatable)
      .doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->asset);
    Utils::JSON::readProp(doc, data->autoRun, true);
    Utils::JSON::readProp(doc, data->repeatable, false);
    return data;
  }

  void build(Object& obj, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    auto res = ctx.assetUUIDToIdx.find(data.asset.resolve(obj));
    uint16_t id = 0xDEAD;
    if (res == ctx.assetUUIDToIdx.end()) {
      Utils::Logger::log("Component NodeGraph: UUID not found: " + std::to_string(entry.uuid), Utils::Logger::LEVEL_ERROR);
    } else {
      id = res->second;
    }

    ctx.fileObj.write<uint16_t>(id);
    ctx.fileObj.write<uint8_t>(data.autoRun.resolve(obj) ? 1 : 0);
    ctx.fileObj.write<uint8_t>(data.repeatable.resolve(obj) ? 1 : 0);
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    if (ImTable::start("Comp", &obj)) {
      ImTable::add("Name", entry.name);
      auto &assetList = ctx.project->getAssets().getTypeEntries(FileType::NODE_GRAPH);
      ImTable::addAssetVecComboBox("File", assetList, data.asset.value);

      ImTable::addObjProp("Auto Run", data.autoRun);
      ImTable::addObjProp("Repeatable", data.repeatable);

      ImTable::add("Action");
      if(ImGui::Button(ICON_MDI_PENCIL " Edit")) {
        Editor::Actions::call(Editor::Actions::Type::OPEN_NODE_GRAPH, std::to_string(data.asset.resolve(obj)));
      }

      ImGui::SameLine();
      if(ImGui::Button(ICON_MDI_PLUS " Create")) {
        ImGui::OpenPopup("NewGraph");
      }

      if(ImGui::BeginPopup("NewGraph"))
      {
        static char scriptName[128] = "NodeGraph";
        ImGui::Text("Enter name:");
        ImGui::InputText("##Name", scriptName, sizeof(scriptName));
        if (ImGui::Button("Create")) {
          data.asset.value = ctx.project->getAssets().createNodeGraph(scriptName);
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      ImTable::end();
    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    //Data &data = *static_cast<Data*>(entry.data.get());
    //Utils::Mesh::addSprite(*vp.getSprites(), obj.pos.resolve(obj.propOverrides), obj.uuid, 4);
  }
}
