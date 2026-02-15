/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "undoRedo.h"
#include "../context.h"
#include "imgui.h"

namespace
{
  Editor::UndoRedo::History globalHistory;
}

namespace Editor::UndoRedo
{
  bool History::undo()
  {
    if (!canUndo()) return false;

    auto cmd = std::move(undoStack.back());
    undoStack.pop_back();
    const auto &prevCmd = undoStack.back();

    if (!snapshotScene) return false;
    snapshotScene->deserialize(prevCmd->state);

    uint32_t primarySel = prevCmd->selection.empty() ? 0 : prevCmd->selection.back();
    ctx.setObjectSelectionList(prevCmd->selection, primarySel);
    ctx.sanitizeObjectSelection(snapshotScene);

    redoStack.push_back(std::move(cmd));
    
    return true;
  }
  
  bool History::redo()
  {
    if (!canRedo()) return false;

    auto cmd = std::move(redoStack.back());
    redoStack.pop_back();

    if (!snapshotScene) return false;
    snapshotScene->deserialize(cmd->state);

    uint32_t primarySel = cmd->selection.empty() ? 0 : cmd->selection.back();
    ctx.setObjectSelectionList(cmd->selection, primarySel);
    ctx.sanitizeObjectSelection(snapshotScene);

    undoStack.push_back(std::move(cmd));

    return true;
  }
  
  void History::clear()
  {
    undoStack.clear();
    redoStack.clear();
    nextChangedReason.clear();
    snapshotScene = nullptr;
    snapshotSelUUID = 0;
    snapshotSelUUIDs.clear();
  }

  void History::begin() {
    auto scene = ctx.project ? ctx.project->getScenes().getLoadedScene() : nullptr;
    if (!scene)return;

    if (undoStack.empty()) {
      // If this is the first change, we need to save the initial state of the scene
      std::string initialState = scene->serialize(true);
      std::vector<uint32_t> ids{};
      undoStack.push_back(std::make_unique<Entry>(
        std::move(initialState), "Initial State", ids
      ));
    }

    snapshotScene = scene;
    snapshotSelUUID = ctx.selObjectUUID;
    snapshotSelUUIDs = ctx.selObjectUUIDs;
  }

  void History::end() {
    if (nextChangedReason.empty())return;

    auto scene = snapshotScene;
    snapshotScene = nullptr;
    if (!scene) {
      nextChangedReason.clear();
      return;
    }

    redoStack.clear();
    auto ids = ctx.selObjectUUIDs;

    auto newEntry = std::make_unique<Entry>(
      scene->serialize(true),
      nextChangedReason,
      ids
    );

    nextChangedReason.clear();

    if (!undoStack.empty()) {
      // check against last state to avoid pushing duplicate states
      if (undoStack.back()->state == newEntry->state) {
        return;
      }
    }

    undoStack.push_back(std::move(newEntry));

    if (undoStack.size() > maxHistorySize) {
      undoStack.erase(undoStack.begin(), undoStack.end() - maxHistorySize);
    }
  }

  std::string History::getUndoDescription() const
  {
    if (undoStack.empty()) return "";
    return undoStack.back()->description;
  }
  
  std::string History::getRedoDescription() const
  {
    if (redoStack.empty()) return "";
    return redoStack.back()->description;
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
  
  uint64_t History::getMemoryUsage()
  {
    uint64_t total = 0;
    for(auto &entry : redoStack) {
      total += entry->getMemoryUsage();
    }
    for(auto &entry : undoStack) {
      total += entry->getMemoryUsage();
    }
    return total;
  }

  History& getHistory()
  {
    return globalHistory;
  }
}
