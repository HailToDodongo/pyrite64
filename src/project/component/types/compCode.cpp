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
#include "../../../utils/string.h"

namespace Project::Component::Code
{
  struct Data
  {
    uint64_t scriptUUID{0};
    std::unordered_map<std::string, PropString> args{};
  };

  std::shared_ptr<void> init(Object &obj) {
    auto data = std::make_shared<Data>();
    return data;
  }

  nlohmann::json serialize(const Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    Utils::JSON::Builder builder{};
    builder.set("script", data.scriptUUID);

    auto args = nlohmann::json::object();
    for (auto &arg : data.args) {
      args[arg.second.name] = arg.second.value;
    }
    builder.doc["args"] = args;

    return builder.doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    data->scriptUUID = doc["script"].get<uint64_t>();
    if (doc.contains("args")) {
      auto &argsObj = doc["args"];
      for (auto& [key, val] : argsObj.items()) {
        data->args[key] = PropString{key, val.get<std::string>()};
      }
    }
    return data;
  }

  void build(Object& obj, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    auto idRes = ctx.codeIdxMapUUID.find(data.scriptUUID);
    uint16_t id = 0xDEAD;
    if (idRes == ctx.codeIdxMapUUID.end()) {
      Utils::Logger::log("Component Code: Script UUID not found: " + std::to_string(entry.uuid), Utils::Logger::LEVEL_ERROR);
    } else {
      id = idRes->second;
    }

    ctx.fileObj.write<uint16_t>(id);
    ctx.fileObj.write<uint16_t>(0);

    auto script = ctx.project->getAssets().getEntryByUUID(data.scriptUUID);
    if (!script)return;

    for (auto &field : script->params.fields) {
      auto &prop = data.args[field.name];
      auto val = prop.resolve(obj.propOverrides);
      if (val.empty())val = field.defaultValue;
      if (val.empty())val = "0";

      if(field.type == Utils::DataType::ASSET_SPRITE)
      {
        uint64_t uuid = Utils::parseU64(val);
        ctx.fileObj.write<uint32_t>(ctx.assetUUIDToIdx[uuid]);
      } else if(field.type == Utils::DataType::OBJECT_REF) {
        uint32_t uuid = static_cast<uint32_t>(Utils::parseU64(val));
        auto refObj = ctx.scene->getObjectByUUID(uuid);
        uint16_t objId = refObj ? refObj->id : 0;
        ctx.fileObj.write<uint32_t>(objId);
      } else {
        ctx.fileObj.writeAs(val, field.type);
      }
    }
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    auto &assets = ctx.project->getAssets();
    auto &scriptList = assets.getTypeEntries(FileType::CODE_OBJ);

    if (ImTable::start("Comp", &obj)) {
      ImTable::add("Name", entry.name);
      ImTable::addAssetVecComboBox("Script", scriptList, data.scriptUUID);

      auto script = assets.getEntryByUUID(data.scriptUUID);
      if (script) {

        ImTable::add("Arguments:");
        if (script->params.fields.empty()) {
          ImGui::Text("(None)");
        }

        for (auto &field : script->params.fields)
        {
          std::string name{};
          auto metaName = field.attr.find("P64::Name");
          if (metaName == field.attr.end())continue;
          name = metaName->second;

          if(data.args.find(field.name) == data.args.end()) {
            data.args[field.name] = PropString{field.name, field.defaultValue};
            data.args[field.name].id = Utils::Hash::randomU64();
          }

          if(field.type == Utils::DataType::ASSET_SPRITE)
          {
            const auto &assets = ctx.project->getAssets().getTypeEntries(FileType::IMAGE);
            uint64_t uuid = Utils::parseU64(data.args[field.name].value);
            ImTable::addAssetVecComboBox(name, assets, uuid, [&](uint64_t newId) {
              data.args[field.name].value = std::to_string(newId);
            });
          } else if(field.type == Utils::DataType::OBJECT_REF) {
            // @TODO: do this in scene itself
            auto &map = ctx.project->getScenes().getLoadedScene()->objectsMap;
            std::vector<ImTable::ComboEntry> objList;
            objList.push_back({0, "<None>"});

            for (auto &[id, object] : map) {
              objList.push_back({
                .value = object->uuid,
                .name = object->name,
              });
            }

            uint32_t uuid = static_cast<uint32_t>(Utils::parseU64(data.args[field.name].value));
            ImTable::addObjectVecComboBox(name, objList, uuid, [&](uint32_t newId) {
              data.args[field.name].value = std::to_string(newId);
            });
          } else {
            ImTable::addObjProp(name, data.args[field.name]);
          }
        }
      }

      ImTable::end();
    }
  }
}
