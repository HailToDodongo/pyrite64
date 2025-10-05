/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include "imgui.h"
#include "IconsFontAwesome4.h"
#include "../../utils/filePicker.h"
#include "../../utils/color.h"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"

namespace ImGui::InpTable
{
  inline void start(const char *name)
  {
    ImGui::BeginTable(name, 2);
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Input", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(1);
    ImGui::PushItemWidth(-FLT_MIN);
  }

  inline void end() {
    ImGui::EndTable();
  }

  inline void add(const std::string &name) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::Text(name.c_str());
    ImGui::TableSetColumnIndex(1);
  }

  inline void addString(const std::string &name, std::string &str) {
    ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text(name.c_str());
      ImGui::TableSetColumnIndex(1);

      auto labelHidden = "##" + name;
      ImGui::InputText(labelHidden.c_str(), &str);
  }

  inline void addComboBox(const std::string &name, int &itemCurrent, const char* const items[], int itemsCount) {
    ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text(name.c_str());
      ImGui::TableSetColumnIndex(1);

      auto labelHidden = "##" + name;
      ImGui::Combo(labelHidden.c_str(), &itemCurrent, items, itemsCount);
  }

  inline void addCheckBox(const std::string &name, bool &value) {
    ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text(name.c_str());
      ImGui::TableSetColumnIndex(1);

      auto labelHidden = "##" + name;
      ImGui::Checkbox(labelHidden.c_str(), &value);
  }

  inline void addInputFloat(const std::string &name, float &value) {
    ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text(name.c_str());
      ImGui::TableSetColumnIndex(1);

      auto labelHidden = "##" + name;
      ImGui::InputFloat(labelHidden.c_str(), &value);
  }

  inline void addInputVec3(const std::string &name, glm::vec3 &v) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::Text(name.c_str());
    ImGui::TableSetColumnIndex(1);

    auto labelHidden = "##0" + name;
    ImGui::InputFloat3(labelHidden.c_str(), &v.x);
  }

  inline void addInputQuat(const std::string &name, glm::quat &v) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::Text(name.c_str());
    ImGui::TableSetColumnIndex(1);

    auto labelHidden = "##0" + name;
    ImGui::InputFloat4(labelHidden.c_str(), &v.x);
  }

  inline void addInputInt(const std::string &name, int &value) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::Text(name.c_str());
    ImGui::TableSetColumnIndex(1);

    auto labelHidden = "##" + name;
    ImGui::InputInt(labelHidden.c_str(), &value);
  }

  inline void addColor(const std::string &name, Utils::Color &color, bool withAlpha = true) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::Text(name.c_str());
    ImGui::TableSetColumnIndex(1);

    if (withAlpha) {
      ImGui::ColorEdit4(name.c_str(), color.rgba, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    } else {
      ImGui::ColorEdit3(name.c_str(), color.rgba, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    }
  }

  inline void addPath(const std::string &name, std::string &str, bool isDir = false, const std::string &placeholder = "") {

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::Text(name.c_str());
    ImGui::TableSetColumnIndex(1);

    auto labelHidden = "##" + name;
    ImGui::PushID(labelHidden.c_str());
    if (ImGui::Button(ICON_FA_FOLDER)) {
      Utils::FilePicker::open([&str](const std::string &path) {
        str = path;
      }, isDir);
    }
    ImGui::PopID();
    ImGui::SameLine();

    if (placeholder.empty()) {
      ImGui::InputText(labelHidden.c_str(), &str);
    } else {
      ImGui::InputTextWithHint(labelHidden.c_str(), placeholder.c_str(), &str);
    }
  }

}
