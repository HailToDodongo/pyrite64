/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "notification.h"
#include "imgui.h"
#include "theme.h"
#include <vector>
#include <IconsMaterialDesignIcons.h>
#include <mutex>

namespace Editor::Noti
{
  constexpr float STAY_TIME = 3.0f;
  constexpr float FADE_TIME = 0.25f;

  struct Notification
  {
    Type type;
    std::string text;
    float timeLeft;
    float totalTime;
  };

  std::mutex notiMutex{};
  std::vector<Notification> notifications{};

  Notification action{};

  void add(Type type, const std::string &text)
  {
    std::lock_guard<std::mutex> lock{notiMutex};
    float time = type == Type::ERROR ? (STAY_TIME * 2) : STAY_TIME;
    notifications.push_back({type, text, time, time});
  }

  void showAction(const std::string &text)
  {
    action = {Type::INFO, text, 1.5f, 1.5f};
  }

  void draw()
  {
    notiMutex.lock();
    auto notiLocal = notifications;
    notiMutex.unlock();
    
    if(notiLocal.empty() && action.text.empty())return;

    ImVec2 PADDING = {16_px, 16_px};

    // bottom right window pos
    auto pos = ImGui::GetIO().DisplaySize;
    pos.x -= 20_px;
    pos.y -= 20_px;

    ImGui::PushFont(nullptr, 18_px);
    ImDrawList *drawList = ImGui::GetForegroundDrawList();

    for (auto it = notiLocal.begin(); it != notiLocal.end();) 
    {
      ImVec2 textSize = ImGui::CalcTextSize(it->text.c_str());
      textSize.x += 20_px; // add some padding for the icon
      ImVec2 boxSize = {textSize.x + PADDING.x * 2, textSize.y + PADDING.y * 2};
      ImVec2 boxPos = {pos.x - boxSize.x, pos.y - boxSize.y}; 

      // scroll in from the right
      if(it->timeLeft < (FADE_TIME)) {
        float t = 1.0f - (it->timeLeft / FADE_TIME);
        t*= t;
        boxPos.x = pos.x + (boxSize.x * t) - boxSize.x;
      }
      if(it->timeLeft > (it->totalTime-FADE_TIME)) {
        float t = ((it->timeLeft - (it->totalTime-FADE_TIME)) / FADE_TIME);
        t*= t;
        boxPos.x = pos.x + (boxSize.x * t) - boxSize.x;
      }

      ImU32 bgColor = ImGui::GetColorU32({0.2f, 0.2f, 0.2f, 1.0f});
      ImU32 txtCol = ImGui::GetColorU32({1.0f, 1.0f, 1.0f, 0.9f});

      if(it->type == Type::ERROR) {
        bgColor = ImGui::GetColorU32({0.4f, 0.2f, 0.2f, 0.9f});
      }

      drawList->AddRectFilled(boxPos, {boxPos.x + boxSize.x, boxPos.y + boxSize.y}, bgColor, 5.0f);
      drawList->AddText({boxPos.x + PADDING.x, boxPos.y + PADDING.y}, txtCol, it->text.c_str());
      // also show MDI icons for success and error
      auto iconPos = ImVec2{boxPos.x + boxSize.x - PADDING.x-14_px, boxPos.y + PADDING.y};
      if (it->type == Type::SUCCESS) {
        drawList->AddText(iconPos, ImGui::GetColorU32({0.2f, 1.0f, 0.2f, 0.9f}), ICON_MDI_CHECK_CIRCLE);
      } else if (it->type == Type::ERROR) {
        drawList->AddText(iconPos, ImGui::GetColorU32({1.0f, 0.2f, 0.2f, 0.9f}), ICON_MDI_ALERT_CIRCLE);
      }

      // update time left and remove if expired
      it->timeLeft -= ImGui::GetIO().DeltaTime;
      if (it->timeLeft <= 0) {
        it = notiLocal.erase(it);
      } else {
        ++it;
      }

      // move up the next notification
      pos.y -= boxSize.y + 10_px;
    }

    if(!action.text.empty()) {
      ImVec2 textSize = ImGui::CalcTextSize(action.text.c_str());
      ImVec2 boxSize = {textSize.x + PADDING.x * 2, textSize.y + PADDING.y * 2};
      ImVec2 boxPos = {
        (ImGui::GetIO().DisplaySize.x - boxSize.x) / 2,
        (ImGui::GetIO().DisplaySize.y - boxSize.y) / 2
      };

      float t{1.0f};
      if(action.timeLeft < FADE_TIME) {
        t = (action.timeLeft / FADE_TIME);
      }

      ImU32 bgColor = ImGui::GetColorU32({0.2f, 0.2f, 0.2f, t});
      ImU32 txtCol = ImGui::GetColorU32({1.0f, 1.0f, 1.0f, t});

      drawList->AddRectFilled(boxPos, {boxPos.x + boxSize.x, boxPos.y + boxSize.y}, bgColor, 5.0f);
      drawList->AddText({boxPos.x + PADDING.x, boxPos.y + PADDING.y}, txtCol, action.text.c_str());

      action.timeLeft -= ImGui::GetIO().DeltaTime;
      if (action.timeLeft <= 0) {
        action.text.clear();
      }
    }


    ImGui::PopFont();

    // save back the updated notifications
    notiMutex.lock();
    notifications = notiLocal;
    notiMutex.unlock();
  }
}
