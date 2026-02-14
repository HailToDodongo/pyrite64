/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once

#include <memory>
#include <string>
#include <vector>

namespace Project {
  class Scene;
  class Object;
}

namespace Editor::UndoRedo {
  class History;
}

namespace Editor::SelectionUtils
{
  std::vector<Project::Object*> collectSelectedObjects(Project::Scene &scene);
  std::vector<std::shared_ptr<Project::Object>> collectSelectedObjectRefs(Project::Scene &scene);

  bool deleteSelectedObjects(
    Project::Scene &scene,
    Editor::UndoRedo::History &history,
    const std::string &description,
    bool endActiveSnapshot = true
  );
}
