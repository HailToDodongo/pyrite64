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

    // Selected scene-object UUID per object-ref slot; resolved to a runtime id at build.
    std::map<uint16_t, uint32_t> objRefs{};

    // Per-object default for each variable (object UUID for "objref", number, or array).
    std::map<std::string, nlohmann::json> varDefaults{};

    // Editor-only cache of the selected graph's declared object slots + variables.
    uint64_t cachedAsset{};
    std::vector<::Project::Graph::ObjRefParam> cachedRefs{};
    std::vector<::Project::Graph::GraphVar> cachedVars{};
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
      .set(data.autoRun);

    auto refs = nlohmann::json::object();
    for(auto &[slot, uuid] : data.objRefs) {
      if(uuid == 0)continue;
      refs[std::to_string(slot)] = uuid;
    }
    builder.doc["objRefs"] = refs;

    auto vars = nlohmann::json::object();
    for(auto &[name, val] : data.varDefaults) {
      if(name.empty() || val.is_null())continue;
      vars[name] = val;
    }
    builder.doc["varDefaults"] = vars;
    return builder.doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->asset);
    Utils::JSON::readProp(doc, data->autoRun, true);

    if(doc.contains("objRefs")) {
      for(auto &[slot, uuid] : doc["objRefs"].items()) {
        data->objRefs[static_cast<uint16_t>(std::stoul(slot))] = uuid.get<uint32_t>();
      }
    }
    if(doc.contains("varDefaults")) {
      for(auto &[name, val] : doc["varDefaults"].items()) {
        data->varDefaults[name] = val;
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
    // @TODO: remove
    ctx.fileObj.write<uint8_t>(0); // (was 'repeatable')

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


    auto vars = graphAsset ? ::Project::Graph::Graph::getVariables(Utils::FS::loadTextFile(graphAsset->path))
                           : std::vector<::Project::Graph::GraphVar>{};
    ctx.fileObj.write<uint32_t>(::Project::Graph::varBlobBytes(vars));

    auto defAt = [&](const std::string &name) -> nlohmann::json {
      auto it = data.varDefaults.find(name);
      return it != data.varDefaults.end() ? it->second : nlohmann::json{};
    };
    auto fnum = [](const nlohmann::json &j, size_t i, float def) -> float {
      if(j.is_array() && i < j.size() && j[i].is_number())return j[i].get<float>();
      return def;
    };

    for(auto &var : vars) {
      const auto &def = defAt(var.name);
      if(var.type == "f32") {
        ctx.fileObj.write<float>(def.is_number() ? def.get<float>() : 0.0f);
      } else if(var.type == "u32") {
        ctx.fileObj.write<uint32_t>(def.is_number() ? def.get<uint32_t>() : 0u);
      } else if(var.type == "vec3") {
        ctx.fileObj.write<float>(fnum(def,0,0)); ctx.fileObj.write<float>(fnum(def,1,0)); ctx.fileObj.write<float>(fnum(def,2,0));
      } else if(var.type == "vec4") {
        ctx.fileObj.write<float>(fnum(def,0,0)); ctx.fileObj.write<float>(fnum(def,1,0));
        ctx.fileObj.write<float>(fnum(def,2,0)); ctx.fileObj.write<float>(fnum(def,3,0));
      } else if(var.type == "objref") {
        uint16_t runtimeId = 0;
        if(def.is_number() && def.get<uint64_t>() != 0) {
          auto refObj = ctx.scene ? ctx.scene->getObjectByUUID(def.get<uint64_t>()) : nullptr;
          if(refObj)runtimeId = refObj->runtimeId;
        }
        ctx.fileObj.write<uint16_t>(runtimeId);
        ctx.fileObj.write<uint16_t>(0); // padding to the 4-byte slot
      } else { // i32 and anything else: 4-byte int
        ctx.fileObj.write<int32_t>(def.is_number() ? def.get<int32_t>() : 0);
      }
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

      // Object references declared by the graph (its "Object" nodes). Re-scanned
      // whenever the selected graph changes.
      uint64_t curAsset = data.asset.resolve(obj);
      if(curAsset != data.cachedAsset) {
        data.cachedAsset = curAsset;
        data.cachedRefs.clear();
        data.cachedVars.clear();
        auto graphAsset = ctx.project->getAssets().getEntryByUUID(curAsset);
        if(graphAsset) {
          auto json = Utils::FS::loadTextFile(graphAsset->path);
          data.cachedRefs = ::Project::Graph::Graph::getObjectRefs(json);
          data.cachedVars = ::Project::Graph::Graph::getVariables(json);
        }
      }

      // Build a scene-object pick list once (shared by object refs + objref vars).
      auto buildObjList = [&]() {
        std::vector<ImTable::ComboEntry> objList;
        objList.push_back({0, "<None>"});
        auto scene = ctx.project->getScenes().getLoadedScene();
        if(scene) {
          for(auto &[uuid, object] : scene->objectsMap) objList.push_back({object->uuid, object->name});
        }
        return objList;
      };

      if(!data.cachedRefs.empty()) {
        auto objList = buildObjList();
        for(auto &ref : data.cachedRefs) {
          ImGui::PushID(ref.slot);
          ImTable::add(ref.name.empty() ? "Object" : ref.name);
          ImTable::addObjectVecComboBox("", objList, data.objRefs[ref.slot]);
          ImGui::PopID();
        }
      }

      // Per-variable default values
      if(!data.cachedVars.empty()) {
        std::vector<ImTable::ComboEntry> objList; // built lazily for objref vars
        for(auto &var : data.cachedVars) {
          if(var.name.empty())continue;
          ImGui::PushID(var.name.c_str());
          auto &def = data.varDefaults[var.name];

          if(var.type == "objref") {
            if(objList.empty()) objList = buildObjList();
            uint32_t uuid = def.is_number() ? def.get<uint32_t>() : 0u;
            ImTable::addObjectVecComboBox(var.name, objList, uuid);
            def = uuid;
          } else if(var.type == "f32") {
            ImTable::add(var.name);
            float v = def.is_number() ? def.get<float>() : 0.0f;
            ImGui::SetNextItemWidth(-1);
            if(ImGui::InputFloat("##v", &v)) def = v;
          } else if(var.type == "vec3" || var.type == "vec4") {
            int n = (var.type == "vec4") ? 4 : 3;
            float v[4] = {0,0,0,0};
            for(int i=0;i<n;++i) if(def.is_array() && (size_t)i<def.size() && def[i].is_number()) v[i]=def[i].get<float>();
            ImTable::add(var.name);
            ImGui::SetNextItemWidth(-1);
            bool ch = (n == 4) ? ImGui::InputFloat4("##v", v) : ImGui::InputFloat3("##v", v);
            if(ch) { def = nlohmann::json::array(); for(int i=0;i<n;++i) def.push_back(v[i]); }
          } else { // i32 / u32: integer field
            ImTable::add(var.name);
            int v = def.is_number() ? def.get<int>() : 0;
            ImGui::SetNextItemWidth(-1);
            if(ImGui::InputInt("##v", &v)) def = v;
          }
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
