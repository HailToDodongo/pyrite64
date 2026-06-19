/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "scriptNode.h"

#include "../../../context.h"
#include "../../../editor/imgui/helper.h"
#include "../../../utils/hash.h"
#include "../valueTypes.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

#include <unordered_map>

namespace Project::Graph::Node
{
  namespace
  {
    // One cached pin style per value type, coloured from the type registry
    std::unordered_map<std::string, std::shared_ptr<ImFlow::PinStyle>> g_pinStyleCache{};

    std::shared_ptr<ImFlow::PinStyle> pinStyleForType(const std::string &typeId)
    {
      auto it = g_pinStyleCache.find(typeId);
      if(it != g_pinStyleCache.end())return it->second;

      auto base = PIN_STYLE_VALUE ? *PIN_STYLE_VALUE
                                  : ImFlow::PinStyle{IM_COL32(0xFF,0x99,0x55,0xFF), 0, 6.0f, 7.0f, 6.5f, 1.3f};
      base.color = colorOf(typeId);
      auto style = std::make_shared<ImFlow::PinStyle>(base);
      g_pinStyleCache[typeId] = style;
      return style;
    }

    std::string pinTypeId(const PinDef &pin)
    {
      if(!pin.value)return LOGIC_TYPE;
      return pin.valueType.empty() ? std::string{"f32"} : pin.valueType;
    }

    bool scalarInlineEditor(const std::string &typeId, PropType &editor)
    {
      std::string ct = cTypeOf(typeId);
      if(ct == "float")    { editor = PropType::F32; return true; }
      if(ct == "int32_t")  { editor = PropType::I32; return true; }
      if(ct == "uint32_t") { editor = PropType::U32; return true; }
      return false;
    }

    // Format a float as a C++ literal, e.g. 1.5 -> "1.5f", 2 -> "2.0f".
    std::string fmtFloat(float v)
    {
      std::string s = std::to_string(v);
      if(s.find('.') != std::string::npos) {
        while(s.size() > 1 && s.back() == '0') s.pop_back();
        if(s.back() == '.') s += '0';
      }
      return s + "f";
    }
  }

  void refreshPinStyleColors()
  {
    for(auto &[typeId, style] : g_pinStyleCache) {
      if(style) style->color = colorOf(typeId);
    }
  }

  ScriptNode::ScriptNode(const NodeSpec* spec)
    : spec{spec}
  {
    uuid = Utils::Hash::randomU64();
    setStyle(std::make_shared<ImFlow::NodeStyle>(spec->color, ImColor(0,0,0,255), spec->rounding));

    for(const auto &p : spec->inputs)  addInputPin(p);
    for(const auto &p : spec->outputs) addOutputPin(p);

    // Seed defaults so the prop bag is always complete for codegen/draw.
    for(const auto &p : spec->props) {
      if(p.type == PropType::String) props[p.key] = p.defStr;
      else                           props[p.key] = p.defNum;
    }

    refreshTitle();
  }

  void ScriptNode::addInputPin(const PinDef &pin)
  {
    std::string typeId = pinTypeId(pin);
    size_t idx = inTypes.size();
    inTypes.push_back(typeId);

    // dynamic/live link filter
    auto self = this;
    auto filter = [self, idx](ImFlow::Pin* out, ImFlow::Pin*) -> bool {
      std::string inType = idx < self->inTypes.size() ? self->inTypes[idx] : std::string{};
      auto outNode = static_cast<Base*>(out->getParent());
      return canConnect(outNode ? outNode->outPinType(out) : std::string{}, inType);
    };

    // A value input takes a single source, a logic input lets many nodes converge.
    if(pin.value) addIN<TypeValue>(pin.name, filter, pinStyleForType(typeId))->setSingleLink(true);
    else          addIN<TypeLogic>(pin.name, filter, PIN_STYLE_LOGIC);
  }

