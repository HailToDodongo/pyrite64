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
#include "../../../utils/fs.h"
#include "../../graph/graph.h"

#include <map>
#include <algorithm>

namespace Project::Component::NodeGraph
{
  struct Data
  {
    PROP_U64(asset);
    PROP_BOOL(autoRun);
    PROP_BOOL(repeatable);

    // Object references the graph declares (via "Object" nodes), keyed by slot.
    // Stores the selected scene object UUID; resolved to a runtime id at build time.
    std::map<uint16_t, uint32_t> objRefs{};

    // Editor-only cache of the selected graph's declared object slots.
    uint64_t cachedAsset{};
    std::vector<::Project::Graph::ObjRefParam> cachedRefs{};
  };

  std::shared_ptr<void> init(Object &obj) {
    auto data = std::make_shared<Data>();
    data->autoRun.value = true;
    return data;
  }

  nlohmann::json serialize(const Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    auto builder = Utils::JSON::Builder{}
      .set(data.asset)
      .set(data.autoRun)
      .set(data.repeatable);

    auto refs = nlohmann::json::object();
    for(auto &[slot, uuid] : data.objRefs) {
      if(uuid == 0)continue;
      refs[std::to_string(slot)] = uuid;
    }
    builder.doc["objRefs"] = refs;
    return builder.doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->asset);
    Utils::JSON::readProp(doc, data->autoRun, true);
    Utils::JSON::readProp(doc, data->repeatable, false);

    if(doc.contains("objRefs")) {
      for(auto &[slot, uuid] : doc["objRefs"].items()) {
        data->objRefs[static_cast<uint16_t>(std::stoul(slot))] = uuid.get<uint32_t>();
      }
    }
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

    // Object references: write a dense array [0..maxSlot] of resolved runtime ids.
    // Must stay in sync with P64::NodeGraph::MAX_OBJ_REFS in the engine.
    constexpr int MAX_OBJ_REFS = 8;
    int count = 0;
    auto graphAsset = ctx.project->getAssets().getEntryByUUID(data.asset.resolve(obj));
    if(graphAsset) {
      for(auto &ref : ::Project::Graph::Graph::getObjectRefs(Utils::FS::loadTextFile(graphAsset->path))) {
        count = std::max(count, ref.slot + 1);
      }
    }
    if(count > MAX_OBJ_REFS)count = MAX_OBJ_REFS;

    ctx.fileObj.write<uint8_t>(static_cast<uint8_t>(count));
    ctx.fileObj.write<uint8_t>(0); // padding (keeps the following u16 array aligned)

    for(int slot=0; slot<count; ++slot) {
      uint16_t runtimeId = 0;
      auto it = data.objRefs.find(static_cast<uint16_t>(slot));
      if(it != data.objRefs.end() && it->second != 0) {
        auto refObj = ctx.scene ? ctx.scene->getObjectByUUID(it->second) : nullptr;
        if(refObj)runtimeId = refObj->runtimeId;
      }
      ctx.fileObj.write<uint16_t>(runtimeId);
    }
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

      // Object references declared by the graph (its "Object" nodes). Re-scanned
      // whenever the selected graph changes.
      uint64_t curAsset = data.asset.resolve(obj);
      if(curAsset != data.cachedAsset) {
        data.cachedAsset = curAsset;
        data.cachedRefs.clear();
        auto graphAsset = ctx.project->getAssets().getEntryByUUID(curAsset);
        if(graphAsset) {
          data.cachedRefs = ::Project::Graph::Graph::getObjectRefs(Utils::FS::loadTextFile(graphAsset->path));
        }
      }

      if(!data.cachedRefs.empty()) {
        std::vector<ImTable::ComboEntry> objList;
        objList.push_back({0, "<None>"});
        auto scene = ctx.project->getScenes().getLoadedScene();
        if(scene) {
          for(auto &[uuid, object] : scene->objectsMap) {
            objList.push_back({object->uuid, object->name});
          }
        }

        for(auto &ref : data.cachedRefs) {
          ImGui::PushID(ref.slot);
          ImTable::add(ref.name.empty() ? "Object" : ref.name);
          ImTable::addObjectVecComboBox("", objList, data.objRefs[ref.slot]);
          ImGui::PopID();
        }
      }

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
