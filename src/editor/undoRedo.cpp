/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "undoRedo.h"
#include "../context.h"
#include "../utils/logger.h"

namespace
{
  class SceneSnapshotCommand : public Editor::UndoRedo::Command
  {
    private:
      Project::Scene* scene;
      std::string beforeState; // Scene state before the change
      std::string afterState; // Scene state after the change
      std::string description; // Description of the change for undo/redo menu
      uint32_t selBefore; // Primary selected object before the change
      uint32_t selAfter; // Primary selected object after the change
      std::vector<uint32_t> selUUIDsBefore; // All selected object UUIDs before the change
      std::vector<uint32_t> selUUIDsAfter; // All selected object UUIDs after the change

    public:
      SceneSnapshotCommand(Project::Scene* targetScene, std::string before, std::string after, std::string desc,
                           uint32_t selBefore, uint32_t selAfter,
                           std::vector<uint32_t> selUUIDsBefore, std::vector<uint32_t> selUUIDsAfter)
        : scene(targetScene),
          beforeState(std::move(before)),
          afterState(std::move(after)),
          description(std::move(desc)),
          selBefore(selBefore),
          selAfter(selAfter),
          selUUIDsBefore(std::move(selUUIDsBefore)),
          selUUIDsAfter(std::move(selUUIDsAfter))
      {}

      void execute() override
      {
        if (!scene) return;
        scene->deserialize(afterState);
        ctx.selObjectUUIDs = selUUIDsAfter;
        ctx.selObjectUUID = selAfter;
        ctx.sanitizeObjectSelection(scene);
      }

      void undo() override
      {
        if (!scene) return;
        scene->deserialize(beforeState);
        ctx.selObjectUUIDs = selUUIDsBefore;
        ctx.selObjectUUID = selBefore;
        ctx.sanitizeObjectSelection(scene);
      }

      std::string getDescription() const override
      {
        return description;
      }
  };
}

namespace
{
  Editor::UndoRedo::History globalHistory;
}

namespace Editor::UndoRedo
{
  bool History::undo()
  {
    if (undoStack.empty()) return false;
    
    auto cmd = std::move(undoStack.back());
    undoStack.pop_back();
    
    cmd->undo();
    Utils::Logger::log("Undo: " + cmd->getDescription(), Utils::Logger::LEVEL_INFO);
    redoStack.push_back(std::move(cmd));
    
    return true;
  }
  
  bool History::redo()
  {
    if (redoStack.empty()) return false;
    
    auto cmd = std::move(redoStack.back());
    redoStack.pop_back();
    
    cmd->execute();
    Utils::Logger::log("Redo: " + cmd->getDescription(), Utils::Logger::LEVEL_INFO);
    undoStack.push_back(std::move(cmd));
    
    return true;
  }
  
  void History::clear()
  {
    undoStack.clear();
    redoStack.clear();
    snapshotDepth = 0;
    snapshotBefore.clear();
    snapshotDescription.clear();
    snapshotScene = nullptr;
    snapshotSelUUID = 0;
    snapshotSelUUIDs.clear();
  }

  bool History::beginSnapshot(const std::string& description)
  {
    auto scene = ctx.project ? ctx.project->getScenes().getLoadedScene() : nullptr;
    if (!scene) {
      return false;
    }

    if (snapshotDepth == 0) {
      snapshotBefore = scene->serialize();
      snapshotDescription = description;
      snapshotScene = scene;
      snapshotSelUUID = ctx.selObjectUUID;
      snapshotSelUUIDs = ctx.selObjectUUIDs;
    }

    ++snapshotDepth;
    return true;
  }

  bool History::beginSnapshotFromState(const std::string& before, const std::string& description)
  {
    auto scene = ctx.project ? ctx.project->getScenes().getLoadedScene() : nullptr;
    if (!scene) {
      return false;
    }

    if (snapshotDepth == 0) {
      snapshotBefore = before;
      snapshotDescription = description;
      snapshotScene = scene;
      snapshotSelUUID = ctx.selObjectUUID;
      snapshotSelUUIDs = ctx.selObjectUUIDs;
    }

    ++snapshotDepth;
    return true;
  }

  bool History::endSnapshot()
  {
    if (snapshotDepth <= 0) {
      snapshotDepth = 0;
      return false;
    }

    --snapshotDepth;
    if (snapshotDepth > 0) {
      return false;
    }

    auto scene = snapshotScene;
    snapshotScene = nullptr;
    if (!scene) {
      snapshotBefore.clear();
      snapshotDescription.clear();
      return false;
    }

    std::string after = scene->serialize();
    std::string before = std::move(snapshotBefore);
    std::string description = std::move(snapshotDescription);

    snapshotBefore.clear();
    snapshotDescription.clear();

    if (before == after) {
      return false;
    }

    redoStack.clear();
    undoStack.push_back(std::make_unique<SceneSnapshotCommand>(
      scene,
      std::move(before),
      std::move(after),
      std::move(description),
      snapshotSelUUID,
      ctx.selObjectUUID,
      std::move(snapshotSelUUIDs),
      ctx.selObjectUUIDs
    ));

    if (undoStack.size() > maxHistorySize) {
      undoStack.erase(undoStack.begin(), undoStack.end() - maxHistorySize);
    }
    return true;
  }

  std::string History::captureSnapshotState()
  {
    auto scene = ctx.project ? ctx.project->getScenes().getLoadedScene() : nullptr;
    if (!scene) {
      return {};
    }
    return scene->serialize();
  }
  
  std::string History::getUndoDescription() const
  {
    if (undoStack.empty()) return "";
    return undoStack.back()->getDescription();
  }
  
  std::string History::getRedoDescription() const
  {
    if (redoStack.empty()) return "";
    return redoStack.back()->getDescription();
  }

  void History::setMaxHistorySize(size_t size)
  {
    maxHistorySize = size;
    if (maxHistorySize == 0) {
      undoStack.clear();
      redoStack.clear();
      return;
    }

    if (undoStack.size() > maxHistorySize) {
      undoStack.erase(undoStack.begin(), undoStack.end() - maxHistorySize);
    }

    if (redoStack.size() > maxHistorySize) {
      redoStack.erase(redoStack.begin(), redoStack.end() - maxHistorySize);
    }
  }
  
  History& getHistory()
  {
    return globalHistory;
  }

  SnapshotScope::SnapshotScope(History& targetHistory, const std::string& description)
    : history(&targetHistory)
  {
    active = history->beginSnapshot(description);
  }

  SnapshotScope::~SnapshotScope()
  {
    if (history && active) {
      history->endSnapshot();
    }
  }
}
