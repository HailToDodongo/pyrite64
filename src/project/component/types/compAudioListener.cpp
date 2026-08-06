/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "../components.h"
#include "../../../context.h"
#include "../../../editor/imgui/helper.h"
#include "../../../utils/json.h"
#include "../../../utils/colors.h"
#include "../../../editor/pages/parts/viewport3D.h"
#include "../../../renderer/scene.h"
#include "../../../utils/meshGen.h"

namespace Project::Component::AudioListener
{
  struct Data {};

  std::shared_ptr<void> init(Object &obj) {
    return std::make_shared<Data>();
  }

  nlohmann::json serialize(const Entry &entry) {
    return nlohmann::json::object();
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    return std::make_shared<Data>();
  }

  void build(Object&, Entry &entry, Build::SceneCtx &ctx) {}

  void draw(Object &obj, Entry &entry)
  {
    if (ImTable::start("Comp", &obj)) {
      ImTable::add("Name", entry.name);
      ImTable::end();
    }
    ImGui::TextDisabled("Drives the 3D audio listener from this object's\nposition and rotation (-Z forward, Y-up).");
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    glm::u8vec4 col{0xFF};
    if (ctx.isObjectSelected(obj.uuid)) {
      col = Utils::Colors::kSelectionTint;
    }
    Utils::Mesh::addSprite(*vp.getSprites(), obj.pos.resolve(obj.propOverrides), obj.uuid, 4, col);
  }
}