  void ScriptNode::setValuePinType(bool isInput, uint32_t idx, const std::string &typeId)
  {
    auto &types = isInput ? inTypes : outTypes;
    if(idx >= types.size() || types[idx] == typeId)return;
    types[idx] = typeId;

    auto &pins = isInput ? getIns() : getOuts();
    if(idx >= pins.size())return;
    auto *pin = pins[idx].get();
    pin->getStyle() = pinStyleForType(typeId);

    // Drop only the links that are no longer valid under the new type
    for(auto &weak : pin->getLinks()) {
      auto link = weak.lock();
      if(!link)continue;
      bool ok;
      if(isInput) {
        auto *src = static_cast<Base*>(link->left()->getParent());
        ok = canConnect(src ? src->outPinType(link->left()) : std::string{}, typeId);
      } else {
        auto *dst = static_cast<Base*>(link->right()->getParent());
        ok = canConnect(typeId, dst ? dst->inPinType(link->right()) : std::string{});
      }
      if(!ok) {
        (isInput ? pin : link->right())->deleteLink(link.get());
      }
    }
  }

  void ScriptNode::addOutputPin(const PinDef &pin)
  {
    std::string typeId = pinTypeId(pin);
    outTypes.push_back(typeId);
    // A logic output goes forward to a single node; a value output may fan out freely.
    if(pin.value) (void)addOUT<TypeValue>(pin.name, pinStyleForType(typeId));
    else          addOUT<TypeLogic>(pin.name, PIN_STYLE_LOGIC)->setSingleLink(true);
  }

  void ScriptNode::addLogicOut(const std::string &name) {
    outTypes.push_back(LOGIC_TYPE);
    addOUT<TypeLogic>(name, PIN_STYLE_LOGIC)->setSingleLink(true);
  }
  void ScriptNode::addValueOut(const std::string &name) {
    outTypes.push_back("f32");
    (void)addOUT<TypeValue>(name, pinStyleForType("f32"));
  }

  std::string ScriptNode::evalTitleTemplate() const
  {
    const std::string &tpl = spec->titleTemplate;
    std::string out{};
    for(size_t i = 0; i < tpl.size(); ) {
      if(tpl[i] != '{') { out.push_back(tpl[i++]); continue; }
      size_t end = tpl.find('}', i);
      if(end == std::string::npos) { out.append(tpl.substr(i)); break; }

      std::string token = tpl.substr(i + 1, end - i - 1);
      bool wantIcon = false;
      if(auto dot = token.rfind(".icon"); dot != std::string::npos && dot == token.size() - 5) {
        wantIcon = true;
        token = token.substr(0, dot);
      }

      const PropDef* prop = nullptr;
      for(const auto &p : spec->props) if(p.key == token) { prop = &p; break; }
      if(prop && prop->type == PropType::Enum) {
        int idx = std::clamp(getInt(token), 0, (int)prop->enumOptions.size() - 1);
        if(!prop->enumOptions.empty()) out += prop->enumOptions[idx];
      } else if(wantIcon) {
        // no-op: ".icon" only meaningful for enums
      } else {
        auto it = props.find(token);
        if(it != props.end()) out += it->is_string() ? it->get<std::string>() : it->dump();
      }
      i = end + 1;
    }
    return out;
  }

  void ScriptNode::refreshTitle()
  {
    if(spec->title)                     setTitle(spec->title(*this));
    else if(!spec->titleTemplate.empty()) setTitle(evalTitleTemplate());
    else                                setTitle(spec->name);
  }

  int ScriptNode::getInt(const std::string &key, int def) const
  {
    auto it = props.find(key);
    return (it != props.end() && it->is_number()) ? it->get<int>() : def;
  }

  float ScriptNode::getF32(const std::string &key, float def) const
  {
    auto it = props.find(key);
    return (it != props.end() && it->is_number()) ? it->get<float>() : def;
  }

  std::string ScriptNode::getStr(const std::string &key, const std::string &def) const
  {
    auto it = props.find(key);
    return (it != props.end() && it->is_string()) ? it->get<std::string>() : def;
  }

  bool ScriptNode::isInputConnected(uint32_t idx) const
  {
    auto &ins = const_cast<ScriptNode*>(this)->getIns();
    return idx < ins.size() && !ins[idx]->getLinks().empty();
  }

