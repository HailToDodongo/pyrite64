/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <algorithm>
#include <cctype>
#include <cstring>
#include <utility>
#include <string>
#include <unordered_map>
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "IconsMaterialDesignIcons.h"
#include "../../project/project.h"
#include "../undoRedo.h"
#include "../keymap.h"
#include "../../utils/filePicker.h"
#include "../../utils/prop.h"
#include "theme.h"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <functional>

#include "imgui_internal.h"

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
  inline void SideBySide(auto cbA, auto cbB, float sizeOffset = 0)
  {
    BeginGroup();
    ImGui::PushMultiItemsWidths(2, CalcItemWidth() - sizeOffset);
    cbA();
    PopItemWidth(); SameLine();
    cbB();
    PopItemWidth();
    EndGroup();
  }

  inline bool CollapsingSubHeader(const char* label, ImGuiTreeNodeFlags flags = 0)
  {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 8_px);
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

  // Faint help "?" icon, drawn at the current cursor position and vertically centered
  // to the current row height.
  // When clicked, opens the docs page at `docPath` (a slug relative to the docs root, e.g. "/manual/editor/components/code")
  bool HelpIcon(const char* docPath, const char* tooltip = "Open Docs", float glyphSize = 19_px);

  inline bool IconToggle(bool &state, const char* labelOn, const char* labelOff, const ImVec2 &labelSize)
  {
    if(IconButton(
      state ? labelOn : labelOff,
      labelSize,
      state ? ImGui::Theme::getColor("iconActive", ImVec4{1,1,1,1})
            : ImGui::Theme::getColor("iconInactive", ImVec4{0.6f,0.6f,0.6f,1})
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
      if (std::cmp_equal(id, item.getId()))break;
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

  bool rotationInput(glm::quat &quat);
  void makeTabVisible(const char* tabName);
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

  inline bool start(const char *name, Project::Object *nextObj = nullptr, ImVec2 widths = {-1,-1})
  {
    obj = nullptr;
    if (!ImGui::BeginTable(name, 2))return false;
    obj = nextObj;
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, widths[0]);
    ImGui::TableSetupColumn("Input", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(1);
    ImGui::PushItemWidth(widths[1] >= 0 ? widths[1] : -FLT_MIN);
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

  /**
   * Opens a ComboBox popup on the next BeginCombo call.
   * @param label Internal ImGui label used by the ComboBox.
   */
  inline void unfoldComboBox(const char *label)
  {
    ImGuiID popupId = ImHashStr("##ComboPopup", 0, ImGui::GetID(label));
    ImGui::OpenPopupEx(popupId, ImGuiPopupFlags_None);
  }

  /**
   * Draws a search input for an opened ComboBox popup.
   * @param label ImGui label used to derive the combo-specific filter state.
   * @return Pointer to the filter string owned by this helper.
   */
  inline std::string* drawComboSearchFilter(const char *label)
  {
    // Keep one filter string per combo so multiple searchable combos do not share text
    static std::unordered_map<ImGuiID, std::string> filters;

    // Use the ImGui ID so repeated labels still map to the correct popup state
    auto *filter = &filters[ImGui::GetID(label)];

    // Start each newly opened popup with a focused empty search box
    if (ImGui::IsWindowAppearing()) {
      filter->clear();
      ImGui::SetKeyboardFocusHere();
    }

    // Match the filter input width to the combo popup content width
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##Filter", "Filter...", filter);

    // Separate the filter input from the selectable rows
    ImGui::Separator();

    return filter;
  }

  /**
   * Returns whether a character starts a searchable PascalCase word.
   * @param label Item text being searched.
   * @param labelLength Cached label length used to inspect the next character safely
   * @param index Character index to test.
   * @return True when the character should be treated as a word start.
   */
  inline bool isPascalWordStart(const char *label, size_t labelLength, size_t index)
  {
    unsigned char currentChar = label[index];

    // Separators are not words, but the first alphanumeric character after them is
    if (!std::isalnum(currentChar)) return false;
    if (index == 0) return true;

    unsigned char previousChar = label[index - 1];

    // Non-alphanumeric separators split words, and digit runs start only on the first digit
    if (!std::isalnum(previousChar)) return true;
    if (std::isdigit(currentChar)) return !std::isdigit(previousChar);
    if (std::isdigit(previousChar)) return true;

    // Uppercase letters after lowercase letters mark normal PascalCase boundaries
    if (!std::isupper(currentChar)) return false;
    if (std::islower(previousChar)) return true;

    // Treat the last capital in an acronym as a word start. For example XMLParser -> Parser
    bool nextIsLower = index + 1 < labelLength && std::islower((unsigned char)label[index + 1]);
    return std::isupper(previousChar) && nextIsLower;
  }

  /**
   * Returns whether a label word starts with a filter token, ignoring case.
   * @param label Item text being searched.
   * @param labelLength Cached label length used to avoid reading past the end.
   * @param labelStart Word start index inside label.
   * @param filter Full filter text.
   * @param filterStart Token start index inside filter.
   * @param filterEnd Token end index inside filter.
   * @return True when the token matches the start of the label word.
   */
  inline bool labelStartsWithFilterToken(
    const char *label,
    size_t labelLength,
    size_t labelStart,
    const std::string &filter,
    size_t filterStart,
    size_t filterEnd
  )
  {
    size_t tokenLength = filterEnd - filterStart;

    // The token cannot match if it would extend past the label
    if (labelStart + tokenLength > labelLength) return false;

    // Compare the token against the label word prefix without case sensitivity
    for (size_t i = 0; i < tokenLength; ++i) {
      auto labelChar = (unsigned char)label[labelStart + i];
      auto filterChar = (unsigned char)filter[filterStart + i];
      if (std::tolower(labelChar) != std::tolower(filterChar)) return false;
    }
    return true;
  }

  /**
   * Matches a spaced filter against ordered PascalCase word starts.
   * @param label Item text being searched.
   * @param filter Filter text split by spaces into word-prefix tokens.
   * @return True when every token matches a later PascalCase word start.
   */
  inline bool labelMatchesPascalWordFilter(const char *label, const std::string &filter)
  {
    size_t labelLength = std::strlen(label);
    size_t labelStart = 0;
    size_t filterStart = 0;

    while (filterStart < filter.size()) {
      // Collapse repeated spaces between filter tokens
      while (filterStart < filter.size() && std::isspace((unsigned char)filter[filterStart])) {
        ++filterStart;
      }
      if (filterStart >= filter.size()) return true;

      // Read one token from the spaced filter
      size_t filterEnd = filterStart;
      while (filterEnd < filter.size() && !std::isspace((unsigned char)filter[filterEnd])) {
        ++filterEnd;
      }

      // Each token must match the start of a later PascalCase word
      bool foundToken = false;
      for (; labelStart < labelLength; ++labelStart) {
        if (!isPascalWordStart(label, labelLength, labelStart)) continue;
        if (!labelStartsWithFilterToken(label, labelLength, labelStart, filter, filterStart, filterEnd)) continue;

        labelStart += filterEnd - filterStart;
        foundToken = true;
        break;
      }
      if (!foundToken) return false;

      filterStart = filterEnd;
    }

    return true;
  }

  /**
   * Returns whether a label matches a filter, ignoring case.
   * @param label Item text.
   * @param filter Filter text.
   * @return True when the item matches the filter.
   */
  inline bool labelMatchesFilter(const char *label, const std::string &filter)
  {
    // There is no filter --> The label matches
    if (filter.empty()) return true;

    // The filter contains spaces --> Filter PascalCase words
    if (std::any_of(filter.begin(), filter.end(), [](unsigned char c) { return std::isspace(c); })) {
      return labelMatchesPascalWordFilter(label, filter);
    }

    // std::search needs an explicit end pointer for the C-string label
    const char *labelEnd = label + std::strlen(label);

    // Compare each character case-insensitively while searching for the filter text
    return std::search(label, labelEnd, filter.begin(), filter.end(),
      [](unsigned char labelChar, unsigned char filterChar) {
        return std::tolower(labelChar) == std::tolower(filterChar);
      }
    ) != labelEnd;
  }

  /**
   * Draws a ComboBox.
   * @tparam GetLabel Callable that returns the visible label for an item index.
   * @tparam ApplySelection Callable that applies the selected item index.
   * @param label Internal ImGui label for the ComboBox.
   * @param count Number of selectable items.
   * @param current Current selected index, updated when the user selects an item.
   * @param preview Text shown while the ComboBox is collapsed.
   * @param snapshotLabel Undo history label used when a selection changes.
   * @param getLabel Label resolver for each item index.
   * @param applySelection Selection callback for each item index.
   * @param searchable Whether to allow to filter the values.
   * @return True when the selection changed during this draw.
   */
  template<typename GetLabel, typename ApplySelection>
  inline bool drawComboSelection(
    const char* label,
    int count,
    int &current,
    const char* preview,
    const std::string &snapshotLabel,
    GetLabel getLabel,
    ApplySelection applySelection,
    bool searchable = false
  ) {
    bool changed = false;

    // ComboBox is open --> Draw DropDown with the values
    if (ImGui::BeginCombo(label, preview)) {
      std::string *filter = nullptr;
      // Allow to search --> Display filter
      if (searchable) {
        filter = drawComboSearchFilter(label);
      }

      bool hasMatches = false; // Holds whether there are values to display in the DropDown

      for (int i = 0; i < count; ++i) {
        // Resolve the label
        const char *itemLabel = getLabel(i);

        // The item doesn't match the filter --> Skip it
        if (filter && !labelMatchesFilter(itemLabel, *filter)) continue;

        hasMatches = true;
        bool selected = (i == current);

        // The uses has chosen the row
        if (ImGui::Selectable(itemLabel, selected)) {
          // Is an object-backed edit --> Add to history
          if (obj) {
            Editor::UndoRedo::getHistory().markChanged(snapshotLabel);
          }
          applySelection(i);

          // Keep the local index in sync with the caller-owned value
          current = i;
          changed = true;
        }

        // Is the selected value --> Focus it for keyboard navigation
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }

      // Is searchable and there are no matches --> Display info text
      if (searchable && !hasMatches) {
        ImGui::TextDisabled("(No results)");
      }

      // End the popup drawing scope opened by BeginCombo
      ImGui::EndCombo();
    }

    // Return whether the selection changed
    return changed;
  }

  template<typename T, typename OnChange>
  inline int addVecComboBox(
    const std::string &name,
    const std::vector<T> &items,
    auto &id,
    OnChange onChange,
    bool searchable = false
  )
  {
    if(!name.empty())add(name);
    bool disabled  (isPrefabLocked());
    if(disabled)ImGui::BeginDisabled();
    int idx = 0;
    for (const auto &item : items) {
      if (std::cmp_equal(id, item.getId()))break;
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
      },
      searchable
    );
    if(disabled)ImGui::EndDisabled();
    return idx;
  }

  template<typename T>
  inline int addVecComboBox(const std::string &name, const std::vector<T> &items, auto &id, bool searchable = false)
  {
    return addVecComboBox(name, items, id, [](auto) {}, searchable);
  }

  // addVecComboBox with drag-drop support and custom validator
  // Validator signature: bool(uint64_t uuid, const char* payloadType)
  template<typename T, typename TValidator, typename OnChange>
  inline int addVecComboBoxWithDragDrop(
    const std::string& name,
    const std::vector<T>& items,
    auto& id,
    TValidator validator,
    OnChange onChange,
    bool searchable = false
  )
  {
    auto oldId = id;
    addVecComboBox(name, items, id, onChange, searchable);
    
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
    OnChange onChange,
    bool searchable = false
  )
  {
    return addVecComboBoxWithDragDrop(name, items, id,
      [assetValidator](uint64_t uuid, const char* type) {
        if (strcmp(type, "ASSET") != 0) return false;
        return assetValidator(uuid);
      },
      onChange,
      searchable
    );
  }

  // overload: accept dropped assets that are present in the combo list.
  template<typename T, typename OnChange>
  inline int addAssetVecComboBox(
    const std::string& name,
    const std::vector<T>& items,
    auto& id,
    OnChange onChange,
    bool searchable = false
  )
  {
    return addAssetVecComboBox(name, items, id,
      [&items](uint64_t uuid) {
        for (const auto &item : items) {
          if (item.getId() == uuid) return true;
        }
        return false;
      },
      onChange,
      searchable
    );
  }

  // overload without OnChange callback.
  template<typename T>
  inline int addAssetVecComboBox(
    const std::string& name,
    const std::vector<T>& items,
    auto& id,
    bool searchable = false
  )
  {
    return addAssetVecComboBox(name, items, id, [](auto){}, searchable);
  }

  // Object-only drag-drop combo box
  // Validator signature: bool(uint32_t objectUUID)
  template<typename T, typename TObjectValidator, typename OnChange>
  inline int addObjectVecComboBox(
    const std::string& name,
    const std::vector<T>& items,
    auto& id,
    TObjectValidator objectValidator,
    OnChange onChange,
    bool searchable = false
  )
  {
    return addVecComboBoxWithDragDrop(name, items, id,
      [objectValidator](uint64_t uuid, const char* type) {
        if (strcmp(type, "OBJECT") != 0) return false;
        return objectValidator(static_cast<uint32_t>(uuid));
      },
      onChange,
      searchable
    );
  }

  // overload: accept dropped objects that are present in the combo list.
  template<typename T, typename OnChange>
  inline int addObjectVecComboBox(
    const std::string& name,
    const std::vector<T>& items,
    auto& id,
    OnChange onChange,
    bool searchable = false
  )
  {
    return addObjectVecComboBox(name, items, id,
      [&items](uint32_t uuid) {
        for (const auto &item : items) {
          if (item.getId() == uuid) return true;
        }
        return false;
      },
      onChange,
      searchable
    );
  }

  // overload without OnChange callback.
  template<typename T>
  inline int addObjectVecComboBox(
    const std::string& name,
    const std::vector<T>& items,
    auto& id,
    bool searchable = false
  )
  {
    return addObjectVecComboBox(name, items, id, [](auto){}, searchable);
  }

  inline bool addComboBox(const std::string &name, int &itemCurrent, const char* const items[], int itemsCount, bool searchable = false) {
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
      [&itemCurrent](int i) { itemCurrent = i; },
      searchable
    );
    if(disabled)ImGui::EndDisabled();
    return res;
  }

  inline bool addComboBox(const std::string &name, int &itemCurrent, const std::vector<const char*> &items, bool searchable = false) {
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
      [&itemCurrent](int i) { itemCurrent = i; },
      searchable
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

  /**
   * Select that allows multiple options to be active.
   * The indices are stored in a bitmask, so up to 8 entries can be used here.
   * @param name display name
   * @param valueMask bitmask of selected options
   * @param values display names of the options, empty strings are not offered as options
   */
  void addMultiSelectMask8(
    const std::string &name,
    uint32_t &valueMask,
    const std::array<std::string, 8> &values,
    const std::string &valueEmpty = "<None>"
  );

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

  // Renders the bit-select combo on a widened 32-bit mask. Implementation in helper.cpp,
  // call the typed bitMaskCombo() wrapper below instead.
  bool bitMaskComboImpl(
    const char *label,
    uint32_t &valueMask,
    const std::vector<std::pair<int, std::string>> &bits,
    const std::string &valueEmpty
  );

  /**
   * Combo box for selecting multiple bits by name, given an explicit (bit, name) list.
   * Works for uint8_t / uint16_t / uint32_t masks. Unlike addMultiSelectMask8 this renders
   * only the combo widget (no table row or disabled handling) so it can be embedded inside
   * e.g. addObjProp's edit function.
   * @param label imgui label (use "##..." to hide it)
   * @param value bitmask of selected bits, modified in place
   * @param bits list of (bit-index, display-name) entries to offer
   * @param valueEmpty text shown when no bit is set
   * @return true if the mask changed this frame
   */
  template<typename T>
  inline bool bitMaskCombo(
    const char *label,
    T &value,
    const std::vector<std::pair<int, std::string>> &bits,
    const std::string &valueEmpty = "<None>"
  ) {
    static_assert(
      std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>,
      "bitMaskCombo only supports uint8_t / uint16_t / uint32_t"
    );
    uint32_t mask = value;
    bool changed = bitMaskComboImpl(label, mask, bits, valueEmpty);
    if (changed) value = static_cast<T>(mask);
    return changed;
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
    } else if constexpr (std::is_same_v<T, glm::vec2>) {
      return ImGui::InputFloat2("##", glm::value_ptr(*value));
    } else if constexpr (std::is_same_v<T, glm::vec3>) {
      return ImGui::InputFloat3("##", glm::value_ptr(*value));
    } else if constexpr (std::is_same_v<T, glm::vec4>) {
      return ImGui::ColorEdit4("##", glm::value_ptr(*value), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
    } else if constexpr (std::is_same_v<T, glm::quat>) {
      ImGui::rotationInput(*value);
    } else if constexpr (std::is_same_v<T, glm::ivec2>) {
      return ImGui::InputInt2("##", glm::value_ptr(*value));
    } else if constexpr (std::is_same_v<T, std::string>) {
      return ImGui::InputText("##", value);
    } else {
      static_assert(!sizeof(T*), "Unsupported type for typedInput");
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
      ImGui::PushFont(nullptr, 18.0_px);

      if(ImGui::IconButton(
        propState->value
        ? ICON_MDI_CHECKBOX_MARKED_CIRCLE
        : ICON_MDI_CHECKBOX_BLANK_CIRCLE_OUTLINE,
        {24_px,24_px},
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
      ImGui::SetItemTooltip("%s Override", isOverrideLocal ? "Disable" : "Enable");
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
      changed = ImGui::ColorEdit4(name.c_str(), glm::value_ptr(color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
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

  bool addKeybind(const std::string &name, ImGuiKeyChord &chord, ImGuiKeyChord defaultValue, bool isChord);

  inline bool addKeybind(const std::string &name, ImGuiKey &key, ImGuiKey defaultValue) {
    return addKeybind(name, (ImGuiKeyChord&)key, (ImGuiKeyChord)defaultValue, false);
  }

  inline bool addKeybind(const std::string &name, ImGuiKeyChord &chord, ImGuiKeyChord defaultValue) {
    return addKeybind(name, chord, defaultValue, true);
  }
}
