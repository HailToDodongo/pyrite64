/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "graph.h"

#include "json.hpp"
#include "../../utils/string.h"

#include "nodes/nodeWait.h"
#include "nodes/nodeObjDel.h"
#include "nodes/nodeStart.h"
#include "nodes/nodeObjEvent.h"
#include "nodes/nodeCompare.h"
#include "nodes/nodeValue.h"
#include "nodes/nodeRepeat.h"
#include "nodes/nodeFunc.h"
#include "nodes/nodeCompBool.h"
#include "nodes/nodeSceneLoad.h"
#include "nodes/nodeArg.h"
#include "nodes/nodeSwitchCase.h"
#include "nodes/nodeNote.h"

namespace
{
  typedef std::function<std::shared_ptr<Project::Graph::Node::Base>(ImFlow::ImNodeFlow &m, const ImVec2&)> NodeCreateFunc;

  struct TableEntry
  {
    NodeCreateFunc create;
    std::string (*getname)();
  };

  uint32_t getIndexLeft(ImFlow::Pin* pin)
  {
    auto leftNode = (Project::Graph::Node::Base*)pin->getParent();
    auto &leftOuts = leftNode->getOuts();
    for(size_t i = 0; i < leftOuts.size(); ++i) {
      if(leftOuts[i].get() == pin) {
        return static_cast<uint32_t>(i);
      }
    }
    return 0;
  }

  uint32_t getIndexRight(ImFlow::Pin* pin)
  {
    auto rightNode = (Project::Graph::Node::Base*)pin->getParent();
    auto &rightIns = rightNode->getIns();
    for(size_t i = 0; i < rightIns.size(); ++i) {
      if(rightIns[i].get() == pin) {
        return static_cast<uint32_t>(i);
      }
    }
    return 0;
  }
}

#define TABLE_ENTRY(name) TableEntry{ \
    [](ImFlow::ImNodeFlow &m, const ImVec2& pos) { return m.addNode<Node::name>(pos); }, \
    Node::name::getname \
  }

namespace Project::Graph::Node
{
  std::shared_ptr<ImFlow::PinStyle> PIN_STYLE_LOGIC = ImFlow::PinStyle::green();
  std::shared_ptr<ImFlow::PinStyle> PIN_STYLE_VALUE = ImFlow::PinStyle::brown();
}

namespace Project::Graph
{
  auto NODE_TABLE = std::to_array<TableEntry>({
    TABLE_ENTRY(Start),
    TABLE_ENTRY(Wait),
    TABLE_ENTRY(ObjDel),
    TABLE_ENTRY(ObjEvent),
    TABLE_ENTRY(Compare),
    TABLE_ENTRY(Value),
    TABLE_ENTRY(Repeat),
    TABLE_ENTRY(Func),
    TABLE_ENTRY(CompBool),
    TABLE_ENTRY(SceneLoad),
    TABLE_ENTRY(Arg),
    TABLE_ENTRY(SwitchCase),
    TABLE_ENTRY(Note),
  });

  const std::vector<std::string> & Graph::getNodeNames()
  {
    static std::vector<std::string> names = {};
    if(names.empty()) {
      for(const auto &entry : NODE_TABLE) {
        names.emplace_back(entry.getname());
      }
    }
    return names;
  }

  std::shared_ptr<Node::Base> Graph::addNode(uint32_t type, const ImVec2 &pos)
  {
    assert(type < NODE_TABLE.size() && "Unknown node type in graph addNode");
    auto newNode = NODE_TABLE[type].create(graph, pos);
    newNode->type = type;
    newNode->uuid = Utils::Hash::randomU64();
    return newNode;
  }

