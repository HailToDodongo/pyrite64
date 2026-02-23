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
#include "../../context.h"
#include "../undoRedo.h"
#include "../../utils/filePicker.h"
#include "../../utils/prop.h"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <functional>

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
  inline bool CollapsingSubHeader(const char* label, ImGuiTreeNodeFlags flags = 0)
  {
    ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
    ImVec2 subHeaderItemSpacing = ImGui::GetStyle().ItemSpacing;
    subHeaderItemSpacing.y -= 4;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, subHeaderItemSpacing);

    auto isOpen = ImGui::CollapsingHeader(label, flags);

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(1);
    return isOpen;
  }

  bool IconButton(const char* label, const ImVec2 &labelSize, const ImVec4 &color = ImVec4{1,1,1,1});

  inline bool IconToggle(bool &state, const char* labelOn, const char* labelOff, const ImVec2 &labelSize)
  {
    if(IconButton(
      state ? labelOn : labelOff,
      labelSize,
      state ? ImVec4{1,1,1,1} : ImVec4{0.6f,0.6f,0.6f,1}
    )) {
      Editor::UndoRedo::getHistory().markChanged("Toggle Property");
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
    if(idx >= (int)items.size())idx = 0;
    if(idx < (int)items.size())id = items[idx].getId();
    return idx;
  }

  // Generic drag-drop target handler for combo boxes
  // Validator signature: bool(uint64_t uuid, const char* payloadType)
  // Returns true if a valid drop was accepted
  template<typename TId, typename TValidator>
  bool HandleComboBoxDragDrop(TId& targetId, TValidator validator)
  {
    if (!ImGui::BeginDragDropTarget()) return false;
    
    bool changed = false;
    
    // Handle ASSET payload
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET")) {
      uint64_t uuid = *((uint64_t*)payload->Data);
      if (validator(uuid, "ASSET")) {
        auto next = static_cast<TId>(uuid);
        if (targetId != next) {
          targetId = next;
          changed = true;
        }
      }
    }
    
    // Handle OBJECT payload  
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("OBJECT")) {
      uint32_t uuid = *((uint32_t*)payload->Data);
      if (validator(uuid, "OBJECT")) {
        auto next = static_cast<TId>(uuid);
        if (targetId != next) {
          targetId = next;
          changed = true;
        }
      }
    }
    
    ImGui::EndDragDropTarget();
    return changed;
  }

}

namespace ImTable
{
  extern Project::Object *obj;
  inline bool prefabEditOverride{false};

  // Checks if the current object is a prefab instance and not in edit mode, or if the prefab edit override is active.
  inline bool isPrefabLocked(const Project::Object *target = nullptr)
  {
    const auto *ref = target ? target : obj;
    if (!ref) return false;
    if (prefabEditOverride) return false;
    return ref->isPrefabInstance() && !ref->isPrefabEdit;
  }

  struct PrefabEditScope
  {
    bool prev{false};
    explicit PrefabEditScope(bool allow) : prev(prefabEditOverride)
    {
      prefabEditOverride = allow;
    }
    ~PrefabEditScope()
    {
      prefabEditOverride = prev;
    }
  };

  struct ComboEntry
  {
    uint32_t value;
    std::string name;
    uint32_t getId() const { return value; }
    const std::string &getName() const { return name; }
  };

