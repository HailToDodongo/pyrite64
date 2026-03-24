/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>

namespace Project {
  class Object;
}

namespace Project::Component::Shared {
  struct MaterialInstance;
}

namespace Editor::MatInstanceEditor
{
  void draw(
    Project::Component::Shared::MaterialInstance &matInst,
    Project::Object &obj,
    uint64_t modelUUID
  );
}
