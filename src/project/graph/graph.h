/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <vector>
#include <cstdint>

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

  // A graph-level variable, stored in the runtime Instance (inst->vars + offset).
  struct GraphVar
  {
    std::string name{};
    std::string type{"i32"}; // value-type id (see valueTypes.h), incl. "objref"
  };

  // A variable placed in the per-instance blob: its byte offset and storage size.
  struct VarLayoutEntry
  {
    std::string name{};
    std::string type{};
    uint32_t offset{};
    uint32_t size{};
  };

  // Assigns each variable a 4-byte-aligned offset in declaration order.
  std::vector<VarLayoutEntry> layoutVariables(const std::vector<GraphVar> &vars);
  // Total blob size (bytes) for the given variables (end of the last entry).
  uint32_t varBlobBytes(const std::vector<GraphVar> &vars);

  class Graph
  {
    public:
      ImFlow::ImNodeFlow graph{};
      std::vector<GraphVar> variables{}; // graph-level variable declarations
      // When set, a dead-ending path restarts at Start instead of ending the coroutine.
      bool repeatable{false};

      std::shared_ptr<Node::Base> addNode(const std::string &typeId, const ImVec2& pos);

      // Read object refs / variables from a serialized graph without building it.
      static std::vector<ObjRefParam> getObjectRefs(const std::string &jsonData);
      static std::vector<GraphVar> getVariables(const std::string &jsonData);

      bool deserialize(const std::string &jsonData);
      // withView includes the canvas pan/zoom (saved to file, excluded from dirty checks).
      std::string serialize(bool withView = true);

      void build(
        Utils::BinaryFile &binFile,
        std::string &source,
        uint64_t uuid
      );
  };
}