  inline bool start(const char *name, Project::Object *nextObj = nullptr, float width = -1)
  {
    obj = nullptr;
    if (!ImGui::BeginTable(name, 2))return false;
    obj = nextObj;
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Input", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(1);
    ImGui::PushItemWidth(width >= 0 ? width : -FLT_MIN);
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

  template<typename GetLabel, typename ApplySelection>
  inline bool drawComboSelection(
    const char* label,
    int count,
    int &current,
    const char* preview,
    const std::string &snapshotLabel,
    GetLabel getLabel,
    ApplySelection applySelection
  ) {
    bool changed = false;
    if (ImGui::BeginCombo(label, preview)) {
      for (int i = 0; i < count; ++i) {
        bool selected = (i == current);
        if (ImGui::Selectable(getLabel(i), selected)) {
          if (obj) {
            Editor::UndoRedo::getHistory().markChanged(snapshotLabel);
            applySelection(i);
          } else {
            applySelection(i);
          }
          current = i;
          changed = true;
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    return changed;
  }

  template<typename T, typename OnChange>
  inline int addVecComboBox(const std::string &name, const std::vector<T> &items, auto &id, OnChange onChange)
  {
    add(name);
    bool disabled  (isPrefabLocked());
    if(disabled)ImGui::BeginDisabled();
    int idx = 0;
    for (const auto &item : items) {
      if (id == item.getId())break;
      ++idx;
    }
    const char* preview = "<None>";
    if (idx >= 0 && idx < (int)items.size()) {
      preview = items[idx].getName().c_str();
    }

    drawComboSelection(
      ("##" + name).c_str(),
      (int)items.size(),
      idx,
      preview,
      "Edit " + name,
      [&items](int i) { return items[i].getName().c_str(); },
      [&items, &id, &onChange](int i) {
        id = items[i].getId();
        onChange(id);
      }
    );
    if(disabled)ImGui::EndDisabled();
    return idx;
  }

  template<typename T>
  inline int addVecComboBox(const std::string &name, const std::vector<T> &items, auto &id)
  {
    return addVecComboBox(name, items, id, [](auto) {});
  }

  // addVecComboBox with drag-drop support and custom validator
  // Validator signature: bool(uint64_t uuid, const char* payloadType)
  template<typename T, typename TValidator, typename OnChange>
  inline int addVecComboBoxWithDragDrop(
    const std::string& name,
    const std::vector<T>& items,
    auto& id,
    TValidator validator,
    OnChange onChange
  )
  {
    auto oldId = id;
    addVecComboBox(name, items, id, onChange);
    
    if (ImGui::HandleComboBoxDragDrop(id, validator)) {
      onChange(id);
    }

    if (oldId != id) {
      Editor::UndoRedo::getHistory().markChanged("Edit " + name);
      return true;
    }
    return false;
  }

  // Asset-only drag-drop combo box
  // Validator signature: bool(uint64_t assetUUID)
  template<typename T, typename TAssetValidator, typename OnChange>
  inline int addAssetVecComboBox(
    const std::string& name,
    const std::vector<T>& items,
    auto& id,
    TAssetValidator assetValidator,
    OnChange onChange
  )
  {
    return addVecComboBoxWithDragDrop(name, items, id,
      [assetValidator](uint64_t uuid, const char* type) {
        if (strcmp(type, "ASSET") != 0) return false;
        return assetValidator(uuid);
      },
      onChange
    );
  }

  // overload: accept dropped assets that are present in the combo list.
  template<typename T, typename OnChange>
  inline int addAssetVecComboBox(
    const std::string& name,
    const std::vector<T>& items,
    auto& id,
    OnChange onChange
  )
  {
    return addAssetVecComboBox(name, items, id,
      [&items](uint64_t uuid) {
        for (const auto &item : items) {
          if (item.getId() == uuid) return true;
        }
        return false;
      },
      onChange
    );
  }

  // overload without OnChange callback.
  template<typename T>
  inline int addAssetVecComboBox(
    const std::string& name,
    const std::vector<T>& items,
    auto& id
  )
  {
    return addAssetVecComboBox(name, items, id, [](auto){});
  }

  // Object-only drag-drop combo box
  // Validator signature: bool(uint32_t objectUUID)
  template<typename T, typename TObjectValidator, typename OnChange>
  inline int addObjectVecComboBox(
    const std::string& name,
    const std::vector<T>& items,
    auto& id,
    TObjectValidator objectValidator,
    OnChange onChange
  )
  {
    return addVecComboBoxWithDragDrop(name, items, id,
      [objectValidator](uint64_t uuid, const char* type) {
        if (strcmp(type, "OBJECT") != 0) return false;
        return objectValidator(static_cast<uint32_t>(uuid));
      },
      onChange
    );
  }

  // overload: accept dropped objects that are present in the combo list.
  template<typename T, typename OnChange>
  inline int addObjectVecComboBox(
    const std::string& name,
    const std::vector<T>& items,
    auto& id,
    OnChange onChange
  )
  {
    return addObjectVecComboBox(name, items, id,
      [&items](uint32_t uuid) {
        for (const auto &item : items) {
          if (item.getId() == uuid) return true;
        }
        return false;
      },
      onChange
    );
  }

  // overload without OnChange callback.
  template<typename T>
  inline int addObjectVecComboBox(
    const std::string& name,
    const std::vector<T>& items,
    auto& id
  )
  {
    return addObjectVecComboBox(name, items, id, [](auto){});
  }

  inline bool addComboBox(const std::string &name, int &itemCurrent, const char* const items[], int itemsCount) {
    add(name);
    bool disabled  (isPrefabLocked());
    auto labelHidden = "##" + name;
    if(disabled)ImGui::BeginDisabled();
    const char* preview = (itemCurrent >= 0 && itemCurrent < itemsCount) ? items[itemCurrent] : "<None>";
    bool res = drawComboSelection(
      labelHidden.c_str(),
      itemsCount,
      itemCurrent,
      preview,
      "Edit " + name,
      [items](int i) { return items[i]; },
      [&itemCurrent](int i) { itemCurrent = i; }
    );
    if(disabled)ImGui::EndDisabled();
    return res;
  }

  inline bool addComboBox(const std::string &name, int &itemCurrent, const std::vector<const char*> &items) {
    add(name);
    bool disabled  (isPrefabLocked());
    if(disabled)ImGui::BeginDisabled();
    auto labelHidden = "##" + name;
    const char* preview = (itemCurrent >= 0 && itemCurrent < (int)items.size()) ? items[itemCurrent] : "<None>";
    bool res = drawComboSelection(
      labelHidden.c_str(),
      (int)items.size(),
      itemCurrent,
      preview,
      "Edit " + name,
      [&items](int i) { return items[i]; },
      [&itemCurrent](int i) { itemCurrent = i; }
    );
    if(disabled)ImGui::EndDisabled();
    return res;
  }

  inline void addCheckBox(const std::string &name, bool &value) {
    add(name);
    bool disabled  (isPrefabLocked());
    if(disabled)ImGui::BeginDisabled();
    auto labelHidden = "##" + name;
    bool changed = ImGui::Checkbox(labelHidden.c_str(), &value);
    if(changed)Editor::UndoRedo::getHistory().markChanged("Edit " + name);
    if(disabled)ImGui::EndDisabled();
  }

  inline void addBitMask8(const std::string &name, uint32_t &value)
  {
    add(name);
    bool disabled  (isPrefabLocked());
    if(disabled)ImGui::BeginDisabled();
    auto labelHidden = "##" + name;
    // 8 checkboxes
    for (int i = 0; i < 8; ++i) {
      bool bit = (value & (1 << i)) != 0;
      bool changed = ImGui::Checkbox(labelHidden.c_str(), &bit);
      if (changed) {
        if (bit) {
          value |= (1 << i);
        } else {
          value &= ~(1 << i);
        }

        Editor::UndoRedo::getHistory().markChanged("Edit " + name);
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
    } else if constexpr (std::is_same_v<T, glm::vec4>) {
      return ImGui::ColorEdit4("##", glm::value_ptr(*value), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    } else if constexpr (std::is_same_v<T, glm::quat>) {
      return ImGui::InputFloat4("##", glm::value_ptr(*value));
    } else if constexpr (std::is_same_v<T, glm::ivec2>) {
      return ImGui::InputInt2("##", glm::value_ptr(*value));
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
    bool disabled  (isPrefabLocked());
    ImGui::PushID(name.c_str());
    if(disabled)ImGui::BeginDisabled();
    bool changed = typedInput<T>(&value);
    if(changed)Editor::UndoRedo::getHistory().markChanged("Edit " + name);
    if(disabled)ImGui::EndDisabled();
    ImGui::PopID();
    return changed;
  }

  template<typename T>
  bool addProp(const std::string &name, Property<T> &prop)
  {
    add(name);
    ImGui::PushID(name.c_str());
    bool changed = typedInput<T>(&prop.value);
    if(changed)Editor::UndoRedo::getHistory().markChanged("Edit " + name);
    ImGui::PopID();
    return changed;
  }

  template<typename T>
  bool addObjProp(
    const std::string &name,
    Property<T> &prop,
    std::function<bool(T*)> editFunc,
    PropBool *propState
  )
  {
    if(!obj)return false;
    bool res{};

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();

    if(propState)
    {
      ImGui::PushFont(nullptr, 18.0f);

      if(ImGui::IconButton(
        propState->value
        ? ICON_MDI_CHECKBOX_MARKED_CIRCLE
        : ICON_MDI_CHECKBOX_BLANK_CIRCLE_OUTLINE,
        {24,24},
        ImVec4{1,1,1,1}
      )) {
        propState->value = !propState->value;
        Editor::UndoRedo::getHistory().markChanged("Edit " + name);
      }
      ImGui::PopFont();
      ImGui::SameLine();
    }

    ImGui::Text("%s", name.c_str());
    ImGui::TableSetColumnIndex(1);

    ImGui::PushID(&prop);

    bool isOverride{true};

    T *val = &prop.value;
    if(isPrefabLocked()) {
      val = &prop.resolve(obj->propOverrides, &isOverride);
    }

    bool isDisabled = !isOverride;
    if(propState && !propState->value)isDisabled = true;

    if(isDisabled)ImGui::BeginDisabled();

    if(isPrefabLocked())
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

    res = editFunc(val);
    if (res) Editor::UndoRedo::getHistory().markChanged("Edit " + name);

    if(isDisabled)ImGui::EndDisabled();

    ImGui::PopID();
    return res;
  }

  template<typename T>
  bool addObjProp(
    const std::string &name,
    Property<T> &prop,
    PropBool *propState = nullptr
  )
  {
    return addObjProp<T>(name, prop, [](T *val) -> bool {
      return typedInput<T>(val);
    }, propState);
  }

  inline void addColor(const std::string &name, glm::vec4 &color, bool withAlpha = true) {
    add(name);
    bool disabled (isPrefabLocked());
    if(disabled)ImGui::BeginDisabled();
    bool changed = false;
    if (withAlpha) {
      changed = ImGui::ColorEdit4(name.c_str(), glm::value_ptr(color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    } else {
      changed = ImGui::ColorEdit3(name.c_str(), glm::value_ptr(color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    }
    if(changed)Editor::UndoRedo::getHistory().markChanged("Edit " + name);
    if(disabled)ImGui::EndDisabled();
  }

  inline void addPath(const std::string &name, std::string &str, bool isDir = false, const std::string &placeholder = "") {
    add(name);
    auto labelHidden = "##" + name;
    ImGui::PushID(labelHidden.c_str());
    if (ImGui::Button(ICON_MDI_FOLDER_OUTLINE)) {
      Utils::FilePicker::open([&str](const std::string &path) {
        str = path;
      }, {.isDirectory = isDir});
    }
    ImGui::PopID();
    ImGui::SameLine();

    bool changed = false;
    if (placeholder.empty()) {
      changed = ImGui::InputText(labelHidden.c_str(), &str);
    } else {
      changed = ImGui::InputTextWithHint(labelHidden.c_str(), placeholder.c_str(), &str);
    }
    if(changed)Editor::UndoRedo::getHistory().markChanged("Edit " + name);
  }

  inline bool addKeybind(const std::string& name, ImGuiKey& key) {
    add(name);
    bool isRebinding = (ctx.rebindingKey == &key);
    const char* label = isRebinding ? "Press any key..." : ImGui::GetKeyName(key);
    ImGui::PushID(name.c_str());
    if (ImGui::Button(label, ImVec2(-FLT_MIN, 0))) {
      ctx.rebindingKey = &key;
    }

    if (!isRebinding) {
      ImGui::PopID();
      return false;
    }

    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; k++) {
      if (!ImGui::IsKeyPressed((ImGuiKey)k)) continue;

      ctx.rebindingKey = nullptr;
      if (k == ImGuiKey_Escape) {
        break;
      } else {
        key = (ImGuiKey)k;
        Editor::UndoRedo::getHistory().markChanged("Rebind " + name);
        ImGui::PopID();
        return true;
      }
    }
    
    ImGui::PopID();
    return false;
  }
}
