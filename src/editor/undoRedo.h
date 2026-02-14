/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Project
{
  class Scene;
}

namespace Editor::UndoRedo
{
  /**
   * Base class for all undoable commands.
   * Commands capture state changes and provide undo/redo functionality.
   */
  class Command
  {
    public:
      virtual ~Command() = default;
      
      /**
       * Execute the command (redo).
       */
      virtual void execute() = 0;
      
      /**
       * Undo the command.
       */
      virtual void undo() = 0;
      
      /**
       * Get a human-readable description of the command.
       */
      virtual std::string getDescription() const = 0;

      virtual uint64_t getMemoryUsage() const = 0;
  };

  /**
   * Manages undo/redo history.
   */
  class History
  {
    private:
      std::vector<std::unique_ptr<Command>> undoStack;
      std::vector<std::unique_ptr<Command>> redoStack;
      size_t maxHistorySize{100};
      int snapshotDepth{0};
      std::string snapshotBefore;
      std::string snapshotDescription;
      Project::Scene* snapshotScene{nullptr};
      uint32_t snapshotSelUUID{0};
      std::vector<uint32_t> snapshotSelUUIDs{};
      
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

      /**
       * Begin a scene snapshot (for grouping edits).
       */
      bool beginSnapshot(const std::string& description);

      /**
       * Begin a scene snapshot using a pre-captured state.
       */
      bool beginSnapshotFromState(const std::string& before, const std::string& description);

      /**
       * End a scene snapshot and push to history if changed.
       */
      bool endSnapshot();

      /**
       * Capture the current scene state for snapshotting.
       */
      std::string captureSnapshotState();

      /**
       * Check if a snapshot is active.
       */
      [[nodiscard]] bool isSnapshotActive() const { return snapshotDepth > 0; }
      
      /**
       * Check if undo is available.
       */
      [[nodiscard]] bool canUndo() const { return !undoStack.empty(); }
      
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

  class SnapshotScope
  {
    private:
      History* history{nullptr};
      bool active{false};

    public:
      SnapshotScope(History& targetHistory, const std::string& description);
      ~SnapshotScope();
  };
  
  /**
   * Get the global history instance.
   */
  History& getHistory();
}
