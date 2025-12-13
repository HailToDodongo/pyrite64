/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "projectBuilder.h"
#include "../utils/string.h"
#include <filesystem>
#include <format>

#include "../utils/fs.h"
#include "../utils/logger.h"

#include "../../../n64/engine/include/script/globalScript.h"

namespace fs = std::filesystem;

void Build::buildScripts(Project::Project &project, SceneCtx &sceneCtx)
{
  auto pathTable = project.getPath() + "/src/p64/scriptTable.cpp";

  std::string srcEntries = "";
  std::string srcSizeEntries = "";
  std::string srcDecl = "";

  auto scripts = project.getAssets().getTypeEntries(Project::AssetManager::FileType::CODE_OBJ);
  uint32_t idx = 0;
  for (auto &script : scripts)
  {
    auto src = Utils::FS::loadTextFile(script.path);
    bool hasInit = Utils::CPP::hasFunction(src, "void", "init");
    bool hasUpdate = Utils::CPP::hasFunction(src, "void", "update");
    bool hasDraw = Utils::CPP::hasFunction(src, "void", "draw");
    bool hasDestroy = Utils::CPP::hasFunction(src, "void", "destroy");
    bool hasEvent = Utils::CPP::hasFunction(src, "void", "onEvent");

    auto uuidStr = std::format("{:016X}", script.uuid);

    srcSizeEntries += uuidStr + "::DATA_SIZE,\n";

    srcDecl += "  namespace " + uuidStr + " {\nstruct Data;\n";
    srcDecl += " extern uint16_t DATA_SIZE;\n";
    if(hasInit)srcDecl += "void init(Object& obj, Data *data);\n";
    if(hasUpdate)srcDecl += "void update(Object& obj, Data *data, float deltaTime);\n";
    if(hasDraw)srcDecl += "void draw(Object& obj, Data *data, float deltaTime);\n";
    if(hasDestroy)srcDecl += "void destroy(Object& obj, Data *data);\n";
    if(hasEvent)srcDecl += "void onEvent(Object& obj, Data *data, const ObjectEvent& event);\n";
    srcDecl += "}\n";

    srcEntries += "{\n";
    if(hasInit)srcEntries += " .init = (FuncObjData)" + uuidStr + "::init,\n";
    if(hasUpdate)srcEntries += " .update = (FuncObjDataDelta)" + uuidStr + "::update,\n";
    if(hasDraw)srcEntries += " .draw = (FuncObjDataDelta)" + uuidStr + "::draw,\n";
    if(hasDestroy)srcEntries += " .destroy = (FuncObjData)" + uuidStr + "::destroy,\n";
    if(hasEvent)srcEntries += " .onEvent = (FuncObjDataEvent)" + uuidStr + "::onEvent,\n";
    srcEntries += "},\n";

    sceneCtx.codeIdxMapUUID[script.uuid] = idx;

    Utils::Logger::log("Script: " + uuidStr + " -> " + std::to_string(idx));
    ++idx;
  }

  auto src = Utils::FS::loadTextFile("data/scripts/scriptTable.cpp");
  src = Utils::replaceAll(src, "__CODE_ENTRIES__", srcEntries);
  src = Utils::replaceAll(src, "__CODE_SIZE_ENTRIES__", srcSizeEntries);
  src = Utils::replaceAll(src, "__CODE_DECL__", srcDecl);


  Utils::FS::saveTextFile(pathTable, src);
}

void Build::buildGlobalScripts(Project::Project &project, SceneCtx &sceneCtx)
{
  auto pathTable = project.getPath() + "/src/p64/globalScriptTable.cpp";
  auto scripts = project.getAssets().getTypeEntries(Project::AssetManager::FileType::CODE_GLOBAL);

  std::string srcDecl = "";

  std::unordered_map<std::string, std::string> enumMap{};
  enumMap["onGameInit"]        = "GAME_INIT";
  enumMap["onScenePreLoad"]    = "SCENE_PRE_LOAD";
  enumMap["onScenePostLoad"]   = "SCENE_POST_LOAD";
  enumMap["onScenePreUnload"]  = "SCENE_PRE_UNLOAD";
  enumMap["onScenePostUnload"] = "SCENE_POST_UNLOAD";
  enumMap["onSceneUpdate"]     = "SCENE_UPDATE";
  enumMap["onScenePreDraw3D"]  = "SCENE_PRE_DRAW_3D";
  enumMap["onScenePostDraw3D"] = "SCENE_POST_DRAW_3D";
  enumMap["onScenePreDraw2D"]  = "SCENE_PRE_DRAW_2D";
  enumMap["onScenePostDraw2D"] = "SCENE_POST_DRAW_2D";

  std::unordered_map<std::string, std::string> nameMap{};
  for (auto &e : enumMap) {
    nameMap[e.first] = "";
  }

  for (auto &script : scripts)
  {
    printf("s: %s\n", script.path.c_str());
    auto src = Utils::FS::loadTextFile(script.path);

    for(auto &pair : nameMap)
    {
      auto funcName = pair.first;

      bool hasFunc = Utils::CPP::hasFunction(src, "void", funcName);
      if (!hasFunc)continue;

      auto uuidStr = std::format("{:016X}", script.uuid);

      srcDecl += "  namespace " + uuidStr + " {\n";
      srcDecl += " void " + funcName + "();\n";
      srcDecl += "}\n";

      pair.second += "  " + uuidStr + "::" + funcName + "();\n";

      //sceneCtx.globalFuncIdxMapUUID[script.uuid][hookIdx] = funcName;

      Utils::Logger::log("Global Script: " + uuidStr + " -> " + funcName);
    }
  }

  std::string srcHook = "";
  for (auto &pair : nameMap)
  {
    if (pair.second.empty())continue;

    srcHook += "case HookType::" + enumMap[pair.first] + ":\n";
    srcHook += pair.second;
    srcHook += " break;\n";
  }

  auto src = Utils::FS::loadTextFile("data/scripts/globalScriptTable.cpp");
  src = Utils::replaceAll(src, "__CODE_DECL__", srcDecl);
  src = Utils::replaceAll(src, "__CODE_HOOKS__", srcHook);
  Utils::FS::saveTextFile(pathTable, src);
}