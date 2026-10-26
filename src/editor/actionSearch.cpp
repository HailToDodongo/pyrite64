/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "actionSearch.h"
#include "actions.h"
#include "imgui/helper.h"
#include "../context.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <algorithm>
#include <sstream>

namespace
{
  constexpr const char* POPUP_ID = "##ActionSearch";

  std::string filter{};
  int selIdx{0};
  bool selChanged{false};

  std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){ return std::tolower(c); });
    return str;
  }

  bool matches(const Editor::Actions::Action &action, const std::string &filterLower)
  {
    auto text = toLower(action.name + " " + action.id);
    std::istringstream words{filterLower};
    std::string word{};
    while(words >> word) {
      if(text.find(word) == std::string::npos)return false;
    }
    return true;
  }

  int onFilterInput(ImGuiInputTextCallbackData *data)
  {
    if(data->EventKey == ImGuiKey_UpArrow)--selIdx;
    if(data->EventKey == ImGuiKey_DownArrow)++selIdx;
    selChanged = true;
    return 0;
  }

  std::vector<const Editor::Actions::Action*> getActions()
  {
    std::vector<const Editor::Actions::Action*> list{};
    auto filterLower = toLower(filter);
    for(auto *action : Editor::Actions::getActions()) 
    {
      if(action->showInSearch 
        && Editor::Actions::isAvailable(*action) 
        && matches(*action, filterLower)
      ) {
        list.push_back(action);
      }
    }
    std::sort(list.begin(), list.end(), [](auto *a, auto *b) { 
      return a->name < b->name; 
    });
    return list;
  }
}

void Editor::ActionSearch::draw()
{
  if(ImGui::IsKeyChordPressed(ctx.prefs.keymap.actionSearch)) {
    filter.clear();
    selIdx = 0;
    ImGui::OpenPopup(POPUP_ID);
  }

  auto vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos({
    vp->WorkPos.x + vp->WorkSize.x * 0.5f, 
    vp->WorkPos.y + vp->WorkSize.y * 0.2f
  }, ImGuiCond_Always, {0.5f, 0.0f});
  
  ImGui::SetNextWindowSize({420_px, 0});

  if(!ImGui::BeginPopup(POPUP_ID))return;
  auto list = getActions();

  if(ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
  }

  // filter input:
  ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
  ImGui::SetNextItemWidth(-1);

  bool submit = ImGui::InputTextWithHint("##Filter", "Search actions...", &filter,
    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory, onFilterInput);
  
  ImGui::PopStyleColor();
  if(ImGui::IsItemEdited())selIdx = 0;
  selIdx = list.empty() ? 0 : std::clamp(selIdx, 0, (int)list.size() - 1);

  std::string runId{};
  if(submit && !list.empty()) {
    runId = list[selIdx]->id;
  }

  // result list:
  float listHeight = ImGui::GetTextLineHeightWithSpacing() * std::clamp((int)list.size(), 1, 10);
  ImGui::BeginChild("##List", {0, listHeight});
  {
    if(list.empty()) {
      ImGui::TextDisabled("No actions found");
    }

    for(int i=0; i<(int)list.size(); ++i) 
    {
      ImGui::PushID(i);
      bool isSel = i == selIdx;
      if(isSel)ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));

      if(ImGui::Selectable(list[i]->name.c_str(), isSel)) {
        runId = list[i]->id;
      }

      if(isSel)ImGui::PopStyleColor();
      if(i == selIdx && selChanged)ImGui::SetScrollHereY();

      ImGui::SameLine();
      ImGui::TextDisabled("%s", list[i]->id.c_str());
      ImGui::PopID();
    }
    selChanged = false;
  }
  ImGui::EndChild();

  if(!runId.empty() || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();

  // execute selection:
  if(!runId.empty())Actions::call(runId);
}
