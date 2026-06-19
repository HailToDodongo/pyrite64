/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include <string>
#include <vector>
#include <functional>
#include "imgui.h"
#include "baseNode.h"

namespace Project::Graph::Node
{
  class ScriptNode;

  enum class PropType
  {
    U16, U32, I32, F32, Bool, String, Enum, SceneRef
  };

  // An editable property on a node, stored in the JSON prop bag under 'key'.
  struct PropDef
  {
    std::string key{};
    std::string label{};            // shown next to the widget; empty hides the label
    PropType type{PropType::I32};
    float defNum{0.0f};             // default for numeric/bool/enum props
    std::string defStr{};           // default for string props
    std::vector<std::string> enumOptions{}; // for PropType::Enum
    float width{80.0f};
    // When >= 0, the widget hides while the input pin at this index is connected.
    int hideIfInputConnected{-1};
    // Render at text-line height so the widget lines up with its paired input pin.
    bool compact{false};
  };

  // A logic- or value-pin on a node.
  struct PinDef
  {
    std::string name{};
    bool value{false};       // true = value pin, false = logic pin (green)
    std::string valueType{}; // for value pins: the data type id (see valueTypes.h)
    float defNum{0.0f};      // default if nothing is connected
  };

  // Fully describes a node type (native specs from nodeRegistry.cpp, others from JS).
  struct NodeSpec
  {
    std::string id{};        // stable identifier, e.g. "core.wait"
    std::string name{};      // menu/title label (may embed an icon glyph)
    std::string category{};  // create-menu group; empty = top level
    ImU32 color{IM_COL32(90,191,93,255)};
    float rounding{3.5f};
    bool entry{false};       // start node, emitted first in the graph
    // Placeholder for an unknown/removed node type; keeps its data instead of dropping it.
    bool missing{false};
    // Hidden from the create menu (still loadable), e.g. deprecated nodes.
    bool hidden{false};

    std::vector<PinDef> inputs{};
    std::vector<PinDef> outputs{};
    std::vector<PropDef> props{};

    // Title template with "{key}" / "{key.icon}" placeholders. Empty -> use 'name'.
    std::string titleTemplate{};

    // Emits the node's logic-flow statements (and value side-effects).
    std::function<void(ScriptNode&, BuildCtx&)> build{};
    // Returns a C++ expression for this node's value output (pull-based).
    // Set on pure/combinational nodes; leave empty for logic-only nodes.
    std::function<std::string(ScriptNode&, BuildCtx&)> value{};

    // Optional hooks for nodes that aren't purely declarative:
    std::function<std::string(ScriptNode&)> title{};   // dynamic title from props
    std::function<void(ScriptNode&)> drawExtra{};      // custom ImGui after props
    std::function<void(ScriptNode&)> syncPins{};       // rebuild dynamic pins on load
    // Runs before codegen so a node can resolve dynamic pin types (e.g. Set/Get Var).
    std::function<void(ScriptNode&, BuildCtx&)> prepareBuild{};
  };
}
