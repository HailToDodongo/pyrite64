/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include "baseNode.h"
#include "../../../utils/hash.h"
#include "../../../editor/imgui/lang.h"

namespace Project::Graph::Node
{
  class Note : public Base
  {
    private:
      std::string text{};
      ImVec2 size{200, 100}; // Size of the group node

    public:
      constexpr static const char* ICON = ICON_MDI_CLIPBOARD_OUTLINE;
      constexpr static const char* NAME = MSG_GRAPH_NODE_NOTE;

      static std::string getname() {
        return std::string{ICON} + Editor::message(NAME);
      }

      Note()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(getname());
        // Remove visible style, set fully transparent background and border
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(0, 0, 0, 0x20), ImColor(0xFF, 0xFF, 0xFF, 0xFF), 0.0f));
      }

      void draw() override {
        auto editor = getHandler();
        if (!editor) {
          return;
        }
        ImGui::SetNextItemWidth(size.x - 10);
        ImGui::InputText("##note_text", &text);
        // Get viewport offset and zoom
        ImVec2 scroll = editor->getScroll();
        float scale = editor->getGrid().scale();
        // Get node position (assume Base provides getPos())
        ImVec2 nodePos = getPos();
        // Convert to screen coordinates
        ImVec2 screenPos = (nodePos + scroll) * scale;
        ImVec2 screenSize = size * scale;
        // Draw invisible button for resizing
        //ImGui::SetCursorPos(screenPos);
        ImGui::InvisibleButton("group_resize", size);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
          ImVec2 delta = ImGui::GetIO().MouseDelta;
          size.x = std::max(50.0f, size.x + delta.x / scale);
          size.y = std::max(50.0f, size.y + delta.y / scale);
        }
        // Draw resize handle
        //ImGui::SetCursorPos(screenPos + ImVec2(screenSize.x - 16 * scale, screenSize.y - 16 * scale));
        //ImGui::Button("resize_handle", ImVec2(16 * scale, 16 * scale));
      }

      void serialize(nlohmann::json &j) override {
        j["text"] = text;
        j["size"] = {size.x, size.y};
      }

      void deserialize(nlohmann::json &j) override {
        text = j.value("text", "");
        if (j.contains("size") && j["size"].is_array() && j["size"].size() == 2) {
          size.x = j["size"][0].get<float>();
          size.y = j["size"][1].get<float>();
        }
      }

      void build(BuildCtx &ctx) override {
      }
  };
}