  bool Graph::deserialize(const std::string &jsonData)
  {
    auto nodeData = nlohmann::json::parse(jsonData);

    std::unordered_map<uint64_t, std::shared_ptr<Node::Base>> newNodes{};
    for(auto &savedNode : nodeData["nodes"]) {
      uint32_t type = savedNode["type"];
      assert(type < NODE_TABLE.size() && "Unknown node type in graph load");
      auto newNode = NODE_TABLE[type].create(graph, {});
      newNode->deserialize(savedNode);
      newNode->setPos({savedNode["pos"][0], savedNode["pos"][1]});
      newNode->type = type;
      newNode->uuid = savedNode["uuid"];
      newNodes[newNode->uuid] = newNode;
    }

    for(auto &savedLink : nodeData["links"]) {
      auto nodeAIt = newNodes.find(savedLink["src"]);
      auto nodeBIt = newNodes.find(savedLink["dst"]);
      if(nodeAIt != newNodes.end() && nodeBIt != newNodes.end()) {
        auto &outs = nodeAIt->second->getOuts();
        auto &ins = nodeBIt->second->getIns();
        uint32_t srcIndex = savedLink.value("srcPort", 0);
        uint32_t dstIndex = savedLink.value("dstPort", 0);

        auto pinA = srcIndex < outs.size() ? outs[ srcIndex ].get() : nullptr;
        auto pinB = dstIndex < ins.size() ? ins[ dstIndex ].get() : nullptr;
        if(pinA && pinB) {
          pinA->createLink(pinB);
        }
      }
    }
    return true;
  }

  std::string Graph::serialize()
  {
    nlohmann::json data{};
    data["nodes"] = nlohmann::json::array();
    for (const auto& [uid, node] : graph.getNodes()) {
      auto p64Node = (Node::Base*)node.get();

      nlohmann::json jNode{};
      jNode["uuid"] = p64Node->uuid;
      jNode["type"] = p64Node->type;
      jNode["pos"] = {p64Node->getPos().x, p64Node->getPos().y};
      p64Node->serialize(jNode);
      data["nodes"].push_back(jNode);
    }

    data["links"] = nlohmann::json::array();
    auto &links = graph.getLinks();
    for (const auto& weakLink : links) {
      if (auto link = weakLink.lock()) {
        auto leftPin = link->left();
        auto rightPin = link->right();

        if (leftPin && rightPin) {
          auto leftNode = leftPin->getParent();
          auto rightNode = rightPin->getParent();
          if(leftNode && rightNode) {

            uint32_t leftIndex = getIndexLeft(leftPin);
            uint32_t rightIndex = getIndexRight(rightPin);

            /*printf("Node Link: %s:%s:%d -> %s:%s:%d\n",
              leftNode->getName().c_str(), leftPin->getName().c_str(), leftIndex,
              rightNode->getName().c_str(), rightPin->getName().c_str(), rightIndex
            );*/
            nlohmann::json jLink{};
            jLink["src"] = ((Node::Base*)leftNode)->uuid;
            jLink["srcPort"] = leftIndex;
            jLink["dst"] = ((Node::Base*)rightNode)->uuid;
            jLink["dstPort"] = rightIndex;
            data["links"].push_back(jLink);
          }
        }
      }
    }

    return data.dump(2);
  }

