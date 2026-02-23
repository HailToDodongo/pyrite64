/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Project
{
  class Scene;
}

namespace Editor::UndoRedo
{
  struct Entry
  {
    std::string state{};
    std::string description{};
    std::vector<uint32_t> selection{};

    uint64_t getMemoryUsage() const {
      return state.capacity() + description.capacity()
      + sizeof(Entry) + selection.capacity() * sizeof(uint32_t);
    }
  };

  /**
   * Manages undo/redo history.
   */
  class History
  {
    private:
      std::vector<std::unique_ptr<Entry>> undoStack;
      std::vector<std::unique_ptr<Entry>> redoStack;
      size_t maxHistorySize{100};
      Project::Scene* snapshotScene{nullptr};
      std::vector<uint32_t> snapshotSelUUIDs{};
      std::string nextChangedReason{};
      std::optional<std::string> savedState{};
      
    public:
      /**
       * Undo the last command.
       * @return true if undo was performed
       */
      bool undo();
      
      /**
       * Redo the last undone command.
       * @return true if redo was performed
       */
      bool redo();
      
      /**
       * Clear all history.
       */
      void clear();

      void markChanged(std::string reason) {
        nextChangedReason = std::move(reason);
      }

      void markSaved();
      [[nodiscard]] bool isDirty() const;

      uint32_t getUndoCount() const { return (uint32_t)undoStack.size(); }
      uint32_t getRedoCount() const { return (uint32_t)redoStack.size(); }

      void begin();
      void end();

      /**
       * Check if undo is available.
       */
      [[nodiscard]] bool canUndo() const { return undoStack.size() > 1; }
      
      /**
       * Check if redo is available.
       */
      [[nodiscard]] bool canRedo() const { return !redoStack.empty(); }
      
      /**
       * Get description of the command that would be undone.
       */
      [[nodiscard]] std::string getUndoDescription() const;
      
      /**
       * Get description of the command that would be redone.
       */
      [[nodiscard]] std::string getRedoDescription() const;
      
      /**
       * Set maximum history size.
       */
      void setMaxHistorySize(size_t size);

      /**
       * Returns currently used memory in bytes.
       */
      uint64_t getMemoryUsage();
  };

  /**
   * Get the global history instance.
   */
  History& getHistory();
}
