/**
* @copyright 2026
* @license MIT
*
* Game logic nodes: movement, spawning, physics, and transform control.
* These fill the biggest gap in the existing node palette — the AI
* can now generate common game behaviors without requiring C++ code.
*/
#pragma once

#include "baseNode.h"
#include "../../../utils/hash.h"

namespace Project::Graph::Node
{
  // ─── MoveToward ───────────────────────────────────────────────────────────
  // Moves this object toward a target object at a given speed.
  // Stops when within `threshold` units. Runs each frame (coro_yield).

  class MoveToward : public Base
  {
    private:
      float speed{5.0f};
      float threshold{0.5f};
      std::string targetObjName{};

    public:
      constexpr static const char* NAME = ICON_MDI_ARROW_RIGHT " Move Toward";

      MoveToward()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(60,180,120,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("Arrived", PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("Moving", PIN_STYLE_LOGIC);
      }

      void draw() override {
        char buf[64]; strncpy(buf, targetObjName.c_str(), sizeof(buf)-1); buf[sizeof(buf)-1]=0;
        ImGui::SetNextItemWidth(100.f);
        if(ImGui::InputText("target", buf, sizeof(buf))) targetObjName = buf;
        ImGui::SetNextItemWidth(60.f);
        ImGui::InputFloat("speed", &speed, 0, 0, "%.1f");
        ImGui::SetNextItemWidth(60.f);
        ImGui::InputFloat("arrive", &threshold, 0, 0, "%.1f");
      }

      void serialize(nlohmann::json &j) override {
        j["targetObjName"] = targetObjName;
        j["speed"] = speed;
        j["threshold"] = threshold;
      }

      void deserialize(nlohmann::json &j) override {
        targetObjName = j.value("targetObjName", "");
        speed = j.value("speed", 5.0f);
        threshold = j.value("threshold", 0.5f);
      }

      void build(BuildCtx &ctx) override {
        uint32_t nameHash = Utils::Hash::crc32(targetObjName.c_str(), targetObjName.size());
        ctx.line("// MoveToward: \"" + targetObjName + "\"")
           .localConst("uint32_t", "target_hash", nameHash)
           .localConst("float", "mv_speed", std::to_string(speed) + "f")
           .localConst("float", "mv_threshold", std::to_string(threshold) + "f")
           .line("{")
           .line("  auto* target = inst->obj->getScene()->findObjectByHash(target_hash);")
           .line("  if(target) {")
           .line("    T3DVec3 dir;")
           .line("    t3d_vec3_diff(&dir, &target->pos, &inst->obj->pos);")
           .line("    float dist = t3d_vec3_len(&dir);")
           .line("    if(dist > mv_threshold) {")
           .line("      t3d_vec3_norm(&dir);")
           .line("      inst->obj->pos.v[0] += dir.v[0] * mv_speed * inst->obj->getScene()->getDeltaTime();")
           .line("      inst->obj->pos.v[1] += dir.v[1] * mv_speed * inst->obj->getScene()->getDeltaTime();")
           .line("      inst->obj->pos.v[2] += dir.v[2] * mv_speed * inst->obj->getScene()->getDeltaTime();");

        // jump(1) = Moving output (still in motion)
        ctx.jump(1);

        ctx.line("    } else {");

        // jump(0) = Arrived output
        ctx.jump(0);

        ctx.line("    }")
           .line("  }")
           .line("}");
      }
  };

  // ─── SetPosition ──────────────────────────────────────────────────────────

  class SetPosition : public Base
  {
    private:
      float x{0}, y{0}, z{0};

    public:
      constexpr static const char* NAME = ICON_MDI_MAP_MARKER " Set Position";

      SetPosition()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(80,140,200,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {
        ImGui::SetNextItemWidth(160.f);
        float v[3] = {x, y, z};
        if(ImGui::InputFloat3("pos", v)) { x=v[0]; y=v[1]; z=v[2]; }
      }

      void serialize(nlohmann::json &j) override {
        j["x"] = x; j["y"] = y; j["z"] = z;
      }

      void deserialize(nlohmann::json &j) override {
        x = j.value("x", 0.f); y = j.value("y", 0.f); z = j.value("z", 0.f);
      }

      void build(BuildCtx &ctx) override {
        ctx.line("inst->obj->pos = (T3DVec3){{" +
                 std::to_string(x) + "f, " +
                 std::to_string(y) + "f, " +
                 std::to_string(z) + "f}};");
      }
  };

