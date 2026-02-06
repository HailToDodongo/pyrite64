/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "nodeEditor.h"

#include "imgui.h"
#include "../../../context.h"
#include "../../../utils/logger.h"
#include "../../imgui/helper.h"
#include "../../imgui/lang.h"

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
  graph.deserialize(currentAsset
    ? Utils::FS::loadTextFile(currentAsset->path)
    : "{}"
  );
  //name = "Node-Editor - ";
  name = currentAsset ? currentAsset->name : message(MSG_GRAPH_NEW);

  auto createPopup = [](Project::Graph::Graph &graph, ImFlow::Pin* pin)
  {
    ImGui::Text(message(MSG_GRAPH_CREATE));
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
      if(ImGui::Selectable((std::string{ICON_MDI_CONTENT_COPY} + message(MSG_GRAPH_DUPLICATE)).c_str())) {
        auto nodeP64 = (Project::Graph::Node::Base*)(node);
        auto newPos = node->getPos() + ImVec2{node->getSize().x, 20};
        nlohmann::json jNode;
        nodeP64->serialize(jNode);
        auto newNode = graph.addNode(nodeP64->type, newPos);
        newNode->deserialize(jNode);
        ImGui::CloseCurrentPopup();
      }
      if(ImGui::Selectable((std::string{ICON_MDI_TRASH_CAN_OUTLINE} + message(MSG_GRAPH_REMOVE)).c_str())) {
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

  return isOpen;
}

void Editor::NodeEditor::save()
{
  Utils::FS::saveTextFile(currentAsset->path, graph.serialize());
}
