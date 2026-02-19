/**
* @copyright 2025 - Max Bebök
* @license MIT
*
* Input & State Management Nodes for the Pyrite64 Node Graph.
*
* Provides:
*  - OnButtonPress   – entry point: fires when a joypad button is pressed
*  - OnButtonHeld    – entry point: fires every tick while a button is held
*  - OnButtonRelease – entry point: fires when a button is released
*  - ReadStick       – value node: reads analog stick X/Y as float pair
*  - SetState        – set a named integer state var on this object
*  - GetState        – read a named integer state var
*  - OnStateChange   – entry point: fires when a state var changes
*  - StateMachine    – multi-output flow based on current state value
*
* N64 joypad buttons:  A, B, Z, Start, DUp, DDown, DLeft, DRight,
*                      L, R, CUp, CDown, CLeft, CRight
*
* State vars are stored as per-object uint16_t values in the coroutine's
* global variable pool — no heap allocation, fits N64 constraints.
*/
#pragma once

#include "baseNode.h"
#include "../../../utils/hash.h"

namespace Project::Graph::Node
{
  // ── Button constants ───────────────────────────────────────────────────────

  namespace {
    constexpr int BUTTON_COUNT = 14;
    constexpr const char* const BUTTON_NAMES[BUTTON_COUNT] = {
      "A", "B", "Z", "Start",
      "D-Up", "D-Down", "D-Left", "D-Right",
      "L", "R",
      "C-Up", "C-Down", "C-Left", "C-Right"
    };
    // N64 joypad bitmask values (libdragon BUTTON_* macros)
    constexpr const char* const BUTTON_MACROS[BUTTON_COUNT] = {
      "BUTTON_A", "BUTTON_B", "BUTTON_Z", "BUTTON_START",
      "BUTTON_D_UP", "BUTTON_D_DOWN", "BUTTON_D_LEFT", "BUTTON_D_RIGHT",
      "BUTTON_L", "BUTTON_R",
      "BUTTON_C_UP", "BUTTON_C_DOWN", "BUTTON_C_LEFT", "BUTTON_C_RIGHT"
    };
  }

  // ── OnButtonPress (entry point) ────────────────────────────────────────────

  class OnButtonPress : public Base
  {
    private:
      int buttonIdx{0};   // index into BUTTON_NAMES
      int port{0};         // controller port 0-3

    public:
      constexpr static const char* NAME = ICON_MDI_GAMEPAD_VARIANT_OUTLINE " On Button Press";

      OnButtonPress()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(
          IM_COL32(0xF0, 0x80, 0x30, 0xFF), ImColor(0,0,0,255), 4.0f));
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {
        ImGui::SetNextItemWidth(90.f);
        ImGui::Combo("Button", &buttonIdx, BUTTON_NAMES, BUTTON_COUNT);
        ImGui::SetNextItemWidth(40.f);
        ImGui::InputInt("Port", &port);
        port = port < 0 ? 0 : (port > 3 ? 3 : port);
      }

      void serialize(nlohmann::json &j) override {
        j["buttonIdx"] = buttonIdx;
        j["port"] = port;
      }

      void deserialize(nlohmann::json &j) override {
        buttonIdx = j.value("buttonIdx", 0);
        port = j.value("port", 0);
      }

