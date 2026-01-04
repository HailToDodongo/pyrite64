/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "IconsMaterialDesignIcons.h"
#include "../../project/project.h"
#include "../../utils/filePicker.h"
#include "../../utils/prop.h"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"

namespace TPL
{
  template <typename C, typename T>
  decltype(auto) access(C& cls, T C::*member) {
    return (cls.*member);
  }

  template <typename C, typename T, typename... Mems>
  decltype(auto) access(C& cls, T C::*member, Mems... rest) {
    return access((cls.*member), rest...);
  }
}

namespace ImGui
{
  bool IconButton(const char* label, const ImVec2 &labelSize, const ImVec4 &color = ImVec4{1,1,1,1});

  inline bool IconToggle(bool &state, const char* labelOn, const char* labelOff, const ImVec2 &labelSize)
  {
    if(IconButton(
      state ? labelOn : labelOff,
      labelSize,
      state ? ImVec4{1,1,1,1} : ImVec4{0.6f,0.6f,0.6f,1}
    )) {
      state = !state;
      return true;
    }
    return false;
  }

  template<typename T>
  int VectorComboBox(
    const std::string &name,
    const std::vector<T> &items,
    auto &id
  ) {
    int idx = 0;
    for (const auto &item : items) {
      if (id == item.getId())break;
      ++idx;
    }
    auto getter = [](void* itemsLocal, int idx)
    {
      auto &items = *static_cast<std::vector<T>*>(itemsLocal);
      if (idx >= 0 && idx < (int)items.size()) {
        return items[idx].getName().c_str();
      }
      return "<None>";
    };

    ImGui::Combo(name.c_str(), &idx, getter, (void*)&items, (int)items.size());
    id = items[idx].getId();
    return idx;
  }
}

namespace ImTable
{
  extern Project::Object *obj;

  struct ComboEntry
  {
    uint32_t value;
    std::string name;
    uint32_t getId() const { return value; }
    const std::string &getName() const { return name; }
  };

  inline bool start(const char *name, Project::Object *nextObj = nullptr)
  {
    obj = nullptr;
    if (!ImGui::BeginTable(name, 2))return false;
    obj = nextObj;
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Input", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(1);
    ImGui::PushItemWidth(-FLT_MIN);
    return true;
  }

  inline void end() {
    obj = nullptr;
    ImGui::EndTable();
  }

