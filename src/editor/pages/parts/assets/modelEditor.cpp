/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "modelEditor.h"
#include "../../../../context.h"

bool Editor::ModelEditor::draw(ImGuiID defDockId)
{
  auto model = ctx.project->getAssets().getEntryByUUID(assetUUID);
  if(!model)return false;

  ImGui::SetNextWindowSize({800,600}, ImGuiCond_Once);
  ImGui::Begin("Model", nullptr, ImGuiWindowFlags_NoCollapse);
  ImGui::Text("Model: %s", model->name.c_str());

  for(auto &entry : model->model.t3dm.materials)
  {
    auto label = "Material: " + entry.first;
    if (ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
      ImGui::Text("This is a fast64 material");
    }

  }


  ImGui::End();
  return true;
}
