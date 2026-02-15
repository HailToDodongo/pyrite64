/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomment"
#include "ImNodeFlow.h"
#pragma GCC diagnostic pop

#include "../../utils/binaryFile.h"
#include "nodes/baseNode.h"

namespace Project::Graph
{
  class Graph
  {
    public:
      ImFlow::ImNodeFlow graph{};

      static const std::vector<std::string>& getNodeNames();
      std::shared_ptr<Node::Base> addNode(uint32_t type, const ImVec2& pos);

      bool deserialize(const std::string &jsonData);
      std::string serialize();

      void build(
        Utils::BinaryFile &binFile,
        std::string &source,
        uint64_t uuid
      );
  };
}
