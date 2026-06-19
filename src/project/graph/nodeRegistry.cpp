/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "nodeRegistry.h"
#include "nodes/scriptNode.h"
#include "jsNodeHost.h"
#include "graph.h"

#include "imgui.h"
#include "IconsMaterialDesignIcons.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "../../utils/string.h"

#include <array>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>

namespace fs = std::filesystem;

namespace Project::Graph::Node
{
  namespace
  {
    // The original integer node-type order, used only to load graphs saved before
    // the switch to stable string ids. Index = legacy "type" value.
    constexpr std::array<const char*, 14> LEGACY_TYPE_IDS = {
      "core.start", "core.wait", "core.objDel", "core.objEvent", "core.compare",
      "core.value", "core.repeat", "core.func", "core.ifElse", "core.sceneLoad",
      "core.arg", "core.switchCase", "core.note", "core.objRef",
    };

    // The currently-edited graph's variable declarations
    const std::vector<::Project::Graph::GraphVar>* g_activeVars = nullptr;

    std::string varTypeByName(const std::string &name)
    {
      if(g_activeVars) for(const auto &v : *g_activeVars) if(v.name == name)return v.type;
      return "i32";
    }

    // Dropdown of the active graph's variables; on change, retypes the node's dynamic
    // value pin(s) to the chosen variable's type and refreshes the title.
    void drawVarSelect(ScriptNode &n)
    {
      std::string cur = n.getStr("var");
      ImGui::SetNextItemWidth(110.0f);
      if(ImGui::BeginCombo("##var", cur.empty() ? "<none>" : cur.c_str())) {
        if(g_activeVars) {
          for(const auto &v : *g_activeVars) {
            bool sel = (v.name == cur);
            if(ImGui::Selectable(v.name.c_str(), sel)) {
              n.setProp("var", v.name);
              applyVarPinTypes(n, v.type);
              n.refreshTitle();
            }
            if(sel) ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
    }

    // Sets a Set Var node's value input + value output types from this graph's
    // variable (used by prepareBuild, which must not touch pin styles/links).
    void prepareVarIO(ScriptNode &n, BuildCtx &ctx)
    {
      if(!ctx.varTypeOf)return;
      auto t = ctx.varTypeOf(n.getStr("var"));
      if(n.inTypes.size()  > 1) n.inTypes[1]  = t;
      if(n.outTypes.size() > 1) n.outTypes[1] = t;
    }

    void addNativeSpecs(std::vector<NodeSpec> &s)
    {
      // Variable Get / Set ------------------------------------------------------
      s.push_back(NodeSpec{
        .id = "core.varGet",
        .name = ICON_MDI_VARIABLE " Get Var",
        .category = "Variables",
        .color = IM_COL32(0x88, 0x88, 0x88, 0xFF), .rounding = 4.0f,
        .outputs = {{"", true, "i32"}},
        .value = [](ScriptNode &n, BuildCtx &ctx) -> std::string {
          return ctx.varLValue ? ctx.varLValue(n.getStr("var")) : std::string{"0"};
        },
        .title = [](ScriptNode &n){
          auto v = n.getStr("var");
          return std::string(ICON_MDI_VARIABLE " ") + (v.empty() ? "Get Var" : v);
        },
        .drawExtra = [](ScriptNode &n){ drawVarSelect(n); },
        .prepareBuild = [](ScriptNode &n, BuildCtx &ctx){
          if(ctx.varTypeOf && !n.outTypes.empty()) n.outTypes[0] = ctx.varTypeOf(n.getStr("var"));
        },
      });

      // Set Var: writes the variable (Set / Add / Subtract) and outputs the result.
      s.push_back(NodeSpec{
        .id = "core.varSet",
        .name = ICON_MDI_VARIABLE " Set Var",
        .category = "Variables",
        .color = IM_COL32(0x88, 0x88, 0x88, 0xFF), .rounding = 4.0f,
        .inputs = {{""}, {"", true, "i32"}},
        .outputs = {{""}, {"", true, "i32"}},
        .props = { PropDef{
          .key = "op", .label = "Op", .type = PropType::Enum,
          .enumOptions = { "Set", "Add", "Subtract" },
          .width = 95,
        } },
        .build = [](ScriptNode &n, BuildCtx &ctx){
          if(!ctx.varLValue)return;
          auto lv = ctx.varLValue(n.getStr("var"));
          auto val = ctx.inputExpr(0);
          switch(n.getInt("op")) {
            case 1:  ctx.incrVar(lv, val);              break; // +=
            case 2:  ctx.line(lv + " -= " + val + ";"); break; // -=
            default: ctx.setVar(lv, val);               break; // =
          }
        },
        .value = [](ScriptNode &n, BuildCtx &ctx) -> std::string {
          return ctx.varLValue ? ctx.varLValue(n.getStr("var")) : std::string{"0"};
        },
        .title = [](ScriptNode &n){
          static const char* OPS[] = {"Set", "Add", "Subtract"};
          int op = n.getInt("op"); if(op < 0 || op > 2) op = 0;
          auto v = n.getStr("var");
          return std::string(ICON_MDI_VARIABLE " ") + OPS[op] + " " + (v.empty() ? "Var" : v);
        },
        .drawExtra = [](ScriptNode &n){ drawVarSelect(n); },
        .prepareBuild = [](ScriptNode &n, BuildCtx &ctx){ prepareVarIO(n, ctx); },
      });

      // Switch-Case --------------------------------------------------------
      s.push_back(NodeSpec{
        .id = "core.switchCase",
        .name = ICON_MDI_CALL_SPLIT " Switch-Case",
        .category = "Flow",
        .color = IM_COL32(0xFF,0x99,0x55,0xFF), .rounding = 4.0f,
        .inputs = {{""}, {"", true, "i32"}},
        .build = [](ScriptNode &n, BuildCtx &ctx){
          ctx.localVar("int", "t_comp", ctx.inputExpr(0));
          auto casesIt = n.getProps().find("cases");
          ctx.line("switch(t_comp) {");
          if(casesIt != n.getProps().end() && casesIt->is_array()) {
            const auto &cases = *casesIt;
            for(size_t i = 0; i < cases.size(); ++i) {
              ctx.line("  case " + std::to_string(cases[i].get<uint32_t>()) + ":")
                .jump((uint32_t)i)
              .line("    break;");
            }
          }
          ctx.line("}");
        },
        .drawExtra = [](ScriptNode &n){
          auto &cases = n.getProps()["cases"];
          if(!cases.is_array()) cases = nlohmann::json::array();
          for(size_t i = 0; i < cases.size(); ++i) {
            uint32_t c = cases[i].get<uint32_t>();
            ImGui::SetNextItemWidth(60.0f);
            ImGui::PushID((int)i);
            if(ImGui::InputScalar("##", ImGuiDataType_U32, &c)) cases[i] = c;
            ImGui::PopID();
          }
          if(ImGui::Button("Add")) {
            cases.push_back(0u);
            n.addLogicOut();
          }
        },
        .syncPins = [](ScriptNode &n){
          auto &cases = n.getProps()["cases"];
          if(!cases.is_array())return;
          for(size_t i = 0; i < cases.size(); ++i) n.addLogicOut();
        },
      });

      // Note ---------------------------------------------------------------
      s.push_back(NodeSpec{
        .id = "core.note",
        .name = ICON_MDI_CLIPBOARD_OUTLINE " Note",
        .category = "Other",
        .color = IM_COL32(0,0,0,0x20), .rounding = 0.0f,
        .build = [](ScriptNode&, BuildCtx&){},
        .drawExtra = [](ScriptNode &n){
          auto editor = n.getHandler();
          if(!editor)return;
          auto &sizeJ = n.getProps()["size"];
          if(!sizeJ.is_array() || sizeJ.size() != 2) sizeJ = {200.0f, 100.0f};
          ImVec2 size{sizeJ[0].get<float>(), sizeJ[1].get<float>()};

          std::string text = n.getStr("text");
          ImGui::SetNextItemWidth(size.x - 10);
          if(ImGui::InputText("##note_text", &text)) n.setProp("text", text);

          float scale = editor->getGrid().scale();
          ImGui::InvisibleButton("group_resize", size);
          if(ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            size.x = std::max(50.0f, size.x + delta.x / scale);
            size.y = std::max(50.0f, size.y + delta.y / scale);
            sizeJ = {size.x, size.y};
          }
        },
      });
    }

    // Specs live behind unique_ptr so their addresses stay stable across reloads;
    // live nodes hold raw NodeSpec* into this map.
    std::unordered_map<std::string, std::unique_ptr<NodeSpec>> g_registry{};
    std::vector<const NodeSpec*> g_specList{};   // selectable specs, menu order
    std::string g_userDir{};
    std::unordered_map<std::string, std::filesystem::file_time_type> g_userMtimes{};
    bool g_loaded = false;

    std::vector<NodeSpec> buildDesired(const std::string &userDir)
    {
      std::vector<NodeSpec> s{};
      addNativeSpecs(s);
      if(Js::init()) Js::loadSpecs(s, userDir);
      return s;
    }

    // Merge specs in place (keeping pointers valid); vanished ids become placeholders.
    void ingest(std::vector<NodeSpec> &&desired)
    {
      std::unordered_set<std::string> seen{};
      for(auto &spec : desired) {
        std::string id = spec.id; // copy before the move empties spec
        seen.insert(id);
        auto it = g_registry.find(id);
        if(it == g_registry.end()) {
          g_registry[id] = std::make_unique<NodeSpec>(std::move(spec));
        } else {
          *it->second = std::move(spec); // address preserved
        }
      }
      for(auto &[id, spec] : g_registry) {
        if(seen.count(id))continue;
        spec->missing = true;
        spec->build = nullptr;
        spec->value = nullptr;
      }

      g_specList.clear();
      for(auto &[id, spec] : g_registry) {
        if(!spec->missing && !spec->hidden) g_specList.push_back(spec.get());
      }
    }

    void refreshUserMtimes()
    {
      g_userMtimes.clear();
      if(g_userDir.empty() || !fs::is_directory(g_userDir))return;
      for(auto &e : fs::directory_iterator(g_userDir)) {
        if(e.is_regular_file() && e.path().extension() == ".js") {
          std::error_code ec{};
          auto t = fs::last_write_time(e.path(), ec);
          if(!ec) g_userMtimes[e.path().string()] = t;
        }
      }
    }

    void ensureLoaded()
    {
      if(!g_loaded) reloadSpecs(g_userDir);
    }
  }

  void setActiveGraphVars(const std::vector<::Project::Graph::GraphVar>* vars)
  {
    g_activeVars = vars;
  }

  void applyVarPinTypes(ScriptNode &n, const std::string &varType)
  {
    const auto tid = n.typeId();
    if(tid == "core.varGet") {
      n.setValuePinType(false, 0, varType);
    } else if(tid == "core.varSet") {
      n.setValuePinType(true, 1, varType);  // value input (set value / delta)
      n.setValuePinType(false, 1, varType); // value output (the new value)
    }
  }

  void reloadSpecs(const std::string &userNodeDir)
  {
    g_userDir = userNodeDir;
    ingest(buildDesired(userNodeDir));
    refreshPinStyleColors(); // type colors may have changed
    refreshUserMtimes();
    g_loaded = true;
  }

  void pollUserNodeReload()
  {
    if(g_userDir.empty())return;

    std::unordered_map<std::string, fs::file_time_type> now{};
    if(fs::is_directory(g_userDir)) {
      for(auto &e : fs::directory_iterator(g_userDir)) {
        if(e.is_regular_file() && e.path().extension() == ".js") {
          std::error_code ec{};
          auto t = fs::last_write_time(e.path(), ec);
          if(!ec) now[e.path().string()] = t;
        }
      }
    }
    if(now != g_userMtimes) reloadSpecs(g_userDir);
  }

  const std::vector<const NodeSpec*>& getNodeSpecs()
  {
    ensureLoaded();
    return g_specList;
  }

  const NodeSpec* findSpec(const std::string &id)
  {
    ensureLoaded();
    auto it = g_registry.find(id);
    return it != g_registry.end() ? it->second.get() : nullptr;
  }

  const NodeSpec* findOrCreatePlaceholder(const std::string &id)
  {
    if(auto *spec = findSpec(id))return spec;
    auto ph = std::make_unique<NodeSpec>();
    ph->id = id;
    ph->name = ICON_MDI_ALERT_OUTLINE " " + id;
    ph->color = IM_COL32(0xAA, 0x33, 0x33, 0xFF);
    ph->missing = true;
    auto *raw = ph.get();
    g_registry[id] = std::move(ph);
    return raw;
  }

  const NodeSpec* findSpecByLegacyType(uint32_t type)
  {
    if(type >= LEGACY_TYPE_IDS.size())return nullptr;
    return findSpec(LEGACY_TYPE_IDS[type]);
  }
}
