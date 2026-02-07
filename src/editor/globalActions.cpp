/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "actions.h"
#include "../project/project.h"
#include "../utils/logger.h"
#include "../context.h"
#include "../build/projectBuilder.h"
#include "../utils/fs.h"
#include "../utils/json.h"
#include "../utils/proc.h"

namespace Editor::Actions
{
  void initGlobalActions()
  {
    registerAction(Type::PROJECT_OPEN, [](const std::string &path) {
       Utils::Logger::log("Open Project: " + path);
       delete ctx.project;
       try {
         ctx.project = new Project::Project(path);
         if(ctx.project && !ctx.project->getScenes().getEntries().empty()) {
           ctx.project->getScenes().loadScene(ctx.project->conf.sceneIdOnBoot);
         }
       } catch (const std::exception &e) {
         Utils::Logger::log("Failed to open project: " + std::string(e.what()), Utils::Logger::LEVEL_ERROR);
         ctx.project = nullptr;
         return false;
       }
       return true;
     });

    registerAction(Type::PROJECT_CLOSE, [](const std::string&) {
      delete ctx.project;
      ctx.project = nullptr;
      return true;
    });

    registerAction(Type::PROJECT_CLEAN, [](const std::string& arg) {
      if (ctx.isBuildOrRunning())return false;
      if (!ctx.project)return false;
      Utils::Logger::log("Clean Project");

      std::string runCmd = "make -C \"" + ctx.project->getPath() + "\" clean";

      ctx.futureBuildRun = std::async(std::launch::async, [] (std::string runCmd)
      {
        Utils::Proc::runSyncLogged(runCmd);
      }, runCmd);

      return true;
    });

    registerAction(Type::PROJECT_CREATE, [](const std::string &payload)
    {
      if(ctx.project)return false;
      nlohmann::json args{};
      try {
        args = nlohmann::json::parse(payload);
      } catch (const std::exception &e) {
        Utils::Logger::log("Failed to parse PROJECT_CREATE args: " + std::string(e.what()), Utils::Logger::LEVEL_ERROR);
        return false;
      }

      Utils::Logger::log("Create Project: " + args.value<std::string>("path", ""));

      // copy example project as template
      Utils::FS::copyDir("n64/examples/empty", args["path"]);

      // clear some temp files
      std::filesystem::path newPath{args["path"]};
      std::filesystem::remove(newPath / "p64_project.z64");
      std::filesystem::remove_all(newPath / "build");
      std::filesystem::remove_all(newPath / "filesystem");

      // open project.json and patch name
      auto configPath = (newPath / "project.json").string();
      auto configJSON = Utils::JSON::loadFile(newPath / "project.json");
      configJSON["name"] = args["name"];
      configJSON["romName"] = args["rom"];
      Utils::FS::saveTextFile(configPath, configJSON.dump(2));

      return true;
    });

    registerAction(Type::PROJECT_BUILD, [](const std::string& arg) {
      if (ctx.isBuildOrRunning())return false;
      if (!ctx.project)return false;

      ImGui::SetWindowFocus("Log");

      auto z64Path = ctx.project->getPath() + "/" + ctx.project->conf.romName + ".z64";
      Utils::FS::delFile(z64Path);

      std::string runCmd{};
      if (arg == "run") {
        runCmd = ctx.project->conf.pathEmu + " " + z64Path;
      }

      ctx.futureBuildRun = std::async(std::launch::async, [] (std::string path, std::string runCmd)
      {
        if(!Build::buildProject(path)) {
          // @TODO: error popup
          return;
        }

        if (!runCmd.empty()) {
          Utils::Proc::runSyncLogged(runCmd);
        }
      }, ctx.project->getPath(), runCmd);

      return true;
    });

    registerAction(Type::ASSETS_RELOAD, [](const std::string&) {
      if(ctx.project) {
        ctx.project->getAssets().reload();
      }
      return true;
    });

    registerAction(Type::COPY, [](const std::string&) {
      if(!ctx.project)return false;
      auto scene = ctx.project->getScenes().getLoadedScene();
      if(!scene)return false;

      auto obj = scene->getObjectByUUID(ctx.selObjectUUID);
      if(!obj)return false;

      ctx.clipboard.data = obj->serialize().dump();
      ctx.clipboard.refUUID = obj->parent ? obj->parent->uuid : 0;

      return true;
    });

    registerAction(Type::PASTE, [](const std::string&) {
      if(!ctx.project || ctx.clipboard.data.empty())return false;
      auto scene = ctx.project->getScenes().getLoadedScene();
      if(!scene)return false;

      auto obj = scene->addObject(ctx.clipboard.data, ctx.clipboard.refUUID);
      ctx.selObjectUUID = obj->uuid;
      return true;
    });
  }
}
