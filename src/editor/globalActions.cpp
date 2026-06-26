/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "actions.h"

#include <cstdlib>
#include <unordered_set>
#include "../project/project.h"
#include "../editor/imgui/notification.h"
#include "../utils/logger.h"
#include "../context.h"
#include "../build/projectBuilder.h"
#include "../project/graph/nodeRegistry.h"
#include "../utils/fs.h"
#include "../utils/json.h"
#include "../utils/proc.h"
#include "undoRedo.h"
#include "pages/editorScene.h"
//#include <stacktrace>

namespace
{
  std::string getProcessPath()
  {
    const char *path = std::getenv("PATH");
    return path ? std::string{path} : std::string{};
  }

  void restoreProcessPath(const std::string &path)
  {
    #if defined(_WIN32)
      _putenv_s("PATH", path.c_str());
    #else
      setenv("PATH", path.c_str(), 1);
    #endif
  }
}

namespace Editor::Actions
{
  void initGlobalActions()
  {
    registerAction(Type::PROJECT_OPEN, [](const std::string &path) {
       Utils::Logger::log("Open Project: " + path);
       // Remember the outgoing project's open windows before it is torn down.
       if(ctx.editorScene && ctx.project) ctx.editorScene->onProjectClosing();
       delete ctx.project;
       UndoRedo::getHistory().clear();
       try {
         ctx.project = new Project::Project(path);
         // Custom node definitions (<project>/nodes/*.js) are loaded by the Project ctor.
         if(ctx.project && !ctx.project->getScenes().getEntries().empty()) {
           ctx.project->getScenes().loadScene(ctx.project->conf.sceneIdLastOpened);
         }
         if(ctx.project && ctx.project->wasSavedWithNewerVersion()) {
           Editor::Noti::add(Editor::Noti::Type::ERROR,
             "This project was saved with a newer editor version (" + ctx.project->conf.editorVersion +
             ", current is v" PYRITE_VERSION ").\nIt was opened anyway, but things may break.");
         }
         // Remember this project in the launcher's recent list (normalized absolute path).
         if(ctx.project) {
           // Resolve the metadata cart/box art image to a file path for the launcher card.
           std::string cardImage{};
           const auto &meta = ctx.project->conf.metadata;
           if(meta.enabled && !meta.langs.empty()) {
             const auto &lang = meta.langs.front();
             uint64_t imgUUID = lang.cartFront ? lang.cartFront : lang.boxFront;
             if(imgUUID) {
               if(auto *e = ctx.project->getAssets().getEntryByUUID(imgUUID)) {
                 cardImage = fs::absolute(e->path).string();
               }
             }
           }
           ctx.prefs.addRecentProject(fs::absolute(path).string(), ctx.project->conf.name, cardImage);
           ctx.prefs.save();
         }
       } catch (const std::exception &e) {
         auto error = "Failed to open project:\n" + std::string(e.what());
         //error += "\n" + std::to_string(std::stacktrace::current());
         Utils::Logger::log(error, Utils::Logger::LEVEL_ERROR);
         Editor::Noti::add(Editor::Noti::Type::ERROR, error);
         ctx.project = nullptr;
         return false;
       }
       return ctx.project != nullptr;
     });

    registerAction(Type::PROJECT_CLOSE, [](const std::string&) {
      ctx.wantsProjectClose = true;
      return true;
    });

    registerAction(Type::PROJECT_CLEAN, [](const std::string& arg) {
      if (ctx.isBuildOrRunning())return false;
      if (!ctx.project)return false;

      return Build::cleanProject(*ctx.project, {
        .code = true,
        .assets = true,
        .engine = true,
        .engineSrc = true
      });
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

      fs::path newPath{args["path"]};
      Utils::Logger::log("Create Project: " + newPath.string());
      std::filesystem::create_directories(newPath);

      // validate directory is empty
      if (!fs::is_empty(newPath)) {
        Editor::Noti::add(Editor::Noti::Type::ERROR, "Failed to create project, directory is not empty!");
        return false;
      }
      
      // copy example project as template
      fs::copy("n64/examples/empty", newPath, 
        fs::copy_options::recursive | fs::copy_options::overwrite_existing
      );

      // clear some temp files
      fs::remove(newPath / "p64_project.z64");
      fs::remove(newPath / "Makefile");
      fs::remove_all(newPath / "build");
      fs::remove_all(newPath / "filesystem");

      // open project.json and patch name
      auto configPath = (newPath / "project.p64proj").string();
      auto configJSON = Utils::JSON::loadFile(newPath / "project.p64proj");
      configJSON["name"] = args["name"];
      configJSON["romName"] = args["rom"];
      Utils::FS::saveTextFile(configPath, configJSON.dump(2));

      return true;
    });

    registerAction(Type::PROJECT_BUILD, [](const std::string& arg) {
      if (ctx.isBuildOrRunning())return false;
      if (!ctx.project)return false;

      ImGui::SetWindowFocus("Log");

      ctx.project->save();
      ctx.editorScene->save();

      auto z64Path = ctx.project->getPath() + "/" + ctx.project->conf.romName + ".z64";
      fs::remove(z64Path);

      std::string runCmd{};
      if (arg == "run") {
        runCmd = ctx.project->conf.pathEmu + " " + z64Path;
      }

      ctx.futureBuildRun = std::async(std::launch::async, [] (std::string configPath, std::string runCmd)
      {
        auto oldPATH = getProcessPath();
        bool result = false;
        try {
          result = Build::buildProject(configPath);
        } catch (const std::exception &e)
        {
          restoreProcessPath(oldPATH);
          auto error = "Build failed with exception:\n" + std::string(e.what());
          //error += "\n" + std::to_string(std::stacktrace::current());
          Utils::Logger::log(error, Utils::Logger::LEVEL_ERROR);
          Editor::Noti::add(Editor::Noti::Type::ERROR, error);
          return;
        }

        restoreProcessPath(oldPATH);
        
        if(!result) {
          Editor::Noti::add(Editor::Noti::Type::ERROR, "Build failed!");
          return;
        }

        if (!runCmd.empty()) {
          Utils::Proc::runSyncLogged(runCmd);
        }
      }, ctx.project->getConfigPath(), runCmd);

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

      const auto &selected = ctx.getSelectedObjectUUIDs();
      if (selected.empty()) return false;

      std::unordered_set<uint32_t> selectedSet(selected.begin(), selected.end());
      std::vector<std::shared_ptr<Project::Object>> toCopy{};
      toCopy.reserve(selected.size());

      for (auto uuid : selected) {
        auto obj = scene->getObjectByUUID(uuid);
        if(!obj) continue;

        bool parentSelected = false;
        for (auto parent = obj->parent; parent; parent = parent->parent) {
          if (selectedSet.contains(parent->uuid)) {
            parentSelected = true;
            break;
          }
        }
        if (!parentSelected) {
          toCopy.push_back(obj);
        }
      }

      if (toCopy.empty()) return false;

      ctx.clipboard.entries.clear();
      ctx.clipboard.entries.reserve(toCopy.size());
      for (const auto &obj : toCopy) {
        Context::Clipboard::Entry entry{};
        entry.data = obj->serialize().dump();
        entry.refUUID = obj->parent ? obj->parent->uuid : 0;
        ctx.clipboard.entries.push_back(std::move(entry));
      }

      return true;
    });

    registerAction(Type::PASTE, [](const std::string&) {
      if(!ctx.project || ctx.clipboard.entries.empty())return false;
      auto scene = ctx.project->getScenes().getLoadedScene();
      if(!scene)return false;

      UndoRedo::getHistory().markChanged("Paste Object");
      ctx.clearObjectSelection();
      for (const auto &entry : ctx.clipboard.entries) {
        std::string data = entry.data;
        auto obj = scene->addObject(data, entry.refUUID);
        if (obj) {
          ctx.addObjectSelection(obj->uuid);
        }
      }
      return true;
    });
  }
}
