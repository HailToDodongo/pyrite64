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
      } else {
        ctx.fileObj.writeAs(val, field.type);
      }
    }
  }

  const char* getter(void* user_data, int idx)
  {
    auto &scriptList = ctx.project->getAssets().getTypeEntries(FileType::CODE_OBJ);
    if (idx < 0 || idx >= (int)scriptList.size())return Editor::message(MSG_OBJECT_COMPONENT_CODE_SELECT);
    return scriptList[idx].name.c_str();
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    auto &assets = ctx.project->getAssets();
    auto &scriptList = assets.getTypeEntries(FileType::CODE_OBJ);

    if (ImTable::start("Comp", &obj)) {
      ImTable::add(Editor::message(MSG_OBJECT_COMPONENT_CODE_NAME), entry.name);
      int idx = ImTable::addVecComboBox(Editor::message(MSG_OBJECT_COMPONENT_CODE_SCRIPT), scriptList, data.scriptUUID);
      //ImGui::InputScalar("##UUID", ImGuiDataType_U64, &data.scriptUUID);

      if (idx < (int)scriptList.size()) {
        const auto &script = scriptList[idx];
        data.scriptUUID = script.getUUID();

        ImTable::add(Editor::message(MSG_OBJECT_COMPONENT_CODE_ARGS));
        if (script.params.fields.empty()) {
          ImGui::Text(Editor::message(MSG_OBJECT_COMPONENT_CODE_ARG_NONE));
        }

        for (auto &field : script.params.fields)
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
            ImTable::addVecComboBox(name, assets, uuid);
            data.args[field.name].value = std::to_string(uuid);
          } else {
            ImTable::addObjProp(name, data.args[field.name]);
          }
        }
      }

      ImTable::end();
    }
  }
}