  namespace
  {
    bool drawProp(ScriptNode &node, const PropDef &p)
    {
      bool changed = false;

      // Compact height (matching the inline input rows) with the label on the left,
      // like the pin names, instead of ImGui's default trailing label.
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f));
      if(!p.label.empty()) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(p.label.c_str());
        ImGui::SameLine();
      }
      ImGui::SetNextItemWidth(p.width);

      switch(p.type)
      {
        case PropType::U16: {
          uint16_t v = node.getU16(p.key);
          changed = ImGui::InputScalar("##v", ImGuiDataType_U16, &v);
          if(changed) node.setProp(p.key, v);
        } break;
        case PropType::U32: {
          uint32_t v = node.getU32(p.key);
          changed = ImGui::InputScalar("##v", ImGuiDataType_U32, &v);
          if(changed) node.setProp(p.key, v);
        } break;
        case PropType::I32: {
          int v = node.getInt(p.key);
          changed = ImGui::InputScalar("##v", ImGuiDataType_S32, &v);
          if(changed) node.setProp(p.key, v);
        } break;
        case PropType::F32: {
          float v = node.getF32(p.key);
          changed = ImGui::InputFloat("##v", &v);
          if(changed) node.setProp(p.key, v);
        } break;
        case PropType::Bool: {
          bool v = node.getBool(p.key);
          changed = ImGui::Checkbox("##v", &v);
          if(changed) node.setProp(p.key, v ? 1 : 0);
        } break;
        case PropType::String: {
          std::string v = node.getStr(p.key);
          changed = ImGui::InputText("##v", &v);
          if(changed) node.setProp(p.key, v);
        } break;
        case PropType::Enum: {
          int v = node.getInt(p.key);
          std::vector<const char*> opts;
          opts.reserve(p.enumOptions.size());
          for(auto &o : p.enumOptions) opts.push_back(o.c_str());
          changed = ImGui::Combo("##v", &v, opts.data(), (int)opts.size());
          if(changed) node.setProp(p.key, v);
        } break;
        case PropType::SceneRef: {
          uint32_t v = node.getU32(p.key);
          ImGui::VectorComboBox("##v", ctx.project->getScenes().getEntries(), v);
          if(v != node.getU32(p.key)) { node.setProp(p.key, v); changed = true; }
        } break;
      }

      ImGui::PopStyleVar();
      return changed;
    }

    // Inline editor for a scalar input, stored under prop key "_in<valIdx>".
    bool drawInlineInput(ScriptNode &node, size_t valIdx, PropType editor, const PinDef &pin)
    {
      std::string key = "_in" + std::to_string(valIdx);
      ImGui::PushID(key.c_str());
      ImGui::SetNextItemWidth(55.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f));
      bool changed = false;

      switch(editor)
      {
        case PropType::F32: {
          float v = node.getF32(key, pin.defNum);
          changed = ImGui::InputFloat("##v", &v);
          if(changed) node.setProp(key, v);
        } break;
        case PropType::I32: {
          int v = node.getInt(key, (int)pin.defNum);
          changed = ImGui::InputScalar("##v", ImGuiDataType_S32, &v);
          if(changed) node.setProp(key, v);
        } break;
        case PropType::U32: {
          uint32_t v = (uint32_t)node.getInt(key, (int)pin.defNum);
          changed = ImGui::InputScalar("##v", ImGuiDataType_U32, &v);
          if(changed) node.setProp(key, (int)v);
        } break;
        default: break;
      }

      ImGui::PopStyleVar();
      ImGui::PopID();
      return changed;
    }
  }

  // Declared prop bound to input pin 'p' via hideIfInputConnected, or null.
  const PropDef* ScriptNode::boundPropForInput(uint32_t p) const
  {
    for(const auto &pr : spec->props) if(pr.hideIfInputConnected == (int)p) return &pr;
    return nullptr;
  }

  void ScriptNode::draw()
  {
    if(spec->missing) {
      ImGui::TextColored(ImVec4{1.0f, 0.5f, 0.5f, 1.0f}, "Unknown node type:");
      ImGui::TextUnformatted(spec->id.c_str());
      ImGui::TextDisabled("(definition not loaded; data preserved)");
      return;
    }

    bool changed = false;

    const float rowH = ImGui::GetTextLineHeight();
    size_t valIdx = 0;
    for(size_t p = 0; p < spec->inputs.size(); ++p) {
      const bool connected = isInputConnected((uint32_t)p);
      const PropDef* bound = boundPropForInput((uint32_t)p);

      if(bound) {
        if(connected) {
          ImGui::Dummy(ImVec2(bound->width, rowH));
        } else {
          ImGui::PushID(bound->key.c_str());
          changed |= drawProp(*this, *bound);
          ImGui::PopID();
        }
      } else if(isValueInput(p)) {
        PropType editor;
        if(!connected && scalarInlineEditor(inTypes[p], editor)) {
          changed |= drawInlineInput(*this, valIdx, editor, spec->inputs[p]);
        } else {
          ImGui::Dummy(ImVec2(0.0f, rowH)); // connected, or no inline editor
        }
      } else {
        ImGui::Dummy(ImVec2(0.0f, rowH)); // logic pin
      }

      if(isValueInput(p)) ++valIdx;
    }

    if(changed && (spec->title || !spec->titleTemplate.empty())) refreshTitle();
  }

  // Properties + custom UI render below the pins (flush with the node's left edge),
  // not in the body column where they'd be indented by the input labels.
  void ScriptNode::drawBottom()
  {
    if(spec->missing)return;

    bool hasProp = false;
    for(const auto &p : spec->props) if(p.hideIfInputConnected < 0) { hasProp = true; break; }
    if(!hasProp && !spec->drawExtra)return; // no extra space for prop-less nodes

    // The body group leaves a trailing item-spacing; pull up so the gap to the inputs
    // matches the spacing between input rows.
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);

    bool changed = false;
    for(const auto &p : spec->props) {
      if(p.hideIfInputConnected >= 0)continue;
      ImGui::PushID(p.key.c_str());
      changed |= drawProp(*this, p);
      ImGui::PopID();
    }

    if(spec->drawExtra) spec->drawExtra(*this);
    ImGui::Dummy(ImVec2(0.0f, 3.0f)); // bottom padding

    if(changed && (spec->title || !spec->titleTemplate.empty())) refreshTitle();
  }

  void ScriptNode::serialize(nlohmann::json &j)
  {
    for(auto &[k, v] : props.items()) {
      j[k] = v;
    }
  }

  void ScriptNode::deserialize(nlohmann::json &j)
  {
    static const std::array<std::string, 4> structural = {"uuid", "type", "typeId", "pos"};
    for(auto &[k, v] : j.items()) {
      if(std::find(structural.begin(), structural.end(), k) != structural.end())continue;
      props[k] = v;
    }

    // Backfill defaults for any declared prop missing from the saved data.
    for(const auto &p : spec->props) {
      if(props.contains(p.key))continue;
      if(p.type == PropType::String) props[p.key] = p.defStr;
      else                           props[p.key] = p.defNum;
    }

    if(spec->syncPins) spec->syncPins(*this);
    refreshTitle();
  }

  void ScriptNode::build(BuildCtx &ctx)
  {
    if(spec->build) spec->build(*this, ctx);
  }

  std::string ScriptNode::value(BuildCtx &ctx)
  {
    return spec->value ? spec->value(*this, ctx) : std::string{};
  }

  std::vector<std::string> ScriptNode::valueInputLiterals()
  {
    std::vector<std::string> out{};
    for(size_t p = 0; p < spec->inputs.size(); ++p) {
      if(!isValueInput(p))continue;

      std::string lit{};
      PropType editor;

      if(!boundPropForInput((uint32_t)p) && scalarInlineEditor(inTypes[p], editor)) {
        std::string key = "_in" + std::to_string(out.size());
        const PinDef &pin = spec->inputs[p];
        switch(editor) {
          case PropType::F32: lit = fmtFloat(getF32(key, pin.defNum)); break;
          case PropType::I32: lit = std::to_string(getInt(key, (int)pin.defNum)); break;
          case PropType::U32: lit = std::to_string((uint32_t)getInt(key, (int)pin.defNum)) + "u"; break;
          default: break;
        }
      }
      out.push_back(lit);
    }
    return out;
  }
}
