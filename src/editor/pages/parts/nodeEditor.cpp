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
#include "../../../project/graph/nodes/scriptNode.h"
#include "../../../project/graph/nodeRegistry.h"
#include "../../../project/graph/valueTypes.h"
#include "../../../utils/fs.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

#include <map>
#include <vector>
#include <array>
#include <algorithm>
#include <cctype>

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
  stylePin.extra.animated = true; // control-flow links render as a flowing dashed line

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
  savedState = graph.serialize(false);
  //name = "Node-Editor - ";
  name = currentAsset ? currentAsset->name : "*New Graph*";

  graph.graph.droppedLinkPopUpContent([this](ImFlow::Pin* pin)
  {
    drawCreateMenu(pin);
  });

  graph.graph.rightClickPopUpContent([this](ImFlow::BaseNode* node)
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
        auto newNode = graph.addNode(nodeP64->typeId(), newPos);
        if(newNode)newNode->deserialize(jNode);
        ImGui::CloseCurrentPopup();
      }
      if(ImGui::Selectable(ICON_MDI_TRASH_CAN_OUTLINE " Remove")) {
        node->destroy();
        ImGui::CloseCurrentPopup();
      }
    } else {
      drawCreateMenu(nullptr);
    }
  });

}

Editor::NodeEditor::~NodeEditor()
{
}