      void build(BuildCtx &ctx) override {
        // Entry point: poll joypad for pressed (edge-triggered)
        ctx.line("joypad_poll();")
          .line("joypad_buttons_t bt = joypad_get_buttons_pressed((joypad_port_t)" + std::to_string(port) + ");")
          .line("if(!(bt.raw & " + std::string(BUTTON_MACROS[buttonIdx]) + ")) return;");
      }
  };

  // ── OnButtonHeld (entry point) ─────────────────────────────────────────────

  class OnButtonHeld : public Base
  {
    private:
      int buttonIdx{0};
      int port{0};

    public:
      constexpr static const char* NAME = ICON_MDI_GAMEPAD_VARIANT " On Button Held";

      OnButtonHeld()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(
          IM_COL32(0xF0, 0xA0, 0x30, 0xFF), ImColor(0,0,0,255), 4.0f));
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {
        ImGui::SetNextItemWidth(90.f);
        ImGui::Combo("Button", &buttonIdx, BUTTON_NAMES, BUTTON_COUNT);
        ImGui::SetNextItemWidth(40.f);
        ImGui::InputInt("Port", &port);
        port = port < 0 ? 0 : (port > 3 ? 3 : port);
      }

      void serialize(nlohmann::json &j) override {
        j["buttonIdx"] = buttonIdx;
        j["port"] = port;
      }

      void deserialize(nlohmann::json &j) override {
        buttonIdx = j.value("buttonIdx", 0);
        port = j.value("port", 0);
      }

      void build(BuildCtx &ctx) override {
        ctx.line("joypad_poll();")
          .line("joypad_buttons_t bt = joypad_get_buttons_held((joypad_port_t)" + std::to_string(port) + ");")
          .line("if(!(bt.raw & " + std::string(BUTTON_MACROS[buttonIdx]) + ")) return;");
      }
  };

  // ── OnButtonRelease (entry point) ──────────────────────────────────────────

  class OnButtonRelease : public Base
  {
    private:
      int buttonIdx{0};
      int port{0};

    public:
      constexpr static const char* NAME = ICON_MDI_GAMEPAD_VARIANT_OUTLINE " On Button Release";

      OnButtonRelease()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(
          IM_COL32(0xF0, 0x60, 0x30, 0xFF), ImColor(0,0,0,255), 4.0f));
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {
        ImGui::SetNextItemWidth(90.f);
        ImGui::Combo("Button", &buttonIdx, BUTTON_NAMES, BUTTON_COUNT);
        ImGui::SetNextItemWidth(40.f);
        ImGui::InputInt("Port", &port);
        port = port < 0 ? 0 : (port > 3 ? 3 : port);
      }

      void serialize(nlohmann::json &j) override {
        j["buttonIdx"] = buttonIdx;
        j["port"] = port;
      }

      void deserialize(nlohmann::json &j) override {
        buttonIdx = j.value("buttonIdx", 0);
        port = j.value("port", 0);
      }

      void build(BuildCtx &ctx) override {
        ctx.line("joypad_poll();")
          .line("joypad_buttons_t bt = joypad_get_buttons_released((joypad_port_t)" + std::to_string(port) + ");")
          .line("if(!(bt.raw & " + std::string(BUTTON_MACROS[buttonIdx]) + ")) return;");
      }
  };

  // ── ReadStick (value node) ─────────────────────────────────────────────────

  class ReadStick : public Base
  {
    private:
      int port{0};
      float deadzone{0.15f};

    public:
      constexpr static const char* NAME = ICON_MDI_DRAG " Read Stick";

      ReadStick()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(
          IM_COL32(0x70, 0xB0, 0xE0, 0xFF), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
        addOUT<TypeValue>("X", PIN_STYLE_VALUE);
        addOUT<TypeValue>("Y", PIN_STYLE_VALUE);
      }

      void draw() override {
        ImGui::SetNextItemWidth(40.f);
        ImGui::InputInt("Port", &port);
        port = port < 0 ? 0 : (port > 3 ? 3 : port);
        ImGui::SetNextItemWidth(60.f);
        ImGui::InputFloat("Dead", &deadzone);
      }

      void serialize(nlohmann::json &j) override {
        j["port"] = port;
        j["deadzone"] = deadzone;
      }

      void deserialize(nlohmann::json &j) override {
        port = j.value("port", 0);
        deadzone = j.value("deadzone", 0.15f);
      }

      void build(BuildCtx &ctx) override {
        auto varX = ctx.globalVar("float", 0.0f);
        auto varY = ctx.globalVar("float", 0.0f);

        ctx.line("joypad_poll();")
          .line("joypad_inputs_t in = joypad_get_inputs((joypad_port_t)" + std::to_string(port) + ");")
          .localVar("float", "sx", std::string("(float)in.stick_x / 127.0f"))
          .localVar("float", "sy", std::string("(float)in.stick_y / 127.0f"))
          .line("if(fabsf(sx) < " + std::to_string(deadzone) + "f) sx = 0.0f;")
          .line("if(fabsf(sy) < " + std::to_string(deadzone) + "f) sy = 0.0f;")
          .line(varX + " = sx;")
          .line(varY + " = sy;");
      }
  };

  // ── SetState ───────────────────────────────────────────────────────────────

  class SetState : public Base
  {
    private:
      std::string stateName{"state"};
      int stateValue{0};

    public:
      constexpr static const char* NAME = ICON_MDI_DATABASE_EDIT_OUTLINE " Set State";

      SetState()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(
          IM_COL32(0xB0, 0x60, 0xD0, 0xFF), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {
        ImGui::SetNextItemWidth(80.f);
        ImGui::InputText("Var", &stateName);
        ImGui::SetNextItemWidth(50.f);
        ImGui::InputInt("Val", &stateValue);
      }

      void serialize(nlohmann::json &j) override {
        j["stateName"] = stateName;
        j["stateValue"] = stateValue;
      }

      void deserialize(nlohmann::json &j) override {
        stateName = j.value("stateName", "state");
        stateValue = j.value("stateValue", 0);
      }

      void build(BuildCtx &ctx) override {
        std::string varName = "gv_state_" + stateName;
        ctx.globalVar("uint16_t", varName, 0);
        ctx.line(varName + " = " + std::to_string(stateValue) + ";");
      }
  };

  // ── GetState (value output) ────────────────────────────────────────────────

  class GetState : public Base
  {
    private:
      std::string stateName{"state"};

    public:
      constexpr static const char* NAME = ICON_MDI_DATABASE_SEARCH_OUTLINE " Get State";

      GetState()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(
          IM_COL32(0x80, 0x60, 0xD0, 0xFF), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
        addOUT<TypeValue>("value", PIN_STYLE_VALUE);
      }

      void draw() override {
        ImGui::SetNextItemWidth(80.f);
        ImGui::InputText("Var", &stateName);
      }

      void serialize(nlohmann::json &j) override {
        j["stateName"] = stateName;
      }

      void deserialize(nlohmann::json &j) override {
        stateName = j.value("stateName", "state");
      }

      void build(BuildCtx &ctx) override {
        std::string varName = "gv_state_" + stateName;
        ctx.globalVar("uint16_t", varName, 0);
        (void)varName; // value is read by connected nodes via the global var
      }
  };

  // ── StateMachine (multi-output flow) ───────────────────────────────────────

  class StateMachine : public Base
  {
    private:
      int stateCount{3};
      std::string stateName{"state"};

    public:
      constexpr static const char* NAME = ICON_MDI_STATE_MACHINE " State Machine";

      StateMachine()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(
          IM_COL32(0xC0, 0x70, 0xE0, 0xFF), ImColor(0,0,0,255), 4.0f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        // Default 3 state outputs
        addOUT<TypeLogic>("S0", PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("S1", PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("S2", PIN_STYLE_LOGIC);
      }

      void draw() override {
        ImGui::SetNextItemWidth(80.f);
        ImGui::InputText("Var", &stateName);
        ImGui::SetNextItemWidth(40.f);
        if(ImGui::InputInt("States", &stateCount)) {
          stateCount = stateCount < 2 ? 2 : (stateCount > 8 ? 8 : stateCount);
        }
      }

      void serialize(nlohmann::json &j) override {
        j["stateName"] = stateName;
        j["stateCount"] = stateCount;
      }

      void deserialize(nlohmann::json &j) override {
        stateName = j.value("stateName", "state");
        stateCount = j.value("stateCount", 3);
      }

      void build(BuildCtx &ctx) override {
        std::string varName = "gv_state_" + stateName;
        ctx.globalVar("uint16_t", varName, 0);

        // Generate a switch-like cascade:
        //   if(state == 0) goto S0;
        //   if(state == 1) goto S1;
        //   ...
        for(int i = 0; i < stateCount; i++) {
          ctx.line("if(" + varName + " == " + std::to_string(i) + ") {");
          ctx.jump(i);
          ctx.line("}");
        }
        // fallthrough: do nothing (stay in current state)
        ctx.line("return;");
      }
  };

}
