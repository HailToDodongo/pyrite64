/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "matInstanceEditor.h"

#include "textureEditor.h"
#include "../../../../project/component/shared/materialInstance.h"
#include "../../../../context.h"
#include "../../../imgui/helper.h"

void Editor::MatInstanceEditor::draw(
  Project::Component::Shared::MaterialInstance &matInst,
  Project::Object &obj,
  uint64_t modelUUID
) {

  auto t3dm = ctx.project->getAssets().getEntryByUUID(modelUUID);
  if(!t3dm || t3dm->model.materials.empty())return;

  if(!ImGui::CollapsingSubHeader("Material Instance", ImGuiTreeNodeFlags_DefaultOpen)
    || !ImTable::start("Mat", &obj)
  ) {
    return;
  }

  auto &firstMat = t3dm->model.materials.begin()->second;
  matInst.validateWithModel(t3dm->model);

  for(uint32_t slotIdx = 0; slotIdx<matInst.texSlots.size(); ++slotIdx)
  {
    auto &slot = matInst.texSlots[slotIdx];
    if(!slot.set.value)continue;
//    assert(slot.dynType.value != 0);

    ImGui::PushID(slotIdx);
    ImTable::add("Placeholder");
    ImGui::Text("Slot #%u", slotIdx);

    if(slot.dynType.value == Project::Assets::MaterialTex::DYN_TYPE_TILE)
    {
      ImTable::add("Offset");
      ImGui::DragFloat2("##Offset", &slot.offset.value.x, 0.25f, 0.0f, 1023.75f, "%.2f");

      slot.offset.value = glm::clamp(slot.offset.value, 0.0f, 1023.75f);
    } else if(slot.dynType.value == Project::Assets::MaterialTex::DYN_TYPE_FULL) {
      TextureEditor::draw(matInst.texSlots[slotIdx]);
    }
    ImGui::PopID();

    ImGui::Dummy(ImVec2(0, 6_px));
  }

  if(!firstMat.zmodeSet.value) {
    ImTable::addObjProp<int32_t>("Depth", matInst.depth, [](int32_t *depth)
    {
      std::array<const char*, 4> items = {"None", "Read", "Write", "Read+Write"};
      return ImGui::Combo("##", depth, items.data(), items.size());
    }, &matInst.setDepth);
  } else matInst.setDepth.value = false;

  if(!firstMat.primColorSet.value) {
    ImTable::addObjProp("Prim-Color", matInst.prim, &matInst.setPrim);
  } else matInst.setPrim.value = false;

  if(!firstMat.envColorSet.value) {
    ImTable::addObjProp("Env-Color", matInst.env, &matInst.setEnv);
  } else matInst.setEnv.value = false;

  ImTable::addObjProp("Fresnel", matInst.fresnel, &matInst.setFresnel);
  if(matInst.fresnel.resolve(obj.propOverrides) != 0)
  {
    ImTable::addObjProp("Fres-Color", matInst.fresnelColor);
  }
  // ImTable::addObjProp("Lighting", data.material.lighting, &data.material.setLighting);

  ImTable::end();
}
