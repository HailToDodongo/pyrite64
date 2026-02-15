/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once

#include <vector>

namespace Project {
  class Scene;
  class Object;
}

namespace Editor::SelectionUtils
{
  std::vector<Project::Object*> collectSelectedObjects(Project::Scene &scene);

  bool deleteSelectedObjects(Project::Scene &scene);
}