void Editor::NodeEditor::drawCreateMenu(ImFlow::Pin* pin)
{
  using Project::Graph::Node::NodeSpec;
  using Project::Graph::Node::canConnect;
  const ImVec2 openPos = graph.graph.screen2grid(ImGui::GetMousePosOnOpeningCurrentPopup());

  // When the menu was opened by dropping a dragged link, figure out what type it carries
  // and which way it points, so we can offer only nodes with a pin that could accept it.
  std::string dragType{};
  bool dragFromOutput = false;
  if(pin) {
    auto *dragNode = static_cast<Project::Graph::Node::Base*>(pin->getParent());
    dragFromOutput = pin->getType() == ImFlow::PinType_Output;
    if(dragNode) dragType = dragFromOutput ? dragNode->outPinType(pin) : dragNode->inPinType(pin);
  }
  auto pinTypeId = [](const auto &p) -> std::string {
    if(!p.value) return Project::Graph::Node::LOGIC_TYPE;
    return p.valueType.empty() ? std::string{"f32"} : p.valueType;
  };
  // Spec is offered only if one of its pins (on the side facing the drag) can connect, casts included
  auto specMatches = [&](const NodeSpec* s) -> bool {
    if(!pin) return true;
    const auto &pins = dragFromOutput ? s->inputs : s->outputs;
    for(const auto &p : pins) {
      if(dragFromOutput ? canConnect(dragType, pinTypeId(p)) : canConnect(pinTypeId(p), dragType)) return true;
    }
    return false;
  };

  auto addSpec = [&](const NodeSpec* spec) {
    auto node = graph.addNode(spec->id, openPos);
    if(!node)return;
    if(pin) {
      auto &pins = dragFromOutput ? node->getIns() : node->getOuts();
      for(auto &np : pins) {
        std::string t = dragFromOutput ? node->inPinType(np.get()) : node->outPinType(np.get());
        if(dragFromOutput ? canConnect(dragType, t) : canConnect(t, dragType)) { np->createLink(pin); break; }
      }
    }
    node->setPos(openPos);
    ImGui::CloseCurrentPopup();
  };

  // Visible label, i.e. the text after the leading icon glyph.
  auto labelOf = [](const NodeSpec* s) {
    auto sp = s->name.find(' ');
    return sp == std::string::npos ? s->name : s->name.substr(sp + 1);
  };
  auto byLabel = [&](const NodeSpec* a, const NodeSpec* b) { return labelOf(a) < labelOf(b); };

  // Search box (first entry), focused when the menu opens. The opening right-click can
  // steal focus, so keep trying for a few frames until the input is actually active.
  if(ImGui::IsWindowAppearing()) { nodeMenuSearch.clear(); nodeMenuFocusFrames = 3; }
  if(nodeMenuFocusFrames > 0) {
    --nodeMenuFocusFrames;
    if(!ImGui::IsAnyItemActive()) ImGui::SetKeyboardFocusHere();
  }
  ImGui::SetNextItemWidth(180.0f);
  ImGui::InputTextWithHint("##search", ICON_MDI_MAGNIFY " Search", &nodeMenuSearch);
  ImGui::Separator();

  // MenuItem uses the label as its id; node names aren't unique (e.g. "Sin"), so scope
  // each item by its stable spec id.
  auto menuItem = [&](const NodeSpec* spec, const char* shortcut) {
    ImGui::PushID(spec->id.c_str());
    bool clicked = ImGui::MenuItem(spec->name.c_str(), shortcut);
    ImGui::PopID();
    if(clicked) addSpec(spec);
  };

  if(!nodeMenuSearch.empty()) {
    // Flat, filtered list (max 10), matched case-insensitively on the label.
    std::string q = nodeMenuSearch;
    std::transform(q.begin(), q.end(), q.begin(), [](unsigned char c){ return std::tolower(c); });

    std::vector<const NodeSpec*> matches{};
    for(const auto *spec : Project::Graph::Node::getNodeSpecs()) {
      if(!specMatches(spec)) continue;
      std::string l = labelOf(spec);
      std::transform(l.begin(), l.end(), l.begin(), [](unsigned char c){ return std::tolower(c); });
      if(l.find(q) != std::string::npos) matches.push_back(spec);
    }
    std::sort(matches.begin(), matches.end(), byLabel);

    if(matches.empty()) ImGui::TextDisabled("No matches");
    int shown = 0;
    for(const auto *spec : matches) {
      if(shown++ >= 10)break;
      menuItem(spec, spec->category.c_str()); // category shown gray + right-aligned
    }
    return;
  }

  // Categorized submenus + ungrouped entries.
  std::map<std::string, std::vector<const NodeSpec*>> groups{};
  std::vector<const NodeSpec*> ungrouped{};
  for(const auto *spec : Project::Graph::Node::getNodeSpecs()) {
    if(!specMatches(spec)) continue;
    if(spec->category.empty()) ungrouped.push_back(spec);
    else                       groups[spec->category].push_back(spec);
  }

  for(auto &[cat, specs] : groups) {
    std::sort(specs.begin(), specs.end(), byLabel);
    if(ImGui::BeginMenu(cat.c_str())) {
      for(const auto *spec : specs) menuItem(spec, nullptr);
      ImGui::EndMenu();
    }
  }
  std::sort(ungrouped.begin(), ungrouped.end(), byLabel);
  for(const auto *spec : ungrouped) menuItem(spec, nullptr);
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

  // Pick up edits to the project's node scripts while the editor is open. Skipped
  // during a build, since that reads node specs from the worker thread.
  if(!ctx.isBuildOrRunning()) {
    Project::Graph::Node::pollUserNodeReload();
  }

  bool isOpen = true;
  if(ImGui::Begin(name.c_str(), &isOpen, ImGuiWindowFlags_NoCollapse)) {
    // Keep the variable nodes' pin types in sync with this graph's declarations.
    Project::Graph::Node::setActiveGraphVars(&graph.variables);
    syncVariablePins();

    if(showVarsPanel) {
      ImGui::BeginChild("graphVarsPanel", ImVec2(210.0f, 0), true);
      drawVariablesPanel();
      ImGui::EndChild();
      ImGui::SameLine();
    }

    ImGui::BeginChild("graphCanvas", ImVec2(0, 0), false);
    graph.graph.setSize(ImGui::GetContentRegionAvail());

    // Keep the canvas background/grid in sync with the active theme (applied each
    // frame so live theme switches take effect). config().color drives the actual fill.
    auto &gridStyle = graph.graph.getStyle();
    gridStyle.colors.background = ImGui::Theme::getColorU32("nodeBackground", IM_COL32(33, 41, 45, 255));
    gridStyle.colors.grid = ImGui::Theme::getColorU32("nodeGrid", IM_COL32(200, 200, 200, 40));
    graph.graph.getGrid().config().color = gridStyle.colors.background;

    // Theme each node's body background (header color stays per node-type).
    ImU32 nodeBody = ImGui::Theme::getColorU32("nodeBody", IM_COL32(55, 64, 75, 255));
    for(auto &[uid, node] : graph.graph.getNodes()) {
      if(node && node->getStyle()) node->getStyle()->bg = nodeBody;
    }

    graph.graph.update();
    ImGui::EndChild();

    Project::Graph::Node::setActiveGraphVars(nullptr);
  }
  ImGui::End();

  auto currentState = graph.serialize(false);
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

  Utils::FS::saveTextFile(currentAsset->path, graph.serialize(true));
  Utils::FS::saveTextFile(currentAsset->path + ".conf", currentAsset->conf.serialize());
  savedState = graph.serialize(false);
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

void Editor::NodeEditor::resetView()
{
  auto &nodes = graph.graph.getNodes();
  if(nodes.empty())return;

  ImVec2 mn{FLT_MAX, FLT_MAX}, mx{-FLT_MAX, -FLT_MAX};
  for(auto &[uid, node] : nodes) {
    ImVec2 p = node->getPos(), s = node->getSize();
    mn.x = std::min(mn.x, p.x);        mn.y = std::min(mn.y, p.y);
    mx.x = std::max(mx.x, p.x + s.x);  mx.y = std::max(mx.y, p.y + s.y);
  }

  ImVec2 canvas = graph.graph.getGrid().size();
  if(canvas.x < 1.0f || canvas.y < 1.0f)return;

  float bw = std::max(mx.x - mn.x, 1.0f), bh = std::max(mx.y - mn.y, 1.0f);
  float scale = std::clamp(std::min(canvas.x / bw, canvas.y / bh) * 0.9f, 0.3f, 1.0f);

  ImVec2 center{(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f};
  graph.graph.setScale(scale);
  graph.graph.setScroll({canvas.x / (2.0f * scale) - center.x, canvas.y / (2.0f * scale) - center.y});
}

void Editor::NodeEditor::addGroup()
{
  // Place a new group centred in the current view.
  ImVec2 canvas = graph.graph.getGrid().size();
  float scale = graph.graph.getScale();
  ImVec2 scroll = graph.graph.getScroll();
  ImVec2 center{canvas.x / (2.0f * scale) - scroll.x, canvas.y / (2.0f * scale) - scroll.y};
  ImVec2 size{125.0f, 60.0f};
  graph.graph.addGroup("Group", {center.x - size.x * 0.5f, center.y - size.y * 0.5f}, size);
}

void Editor::NodeEditor::drawVariablesPanel()
{
  ImGui::TextUnformatted("Tools");
  ImGui::Separator();
    if(ImGui::Button(ICON_MDI_FIT_TO_PAGE_OUTLINE " Reset View", ImVec2(-FLT_MIN, 0))) resetView();
    if(ImGui::Button(ICON_MDI_SHAPE_RECTANGLE_PLUS " Add Group", ImVec2(-FLT_MIN, 0))) addGroup();
  ImGui::Spacing();

  ImGui::TextUnformatted("Settings");
  ImGui::Separator();

    ImGui::Checkbox("Repeat", &graph.repeatable);
    if(ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Loop the graph: when a flow path ends, wait a frame and\nrestart at the Start node.");
    }

  ImGui::Spacing();

  ImGui::TextUnformatted("Variables");
  ImGui::Separator();

  static const std::array<const char*, 6> kTypes = {"i32", "u32", "f32", "vec3", "quat", "objref"};

  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float rowH = ImGui::GetFrameHeight();
  const float comboW = 64.0f;

  int removeIdx = -1;
  for(size_t i = 0; i < graph.variables.size(); ++i) {
    auto &v = graph.variables[i];
    ImGui::PushID((int)i);
    float nameW = ImGui::GetContentRegionAvail().x - comboW - rowH - 2.0f * spacing;
    ImGui::SetNextItemWidth(nameW);
    ImGui::InputText("##name", &v.name);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(comboW);
    if(ImGui::BeginCombo("##type", v.type.c_str())) {
      for(auto *t : kTypes) {
        if(ImGui::Selectable(t, v.type == t)) v.type = t;
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    if(ImGui::Button(ICON_MDI_CLOSE, ImVec2(rowH, rowH))) removeIdx = (int)i;
    ImGui::PopID();
  }
  if(removeIdx >= 0) graph.variables.erase(graph.variables.begin() + removeIdx);

  if(ImGui::Button(ICON_MDI_PLUS " Add Variable", ImVec2(-FLT_MIN, 0))) {
    graph.variables.push_back({"var" + std::to_string(graph.variables.size()), "i32"});
  }
}

void Editor::NodeEditor::syncVariablePins()
{
  auto typeOf = [&](const std::string &varName) -> std::string {
    for(auto &v : graph.variables) if(v.name == varName) return v.type;
    return "i32";
  };
  for(auto &[uid, node] : graph.graph.getNodes()) {
    auto *sn = static_cast<Project::Graph::Node::ScriptNode*>(node.get());
    Project::Graph::Node::applyVarPinTypes(*sn, typeOf(sn->getStr("var")));
  }
}
