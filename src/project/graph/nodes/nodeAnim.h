/**
* @copyright 2026
* @license MIT
*
* Animation control nodes for the Pyrite64 visual scripting system.
* Exposes animation playback, blending, and event detection
* to the node graph — critical for the vibe coding workflow.
*/
#pragma once

#include "baseNode.h"
#include "../../../utils/hash.h"

namespace Project::Graph::Node
{
  // ─── PlayAnim ─────────────────────────────────────────────────────────────
  // Starts playing a named animation on the current entity's AnimModel.
  // Supports speed multiplier and optional looping.

  class PlayAnim : public Base
  {
    private:
      std::string animName{};
      float speed{1.0f};
      bool loop{true};

    public:
      constexpr static const char* NAME = ICON_MDI_PLAY " Play Anim";

      PlayAnim()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(80,160,220,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {
        char buf[64]; strncpy(buf, animName.c_str(), sizeof(buf)-1); buf[sizeof(buf)-1]=0;
        ImGui::SetNextItemWidth(100.f);
        if(ImGui::InputText("anim", buf, sizeof(buf))) animName = buf;
        ImGui::SetNextItemWidth(60.f);
        ImGui::InputFloat("speed", &speed, 0, 0, "%.1f");
        ImGui::Checkbox("loop", &loop);
      }

      void serialize(nlohmann::json &j) override {
        j["animName"] = animName;
        j["speed"] = speed;
        j["loop"] = loop;
      }

      void deserialize(nlohmann::json &j) override {
        animName = j.value("animName", "");
        speed = j.value("speed", 1.0f);
        loop = j.value("loop", true);
      }

      void build(BuildCtx &ctx) override {
        uint32_t hash = Utils::Hash::crc32(animName.c_str(), animName.size());
        ctx.line("// PlayAnim: \"" + animName + "\"")
           .localConst("uint32_t", "anim_hash", hash)
           .line("auto* amodel = inst->obj->getComponent<P64::Component::AnimModel>();")
           .line("if(amodel) {")
           .line("  amodel->setAnim(anim_hash, " + std::to_string(speed) + "f, " + (loop ? "true" : "false") + ");")
           .line("}");
      }
  };

  // ─── StopAnim ─────────────────────────────────────────────────────────────

  class StopAnim : public Base
  {
    public:
      constexpr static const char* NAME = ICON_MDI_STOP " Stop Anim";

      StopAnim()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(220,90,80,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {}

      void serialize(nlohmann::json &) override {}
      void deserialize(nlohmann::json &) override {}

      void build(BuildCtx &ctx) override {
        ctx.line("auto* amodel = inst->obj->getComponent<P64::Component::AnimModel>();")
           .line("if(amodel) { amodel->stop(); }");
      }
  };

  // ─── SetAnimBlend ─────────────────────────────────────────────────────────
  // Blends between main and secondary animation. Factor 0.0 = main only,
  // 1.0 = secondary only. Used for walk→run transitions, hit reactions, etc.

  class SetAnimBlend : public Base
  {
    private:
      std::string blendAnimName{};
      float blendFactor{0.5f};

    public:
      constexpr static const char* NAME = ICON_MDI_TRANSFER " Blend Anim";

      SetAnimBlend()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(100,180,200,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addIN<TypeValue>("Blend", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_VALUE);
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);

        valInputTypes = {0, 1}; // index 0 = logic, index 1 = value
      }

      void draw() override {
        char buf[64]; strncpy(buf, blendAnimName.c_str(), sizeof(buf)-1); buf[sizeof(buf)-1]=0;
        ImGui::SetNextItemWidth(100.f);
        if(ImGui::InputText("blend", buf, sizeof(buf))) blendAnimName = buf;
        ImGui::SetNextItemWidth(60.f);
        ImGui::SliderFloat("factor", &blendFactor, 0.0f, 1.0f);
      }

      void serialize(nlohmann::json &j) override {
        j["blendAnimName"] = blendAnimName;
        j["blendFactor"] = blendFactor;
      }

      void deserialize(nlohmann::json &j) override {
        blendAnimName = j.value("blendAnimName", "");
        blendFactor = j.value("blendFactor", 0.5f);
      }

      void build(BuildCtx &ctx) override {
        uint32_t hash = Utils::Hash::crc32(blendAnimName.c_str(), blendAnimName.size());
        // Use connected Blend value if available, otherwise fall back to constant
        std::string blendExpr;
        if(ctx.inValUUIDs && !ctx.inValUUIDs->empty() && (*ctx.inValUUIDs)[0] != 0) {
          blendExpr = "(float)res_" + Utils::toHex64((*ctx.inValUUIDs)[0]) + " / 65535.0f";
        } else {
          blendExpr = std::to_string(blendFactor) + "f";
        }
        ctx.line("// SetAnimBlend: \"" + blendAnimName + "\"")
           .localConst("uint32_t", "blend_hash", hash)
           .line("auto* amodel = inst->obj->getComponent<P64::Component::AnimModel>();")
           .line("if(amodel) {")
           .line("  amodel->setBlendAnim(blend_hash, " + blendExpr + ");")
           .line("}");
      }
  };

  // ─── WaitAnimEnd ──────────────────────────────────────────────────────────
  // Coroutine node that suspends execution until the current animation
  // finishes playing (non-looping anims only). Crucial for sequencing.

  class WaitAnimEnd : public Base
  {
    public:
      constexpr static const char* NAME = ICON_MDI_TIMER_SAND " Wait Anim End";

      WaitAnimEnd()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(200,180,80,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("Done", PIN_STYLE_LOGIC);
      }

      void draw() override {}
      void serialize(nlohmann::json &) override {}
      void deserialize(nlohmann::json &) override {}

      void build(BuildCtx &ctx) override {
        ctx.line("// WaitAnimEnd: poll until animation completes")
           .line("{")
           .line("  auto* amodel = inst->obj->getComponent<P64::Component::AnimModel>();")
           .line("  while(amodel && !amodel->isAnimDone()) {")
           .line("    coro_yield();")
           .line("  }")
           .line("}");
      }
  };

  // ─── SetAnimSpeed ─────────────────────────────────────────────────────────

  class SetAnimSpeed : public Base
  {
    private:
      float speed{1.0f};

    public:
      constexpr static const char* NAME = ICON_MDI_SPEEDOMETER " Anim Speed";

      SetAnimSpeed()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(150,200,100,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addIN<TypeValue>("Speed", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_VALUE);
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);

        valInputTypes = {0, 1};
      }

      void draw() override {
        ImGui::SetNextItemWidth(60.f);
        ImGui::InputFloat("speed", &speed, 0, 0, "%.2f");
      }

      void serialize(nlohmann::json &j) override {
        j["speed"] = speed;
      }

      void deserialize(nlohmann::json &j) override {
        speed = j.value("speed", 1.0f);
      }

      void build(BuildCtx &ctx) override {
        // Use connected Speed value if available, otherwise fall back to constant
        std::string speedExpr;
        if(ctx.inValUUIDs && !ctx.inValUUIDs->empty() && (*ctx.inValUUIDs)[0] != 0) {
          speedExpr = "(float)res_" + Utils::toHex64((*ctx.inValUUIDs)[0]) + " / 65535.0f";
        } else {
          speedExpr = std::to_string(speed) + "f";
        }
        ctx.line("auto* amodel = inst->obj->getComponent<P64::Component::AnimModel>();")
           .line("if(amodel) { amodel->setSpeed(" + speedExpr + "); }");
      }
  };
}