  void Graph::build(
    Utils::BinaryFile &f,
    std::string &source,
    uint64_t uuid
  )
  {
    auto &nodes = graph.getNodes();

    uint16_t stackSize = 4096;
    f.write<uint64_t>(uuid);
    f.write<uint16_t>(stackSize);


    // maps a node's UUID to its own position in the file
    std::unordered_map<uint64_t, uint32_t> nodeSelfPosMap{};
    // map of nodes and their outgoing links to other nodes
    std::unordered_map<uint64_t, std::vector<uint64_t>> nodeOutgoingMap{};
    std::unordered_map<uint64_t, std::vector<uint64_t>> nodeIngoingValMap{};

    // collect all active links
    for (const auto& weakLink : graph.getLinks())
    {
      if (auto link = weakLink.lock()) {
        auto leftPin = link->left();
        auto rightPin = link->right();
        if (leftPin && rightPin) {
          auto leftNode = (Node::Base*)leftPin->getParent();
          auto rightNode = (Node::Base*)rightPin->getParent();

          uint32_t leftIndex = getIndexLeft(leftPin);
          uint32_t rightIndex = getIndexRight(rightPin);

          /*printf("Link: %016llX @ %d %s:%s -> %016llX @ %d %s:%s\n",
            leftNode->uuid, leftIndex,
            leftNode->getName().c_str(), leftPin->getName().c_str(),
            rightNode->uuid, rightIndex,
            rightNode->getName().c_str(), rightPin->getName().c_str()
          );*/

          auto &e = nodeOutgoingMap[leftNode->uuid];
          if(leftIndex >= e.size()) {
            e.resize(leftIndex + 1, 0);
          }
          e[leftIndex] = rightNode->uuid;

          // for value nodes, also track ingoing connections
          auto &ev = nodeIngoingValMap[rightNode->uuid];
          if(rightIndex >= ev.size()) {
            ev.resize(rightIndex + 1, 0);
          }
          ev[rightIndex] = leftNode->uuid;
        }
      }
    }

    BuildCtx nodeCtx{};
    nodeCtx.source = "";

    // convert nodes to vector, and make sure the start node (type=0) is first
    std::vector<Node::Base*> nodeVec{};
    std::unordered_map<uint64_t, Node::Base*> nodeMap{};
    nodeVec.reserve(nodes.size());
    for(const auto &node : nodes | std::views::values)
    {
      auto p64Node = (Node::Base*)node.get();
      if(p64Node->type == 0) {
        nodeVec.insert(nodeVec.begin(), (Node::Base*)node.get());
      } else {
        nodeVec.push_back((Node::Base*)node.get());
      }
      nodeMap[p64Node->uuid] = p64Node;
    }


    for(auto &[nodeUUID, ingoingVals] : nodeIngoingValMap)
    {
      if(ingoingVals.empty())continue;

      auto p64Node = nodeMap.at(nodeUUID);
      // only keep indices where type is 1 in p64Node->valInputTypes
      std::vector<uint64_t> filteredIngoingVals{};
      for(size_t i = 0; i < p64Node->valInputTypes.size(); ++i)
      {
        if(p64Node->valInputTypes[i] == 1 && i < ingoingVals.size()){
          filteredIngoingVals.push_back(ingoingVals[i]);
        }
      }
      ingoingVals = filteredIngoingVals;
    }

    source += R"(#include <script/nodeGraph.h>)" "\n";
    source += R"(#include <scene/object.h>)" "\n";
    source += R"(#include <scene/scene.h>)" "\n";
    source += "\n";

    source += "namespace P64::NodeGraph::G" + Utils::toHex64(uuid) + " {\n";
    source += R"(void run(void* arg) {)" "\n";

    source += R"(  P64::NodeGraph::Instance* inst = (P64::NodeGraph::Instance*)arg; )" "\n";

    auto nodeLabel = [&](uint64_t uuid) {
      return "NODE_" + Utils::toHex64(uuid);
    };

    for(const auto &node : nodeVec)
    {
      nodeCtx.outUUIDs = &nodeOutgoingMap[node->uuid];
      nodeCtx.inValUUIDs = &nodeIngoingValMap[node->uuid];

      nodeCtx.source += "  " + nodeLabel(node->uuid) + ": // " + node->getName() + "\n";
      nodeCtx.source += "  {\n";

      node->build(nodeCtx);

      if(nodeCtx.outUUIDs->empty()) {
        nodeCtx.line("return;");
      } else {
        nodeCtx.jump(0);
      }

      nodeCtx.source += "  }\n";
    }

    source += "\n// ==== GLOBAL VARS ==== //\n";
    for(auto &globalVar : nodeCtx.vars) {
      source += "  " + globalVar.type + " " + globalVar.name + " = " + globalVar.value + ";\n";
    }

    source += "\n// ==== CODE ==== //\n";
    source += nodeCtx.source;
    source += "}\n";
    source += "}\n";

  }
}
