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
  // A single object reference ("Object" node) declared by a graph.
  // 'slot' indexes into the runtime objRefs array, 'name' is the editor label.
  struct ObjRefParam
  {
    uint16_t slot{};
    std::string name{};
  };

  class Graph
  {
    public:
      ImFlow::ImNodeFlow graph{};

      static const std::vector<std::string>& getNodeNames();
      std::shared_ptr<Node::Base> addNode(uint32_t type, const ImVec2& pos);

      // Scans a serialized graph for its "Object" nodes without building it.
      // Used by the NodeGraph component to expose object-reference slots.
      static std::vector<ObjRefParam> getObjectRefs(const std::string &jsonData);

      bool deserialize(const std::string &jsonData);
      std::string serialize();

      void build(
        Utils::BinaryFile &binFile,
        std::string &source,
        uint64_t uuid
      );
  };
}
