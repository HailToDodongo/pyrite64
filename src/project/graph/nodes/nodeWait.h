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
  class Wait : public Base
  {
    private:
      float time{};

    public:
      constexpr static const char* ICON = ICON_MDI_CLOCK_OUTLINE;
      constexpr static const char* NAME = MSG_GRAPH_NODE_WAIT;

      static std::string getname() {
        return std::string{ICON} + Editor::message(NAME);
      }

      Wait()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(getname());
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(90,191,93,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {
        ImGui::SetNextItemWidth(50.f);
        ImGui::InputFloat(Editor::message(MSG_GRAPH_NODE_WAIT_SEC), &time);

        //showIN("", 0, ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
      }

      void serialize(nlohmann::json &j) override {
        j["time"] = time;
      }

      void deserialize(nlohmann::json &j) override {
        time = j.value("time", 0.0f);
      }

      void build(BuildCtx &ctx) override {
        ctx.localConst("uint64_t", "t_time", (uint64_t)(time * 1000.0f))
          .line("coro_sleep(TICKS_FROM_MS(t_time));");
      }
  };
}