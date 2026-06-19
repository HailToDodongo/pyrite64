/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include <vector>
#include <string>
#include "nodes/nodeSpec.h"

namespace Project::Graph { struct GraphVar; }

namespace Project::Graph::Node
{
  // Points the variable Set/Get nodes at the currently-edited graph's variable
  // declarations (for their dropdowns + live pin types). 
  // Editor-only: set per frame before drawing the graph. Pass nullptr to clear.
  void setActiveGraphVars(const std::vector<::Project::Graph::GraphVar>* vars);

  // Retypes a variable node's dynamic value pin(s) to 'varType' (no-op for other nodes):
  // Get Var's output, and Set Var's value input + value output.
  void applyVarPinTypes(ScriptNode &n, const std::string &varType);

  // (Re)builds the registry: native specs + JS builtins + the given project nodes
  void reloadSpecs(const std::string &userNodeDir = "");

  void pollUserNodeReload();

  // All selectable specs (placeholders for missing types excluded), for the menu.
  const std::vector<const NodeSpec*>& getNodeSpecs();

  // Lookup by stable id; nullptr if unknown.
  const NodeSpec* findSpec(const std::string &id);

  // Like findSpec, but synthesizes (and stores) a placeholder spec for an unknown
  // id so the node and its data are preserved rather than dropped.
  const NodeSpec* findOrCreatePlaceholder(const std::string &id);

  // Lookup by the legacy integer 'type' index used in older saved graphs.
  const NodeSpec* findSpecByLegacyType(uint32_t type);
}
