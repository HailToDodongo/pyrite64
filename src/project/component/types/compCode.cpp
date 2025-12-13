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

namespace Project::Component::Code
{
  struct Data
  {
    uint64_t scriptUUID{0};
    std::unordered_map<std::string, std::string> args{};
  };

  std::shared_ptr<void> init(Object &obj) {
    auto data = std::make_shared<Data>();
    return data;
  }

  std::string serialize(Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    Utils::JSON::Builder builder{};
    builder.set("script", data.scriptUUID);

    Utils::JSON::Builder builderArgs{};
    for (auto &arg : data.args) {
      builderArgs.set(arg.first, arg.second);
    }
    builder.set("args", builderArgs);

    return builder.toString();
  }

  std::shared_ptr<void> deserialize(simdjson::simdjson_result<simdjson::dom::object> &doc) {
    auto data = std::make_shared<Data>();
    data->scriptUUID = doc["script"].get<uint64_t>();
    if (!doc["args"].error()) {
      auto argsObj = doc["args"].get_object();
      for (auto field : argsObj) {
        data->args[std::string{field.key}] = field.value.get_string().value();
      }
    }
    return data;
  }

  void build(Object&, Entry &entry, Build::SceneCtx &ctx)
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
      auto val = data.args[field.name];
      if (val.empty())val = field.defaultValue;
      if (val.empty())val = "0";
      ctx.fileObj.writeAs(val, field.type);
    }
  }

  const char* getter(void* user_data, int idx)
  {
    auto &scriptList = ctx.project->getAssets().getTypeEntries(AssetManager::FileType::CODE_OBJ);
    if (idx < 0 || idx >= scriptList.size())return "<Select Script>";
    return scriptList[idx].name.c_str();
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    auto &assets = ctx.project->getAssets();
    auto &scriptList = assets.getTypeEntries(AssetManager::FileType::CODE_OBJ);

    if (ImGui::InpTable::start("Comp")) {
      ImGui::InpTable::addString("Name", entry.name);
      ImGui::InpTable::add("Script");
      //ImGui::InputScalar("##UUID", ImGuiDataType_U64, &data.scriptUUID);

      int idx = scriptList.size();
      for (int i=0; i<scriptList.size(); ++i) {
        if (scriptList[i].uuid == data.scriptUUID) {
          idx = i;
          break;
        }
      }

      ImGui::Combo("##UUID", &idx, getter, nullptr, scriptList.size()+1);

      if (idx < scriptList.size()) {
        const auto &script = scriptList[idx];
        data.scriptUUID = script.uuid;

        ImGui::InpTable::add("Arguments:");
        if (script.params.fields.empty()) {
          ImGui::Text("(None)");
        }

        int idx = 0;
        for (auto &field : script.params.fields)
        {
          std::string name = field.name;
          auto metaName = field.attr.find("P64::Name");
          if (metaName != field.attr.end()) {
            name = metaName->second;
          }

          ImGui::InpTable::addString(name, data.args[field.name]);
          ++idx;
        }
      }

      ImGui::InpTable::end();
    }
  }
}