  // ─── SetVelocity ──────────────────────────────────────────────────────────
  // Applies a velocity vector each frame. Requires user to call this
  // in a loop or from OnTick. Useful for projectiles / jumping.

  class SetVelocity : public Base
  {
    private:
      float vx{0}, vy{0}, vz{0};

    public:
      constexpr static const char* NAME = ICON_MDI_ROCKET " Set Velocity";

      SetVelocity()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(200,120,60,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {
        ImGui::SetNextItemWidth(160.f);
        float v[3] = {vx, vy, vz};
        if(ImGui::InputFloat3("vel", v)) { vx=v[0]; vy=v[1]; vz=v[2]; }
      }

      void serialize(nlohmann::json &j) override {
        j["vx"] = vx; j["vy"] = vy; j["vz"] = vz;
      }

      void deserialize(nlohmann::json &j) override {
        vx = j.value("vx", 0.f); vy = j.value("vy", 0.f); vz = j.value("vz", 0.f);
      }

      void build(BuildCtx &ctx) override {
        ctx.line("// SetVelocity: apply per-frame motion")
           .line("{")
           .line("  float dt = inst->obj->getScene()->getDeltaTime();")
           .line("  inst->obj->pos.v[0] += " + std::to_string(vx) + "f * dt;")
           .line("  inst->obj->pos.v[1] += " + std::to_string(vy) + "f * dt;")
           .line("  inst->obj->pos.v[2] += " + std::to_string(vz) + "f * dt;")
           .line("}");
      }
  };

  // ─── Spawn ────────────────────────────────────────────────────────────────
  // Instantiates a prefab at an optional offset from this object.

  class Spawn : public Base
  {
    private:
      std::string prefabName{};
      float offX{0}, offY{0}, offZ{0};

    public:
      constexpr static const char* NAME = ICON_MDI_PLUS_BOX " Spawn";

      Spawn()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(180,200,60,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {
        char buf[64]; strncpy(buf, prefabName.c_str(), sizeof(buf)-1); buf[sizeof(buf)-1]=0;
        ImGui::SetNextItemWidth(100.f);
        if(ImGui::InputText("prefab", buf, sizeof(buf))) prefabName = buf;
        ImGui::SetNextItemWidth(160.f);
        float v[3] = {offX, offY, offZ};
        if(ImGui::InputFloat3("offset", v)) { offX=v[0]; offY=v[1]; offZ=v[2]; }
      }

      void serialize(nlohmann::json &j) override {
        j["prefabName"] = prefabName;
        j["offX"] = offX; j["offY"] = offY; j["offZ"] = offZ;
      }

      void deserialize(nlohmann::json &j) override {
        prefabName = j.value("prefabName", "");
        offX = j.value("offX", 0.f); offY = j.value("offY", 0.f); offZ = j.value("offZ", 0.f);
      }

      void build(BuildCtx &ctx) override {
        uint32_t hash = Utils::Hash::crc32(prefabName.c_str(), prefabName.size());
        ctx.line("// Spawn prefab: \"" + prefabName + "\"")
           .localConst("uint32_t", "prefab_hash", hash)
           .line("{")
           .line("  P64::PrefabParams params{};")
           .line("  params.prefabHash = prefab_hash;")
           .line("  params.pos = inst->obj->pos;")
           .line("  params.pos.v[0] += " + std::to_string(offX) + "f;")
           .line("  params.pos.v[1] += " + std::to_string(offY) + "f;")
           .line("  params.pos.v[2] += " + std::to_string(offZ) + "f;")
           .line("  inst->obj->getScene()->spawnPrefab(params);")
           .line("}");
      }
  };

  // ─── GetDistance ───────────────────────────────────────────────────────────
  // Outputs the distance between this object and a target object as a value.

  class GetDistance : public Base
  {
    private:
      std::string targetObjName{};

    public:
      constexpr static const char* NAME = ICON_MDI_RULER " Get Distance";

      GetDistance()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(140,140,200,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
        addOUT<TypeValue>("Dist", PIN_STYLE_VALUE);
      }

      void draw() override {
        char buf[64]; strncpy(buf, targetObjName.c_str(), sizeof(buf)-1); buf[sizeof(buf)-1]=0;
        ImGui::SetNextItemWidth(100.f);
        if(ImGui::InputText("target", buf, sizeof(buf))) targetObjName = buf;
      }

      void serialize(nlohmann::json &j) override {
        j["targetObjName"] = targetObjName;
      }

      void deserialize(nlohmann::json &j) override {
        targetObjName = j.value("targetObjName", "");
      }

