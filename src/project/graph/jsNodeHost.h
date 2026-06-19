/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include <string>
#include <vector>
#include "nodes/nodeSpec.h"

// Embedded-QuickJS host that turns JS node definitions into NodeSpecs.
namespace Project::Graph::Js
{
  // Boots the JS runtime + evaluates the prelude (idempotent); false on failure.
  bool init();

  // Loads node scripts from data/nodes/builtin and the optional user dir into 'out'.
  void loadSpecs(std::vector<Node::NodeSpec> &out, const std::string &userDir = "");
}