  inline void add(const std::string &name) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", name.c_str());
    ImGui::TableSetColumnIndex(1);
  }

  template<typename T>
  inline int addVecComboBox(const std::string &name, const std::vector<T> &items, auto &id)
  {
    add(name);
    bool disabled  (obj && obj->isPrefabInstance() && !obj->isPrefabEdit);
    if(disabled)ImGui::BeginDisabled();
    int res = ImGui::VectorComboBox(name, items, id);
    if(disabled)ImGui::EndDisabled();
    return res;
  }

  inline void addComboBox(const std::string &name, int &itemCurrent, const char* const items[], int itemsCount) {
    add(name);
    bool disabled  (obj && obj->isPrefabInstance() && !obj->isPrefabEdit);
    auto labelHidden = "##" + name;
    if(disabled)ImGui::BeginDisabled();
    ImGui::Combo(labelHidden.c_str(), &itemCurrent, items, itemsCount);
    if(disabled)ImGui::EndDisabled();
  }

  inline void addComboBox(const std::string &name, int &itemCurrent, const std::vector<const char*> &items) {
    add(name);
    bool disabled  (obj && obj->isPrefabInstance() && !obj->isPrefabEdit);
    if(disabled)ImGui::BeginDisabled();
    auto labelHidden = "##" + name;
    ImGui::Combo(labelHidden.c_str(), &itemCurrent, items.data(), (int)items.size());
    if(disabled)ImGui::EndDisabled();
  }

  inline void addCheckBox(const std::string &name, bool &value) {
    add(name);
    bool disabled  (obj && obj->isPrefabInstance() && !obj->isPrefabEdit);
    if(disabled)ImGui::BeginDisabled();
    auto labelHidden = "##" + name;
    ImGui::Checkbox(labelHidden.c_str(), &value);
    if(disabled)ImGui::BeginDisabled();
  }

  inline void addBitMask8(const std::string &name, uint32_t &value)
  {
    add(name);
    bool disabled  (obj && obj->isPrefabInstance() && !obj->isPrefabEdit);
    if(disabled)ImGui::BeginDisabled();
    auto labelHidden = "##" + name;
    // 8 checkboxes
    for (int i = 0; i < 8; ++i) {
      bool bit = (value & (1 << i)) != 0;
      if (ImGui::Checkbox(labelHidden.c_str(), &bit)) {
        if (bit) {
          value |= (1 << i);
        } else {
          value &= ~(1 << i);
        }
      }
      labelHidden += "1";
      if (i < 7)ImGui::SameLine();
    }

    if(disabled)ImGui::EndDisabled();
  }

  template<typename T>
  bool typedInput(T *value)
  {
    if constexpr (std::is_same_v<T, float>) {
      return ImGui::InputFloat("##", value);
    } else if constexpr (std::is_same_v<T, bool>) {
      return ImGui::Checkbox("##", value);
    } else if constexpr (std::is_same_v<T, int>) {
      return ImGui::InputInt("##", value);
    } else if constexpr (std::is_same_v<T, uint32_t>) {
      return ImGui::InputScalar("##", ImGuiDataType_U32, value);
    } else if constexpr (std::is_same_v<T, uint16_t>) {
      return ImGui::InputScalar("##", ImGuiDataType_U16, value);
    } else if constexpr (std::is_same_v<T, uint8_t>) {
      return ImGui::InputScalar("##", ImGuiDataType_U8, value);
    } else if constexpr (std::is_same_v<T, glm::vec3>) {
      return ImGui::InputFloat3("##", glm::value_ptr(*value));
    } else if constexpr (std::is_same_v<T, glm::quat>) {
      return ImGui::InputFloat4("##", glm::value_ptr(*value));
    } else if constexpr (std::is_same_v<T, std::string>) {
      return ImGui::InputText("##", value);
    } else {
      static_assert(false, "Unsupported type for typedInput");
    }
    return false;
  }

  template<typename T>
  bool add(const std::string &name, T &value) {
    add(name);
    bool disabled = false;
    if(obj && obj->isPrefabInstance() && !obj->isPrefabEdit)disabled = true;
    ImGui::PushID(name.c_str());
    if(disabled)ImGui::BeginDisabled();
    bool res = typedInput<T>(&value);
    if(disabled)ImGui::EndDisabled();
    ImGui::PopID();
    return res;
  }

  template<typename T>
  bool addProp(const std::string &name, Property<T> &prop)
  {
    add(name);
    ImGui::PushID(name.c_str());
    bool res = typedInput<T>(&prop.value);
    ImGui::PopID();
    return res;
  }

  template<typename T>
  bool addObjProp(
    const std::string &name,
    Property<T> &prop
  )
  {
    if(!obj)return false;
    bool res{};
    ImTable::add(name);
    ImGui::PushID(&prop);

    bool isOverride{true};

    T *val = &prop.value;
    if(obj->isPrefabInstance() && !obj->isPrefabEdit) {
      val = &prop.resolve(obj->propOverrides, &isOverride);
    }
    if(!isOverride)ImGui::BeginDisabled();

    if(obj && obj->isPrefabInstance() && !obj->isPrefabEdit)
    {
      bool isOverrideLocal = isOverride;
      if(ImGui::IconToggle(
        isOverrideLocal,
        ICON_MDI_LOCK_OPEN,
        ICON_MDI_LOCK,
        ImVec2{16,16}
      )) {
        if(isOverrideLocal) {
          obj->addPropOverride(prop);
        } else {
          obj->removePropOverride(prop);
        }
      }
      ImGui::SameLine();
    }

    res = typedInput<T>(val);

    if(!isOverride)ImGui::EndDisabled();

    ImGui::PopID();
    return res;
  }

  inline void addColor(const std::string &name, glm::vec4 &color, bool withAlpha = true) {
    add(name);
    bool disabled = (obj && obj->uuidPrefab.value);
    if(disabled)ImGui::BeginDisabled();
    if (withAlpha) {
      ImGui::ColorEdit4(name.c_str(), glm::value_ptr(color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    } else {
      ImGui::ColorEdit3(name.c_str(), glm::value_ptr(color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    }
    if(disabled)ImGui::EndDisabled();
  }

  inline void addPath(const std::string &name, std::string &str, bool isDir = false, const std::string &placeholder = "") {
    add(name);
    auto labelHidden = "##" + name;
    ImGui::PushID(labelHidden.c_str());
    if (ImGui::Button(ICON_MDI_FOLDER_OUTLINE)) {
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
