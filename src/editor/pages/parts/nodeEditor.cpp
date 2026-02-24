/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "nodeEditor.h"

#include "imgui.h"
#include "../../../context.h"
#include "../../../utils/logger.h"
#include "../../imgui/helper.h"

#include "ImNodeFlow.h"
#include "json.hpp"
#include "../../../project/graph/nodes/baseNode.h"
#include "../../../utils/fs.h"

namespace
{

}

Editor::NodeEditor::NodeEditor(uint64_t assetUUID)
{
  auto &stylePin = *Project::Graph::Node::PIN_STYLE_LOGIC;
  stylePin = ImFlow::PinStyle{
    IM_COL32(0xAA, 0xAA, 0xAA, 0xFF),
    3, // shape
    6.0f, 7.0f, 6.5f, // radius: base, hover, connected
    1.3f // thickness
  };
  stylePin.extra.padding.y = 16;

  auto &stylePinVal = *Project::Graph::Node::PIN_STYLE_VALUE;
  stylePinVal = ImFlow::PinStyle{
    IM_COL32(0xFF, 0x99, 0x55, 0xFF),
    0, // shape
    6.0f, 7.0f, 6.5f, // radius: base, hover, connected
    1.3f // thickness
  };
  stylePinVal.extra.padding.y = 16;

  currentAsset = ctx.project->getAssets().getEntryByUUID(assetUUID);
  auto loadedState = currentAsset ? Utils::FS::loadTextFile(currentAsset->path) : "{}";
  graph.deserialize(loadedState);
  savedState = graph.serialize();
  //name = "Node-Editor - ";
  name = currentAsset ? currentAsset->name : "*New Graph*";

  auto createPopup = [](Project::Graph::Graph &graph, ImFlow::Pin* pin)
  {
    ImGui::Text("Create New");
    ImGui::Separator();
    auto &names = Project::Graph::Graph::getNodeNames();
    for(size_t i = 0; i < names.size(); ++i) {
      if(ImGui::Selectable(names[i].c_str())) {

        auto newPos = pin ? pin->getParent()->getPos() : ImVec2{0,0};
        newPos.x += 150;
        auto node = graph.addNode(static_cast<uint32_t>(i), newPos);

        auto &ins = node->getIns();
        if(pin && !ins.empty())ins[0]->createLink(pin);

        node->setPos(newPos);
        ImGui::CloseCurrentPopup();
      }
    }
  };

  graph.graph.droppedLinkPopUpContent([&](ImFlow::Pin* pin)
  {
    createPopup(graph, pin);
  });

  graph.graph.rightClickPopUpContent([&](ImFlow::BaseNode* node)
  {
    if(node) {
      if(ImGui::Selectable(ICON_MDI_CONTENT_COPY " Duplicate")) {
        auto nodeP64 = (Project::Graph::Node::Base*)(node);
        ImVec2 newPos{
          node->getPos().x + node->getSize().x,
          node->getPos().y + 20.0f,
        };
        nlohmann::json jNode;
        nodeP64->serialize(jNode);
        auto newNode = graph.addNode(nodeP64->type, newPos);
        newNode->deserialize(jNode);
        ImGui::CloseCurrentPopup();
      }
      if(ImGui::Selectable(ICON_MDI_TRASH_CAN_OUTLINE " Remove")) {
        node->destroy();
        ImGui::CloseCurrentPopup();
      }
    } else {
      createPopup(graph, nullptr);
    }
  });

}

Editor::NodeEditor::~NodeEditor()
{
}

bool Editor::NodeEditor::draw(ImGuiID defDockId)
{
  if(!currentAsset)
  {
    return false;
  }

  if(!isInit)
  {
    isInit = true;
    //ImGui::SetNextWindowDockID(defDockId, ImGuiCond_Once);
    ImGui::SetNextWindowSize({800,600}, ImGuiCond_Once);
  }

  bool isOpen = true;
  ImGui::Begin(name.c_str(), &isOpen, ImGuiWindowFlags_NoCollapse);
  graph.graph.setSize(ImGui::GetContentRegionAvail());
  graph.graph.update();
  ImGui::End();

  auto currentState = graph.serialize();
  auto isDirtyNow = currentState != savedState;

  if (isDirtyNow) {
    if (!dirty || currentState != trackedDirtyState) {
      ctx.project->getAssets().markNodeGraphDirty(currentAsset->getUUID(), currentState);
      trackedDirtyState = currentState;
    }
  } else if (dirty) {
    ctx.project->getAssets().clearNodeGraphDirty(currentAsset->getUUID());
    trackedDirtyState.clear();
  }

  dirty = isDirtyNow;

  return isOpen;
}

void Editor::NodeEditor::save()
{
  if (!currentAsset) {
    return;
  }

  auto currentState = graph.serialize();
  Utils::FS::saveTextFile(currentAsset->path, currentState);
  Utils::FS::saveTextFile(currentAsset->path + ".conf", currentAsset->conf.serialize());
  savedState = currentState;
  trackedDirtyState.clear();
  dirty = false;
  ctx.project->getAssets().markNodeGraphSaved(currentAsset->getUUID(), savedState);
}

void Editor::NodeEditor::discardUnsavedChanges()
{
  if (!currentAsset) {
    return;
  }
  graph.deserialize(savedState);
  trackedDirtyState.clear();
  dirty = false;
  ctx.project->getAssets().clearNodeGraphDirty(currentAsset->getUUID());
}