      void build(BuildCtx &ctx) override {
        uint32_t nameHash = Utils::Hash::crc32(targetObjName.c_str(), targetObjName.size());
        std::string varName = ctx.globalVar("uint16_t", 0);
        ctx.line("// GetDistance to: \"" + targetObjName + "\"")
           .localConst("uint32_t", "dist_target_hash", nameHash)
           .line("{")
           .line("  auto* target = inst->obj->getScene()->findObjectByHash(dist_target_hash);")
           .line("  if(target) {")
           .line("    T3DVec3 diff;")
           .line("    t3d_vec3_diff(&diff, &target->pos, &inst->obj->pos);")
           .line("    " + varName + " = (uint16_t)t3d_vec3_len(&diff);")
           .line("  }")
           .line("}");
      }
  };

  // ─── SetVisible ───────────────────────────────────────────────────────────

  class SetVisible : public Base
  {
    private:
      bool visible{true};

    public:
      constexpr static const char* NAME = ICON_MDI_EYE " Set Visible";

      SetVisible()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(160,160,160,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {
        ImGui::Checkbox("visible", &visible);
      }

      void serialize(nlohmann::json &j) override {
        j["visible"] = visible;
      }

      void deserialize(nlohmann::json &j) override {
        visible = j.value("visible", true);
      }

      void build(BuildCtx &ctx) override {
        ctx.line("inst->obj->setEnabled(" + std::string(visible ? "true" : "false") + ");");
      }
  };

  // ─── PlaySound ────────────────────────────────────────────────────────────

  class PlaySound : public Base
  {
    private:
      std::string soundName{};
      float volume{1.0f};

    public:
      constexpr static const char* NAME = ICON_MDI_VOLUME_HIGH " Play Sound";

      PlaySound()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(220,180,60,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {
        char buf[64]; strncpy(buf, soundName.c_str(), sizeof(buf)-1); buf[sizeof(buf)-1]=0;
        ImGui::SetNextItemWidth(100.f);
        if(ImGui::InputText("sound", buf, sizeof(buf))) soundName = buf;
        ImGui::SetNextItemWidth(60.f);
        ImGui::SliderFloat("vol", &volume, 0.0f, 1.0f);
      }

      void serialize(nlohmann::json &j) override {
        j["soundName"] = soundName;
        j["volume"] = volume;
      }

      void deserialize(nlohmann::json &j) override {
        soundName = j.value("soundName", "");
        volume = j.value("volume", 1.0f);
      }

      void build(BuildCtx &ctx) override {
        uint32_t hash = Utils::Hash::crc32(soundName.c_str(), soundName.size());
        ctx.line("// PlaySound: \"" + soundName + "\"")
           .localConst("uint32_t", "snd_hash", hash)
           .line("P64::AudioManager::play2D(snd_hash, " +
                 std::to_string(volume) + "f);");
      }
  };

  // ─── OnCollide ────────────────────────────────────────────────────────────
  // Entry point node that fires when a collision event is received.

  class OnCollide : public Base
  {
    public:
      constexpr static const char* NAME = ICON_MDI_FLASH " On Collide";

      OnCollide()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(255,120,80,255), ImColor(0,0,0,255), 3.5f));

        addOUT<TypeLogic>("Hit", PIN_STYLE_LOGIC);
        addOUT<TypeValue>("Other", PIN_STYLE_VALUE);
      }

      void draw() override {}
      void serialize(nlohmann::json &) override {}
      void deserialize(nlohmann::json &) override {}

      void build(BuildCtx &ctx) override {
        ctx.line("// OnCollide: entry point for collision events");
      }
  };

  // ─── OnTick ───────────────────────────────────────────────────────────────
  // Entry point node that fires every frame.

  class OnTick : public Base
  {
    public:
      constexpr static const char* NAME = ICON_MDI_UPDATE " On Tick";

      OnTick()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(100,200,100,255), ImColor(0,0,0,255), 3.5f));

        addOUT<TypeLogic>("", PIN_STYLE_LOGIC);
      }

      void draw() override {}
      void serialize(nlohmann::json &) override {}
      void deserialize(nlohmann::json &) override {}

      void build(BuildCtx &ctx) override {
        ctx.line("// OnTick: fires every frame");
      }
  };

  // ─── OnTimer ──────────────────────────────────────────────────────────────
  // Entry point node that fires after a delay, optionally repeating.

  class OnTimer : public Base
  {
    private:
      float interval{1.0f};
      bool repeat{false};

    public:
      constexpr static const char* NAME = ICON_MDI_TIMER " On Timer";

      OnTimer()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(200,200,100,255), ImColor(0,0,0,255), 3.5f));

        addOUT<TypeLogic>("Fire", PIN_STYLE_LOGIC);
      }

      void draw() override {
        ImGui::SetNextItemWidth(60.f);
        ImGui::InputFloat("sec", &interval, 0, 0, "%.1f");
        ImGui::Checkbox("repeat", &repeat);
      }

      void serialize(nlohmann::json &j) override {
        j["interval"] = interval;
        j["repeat"] = repeat;
      }

      void deserialize(nlohmann::json &j) override {
        interval = j.value("interval", 1.0f);
        repeat = j.value("repeat", false);
      }

      void build(BuildCtx &ctx) override {
        ctx.localConst("uint64_t", "timer_ms", (uint64_t)(interval * 1000.0f));
        if(repeat) {
          ctx.line("while(true) {")
             .line("  coro_sleep(TICKS_FROM_MS(timer_ms));");
          ctx.jump(0);
          ctx.line("}");
        } else {
          ctx.line("coro_sleep(TICKS_FROM_MS(timer_ms));");
        }
      }
  };

  // ─── Destroy ──────────────────────────────────────────────────────────────
  // Alias: same as ObjDel but with a friendlier name for vibe coding.

  class Destroy : public Base
  {
    public:
      constexpr static const char* NAME = ICON_MDI_BOMB " Destroy";

      Destroy()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(220,50,50,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeLogic>("", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_LOGIC);
      }

      void draw() override {}
      void serialize(nlohmann::json &) override {}
      void deserialize(nlohmann::json &) override {}

      void build(BuildCtx &ctx) override {
        ctx.line("inst->obj->remove();")
           .line("return;");
      }
  };

  // ─── MathOp ───────────────────────────────────────────────────────────────
  // Basic math operation node: Add, Subtract, Multiply, Divide.

  class MathOp : public Base
  {
    private:
      int op{0}; // 0=Add, 1=Sub, 2=Mul, 3=Div
      uint16_t constVal{0};
      static constexpr const char* OP_NAMES[] = {"Add +", "Sub -", "Mul *", "Div /"};
      static constexpr const char  OP_CHARS[] = {'+', '-', '*', '/'};

    public:
      constexpr static const char* NAME = ICON_MDI_CALCULATOR " Math";

      MathOp()
      {
        uuid = Utils::Hash::randomU64();
        setTitle(NAME);
        setStyle(std::make_shared<ImFlow::NodeStyle>(IM_COL32(180,140,220,255), ImColor(0,0,0,255), 3.5f));

        addIN<TypeValue>("A", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_VALUE);
        addIN<TypeValue>("B", ImFlow::ConnectionFilter::SameType(), PIN_STYLE_VALUE);
        addOUT<TypeValue>("Result", PIN_STYLE_VALUE);

        valInputTypes = {1, 1};
      }

      void draw() override {
        ImGui::SetNextItemWidth(80.f);
        ImGui::Combo("op", &op, "Add +\0Sub -\0Mul *\0Div /\0");
        ImGui::SetNextItemWidth(60.f);
        int v = constVal;
        if(ImGui::InputInt("const", &v)) constVal = (uint16_t)v;
      }

      void serialize(nlohmann::json &j) override {
        j["op"] = op;
        j["constVal"] = constVal;
      }

      void deserialize(nlohmann::json &j) override {
        op = j.value("op", 0);
        constVal = j.value("constVal", (uint16_t)0);
      }

      void build(BuildCtx &ctx) override {
        char opChar = OP_CHARS[op % 4];
        // Name the output variable after this node's UUID (value output convention)
        auto resVar = "res_" + Utils::toHex64(uuid);
        ctx.globalVar("uint16_t", resVar, 0);

        // Determine operand A (input pin 0)
        std::string opA = "0";
        if(ctx.inValUUIDs && !ctx.inValUUIDs->empty() && (*ctx.inValUUIDs)[0] != 0) {
          opA = "res_" + Utils::toHex64((*ctx.inValUUIDs)[0]);
        }

        // Determine operand B (input pin 1), fall back to constVal
        std::string opB = std::to_string(constVal);
        if(ctx.inValUUIDs && ctx.inValUUIDs->size() > 1 && (*ctx.inValUUIDs)[1] != 0) {
          opB = "res_" + Utils::toHex64((*ctx.inValUUIDs)[1]);
        }

        ctx.line(resVar + " = (uint16_t)(" + opA + " " + opChar + " " + opB + ");");
      }
  };
}
