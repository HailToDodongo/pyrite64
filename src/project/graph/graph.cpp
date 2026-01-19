/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "graph.h"

#include "json.hpp"

#include "nodes/nodeWait.h"

typedef std::function<std::shared_ptr<Project::Graph::Node::Base>(ImFlow::ImNodeFlow &m, const ImVec2&)> NodeCreateFunc;
#define TABLE_ENTRY(name) [](ImFlow::ImNodeFlow &m, const ImVec2& pos) { return m.addNode<Node::name>(pos); }

namespace Project::Graph
{
  auto NODE_TABLE = std::to_array<NodeCreateFunc>({
    TABLE_ENTRY(Wait),
  });

  bool Graph::deserialize(const std::string &jsonData)
  {
    auto nodeData = nlohmann::json::parse(jsonData);

    std::unordered_map<uint64_t, std::shared_ptr<Node::Base>> newNodes{};
    for(auto &savedNode : nodeData["nodes"]) {
      uint32_t type = savedNode["type"];
      assert(type < NODE_TABLE.size() && "Unknown node type in graph load");
      auto newNode = NODE_TABLE[type](graph, {});
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
        auto pinA = nodeAIt->second->getOuts()[ savedLink["srcPort"] ].get();
        auto pinB = nodeBIt->second->getIns()[ savedLink["dstPort"] ].get();
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
    for (const auto& weakLink : graph.getLinks()) {
      if (auto link = weakLink.lock()) {
        auto leftPin = link->left();
        auto rightPin = link->right();
        if (leftPin && rightPin) {
          auto leftNode = leftPin->getParent();
          auto rightNode = rightPin->getParent();
          if(leftNode && rightNode) {
            nlohmann::json jLink{};
            jLink["src"] = ((Node::Base*)leftNode)->uuid;
            jLink["srcPort"] = 0; // TODO
            jLink["dst"] = ((Node::Base*)rightNode)->uuid;
            jLink["dstPort"] = 0; // TODO
            data["links"].push_back(jLink);
          }
        }
      }
    }

    return data.dump(2);
  }

  Utils::BinaryFile Graph::build()
  {
    auto &nodes = graph.getNodes();

    Utils::BinaryFile f{};
    f.write<uint16_t>(nodes.size());

    // maps a node's UUID to its own position in the file
    std::unordered_map<uint64_t, uint32_t> nodeSelfPosMap{};
    // map of nodes and their outgoing links to other nodes
    std::unordered_map<uint64_t, std::vector<uint64_t>> nodeOutgoingMap{};

    // collect all active links
    for (const auto& weakLink : graph.getLinks())
    {
      if (auto link = weakLink.lock()) {
        auto leftPin = link->left();
        auto rightPin = link->right();
        if (leftPin && rightPin) {
          auto leftNode = (Node::Base*)leftPin->getParent();
          auto rightNode = (Node::Base*)rightPin->getParent();
          if(leftNode && rightNode) {
            nodeOutgoingMap[leftNode->uuid].push_back(rightNode->uuid);
          }
        }
      }
    }

    // write out node data itself
    for(const auto &node : nodes | std::views::values)
    {
      auto p64Node = (Node::Base*)node.get();
      auto outSize = nodeOutgoingMap[p64Node->uuid].size();

      f.align(2);
      nodeSelfPosMap[p64Node->uuid] = f.getPos();
      f.write<uint8_t>(p64Node->type);
      f.write<uint8_t>(outSize);

      for(auto i=0; i < outSize; ++i) {
        f.write<uint16_t>(0xFFFF); // next node(s), patched later
      }

      p64Node->build(f);
    }

    // now patch in the outgoing links
    for(const auto &node : nodes | std::views::values)
    {
      auto p64Node = (Node::Base*)node.get();

      auto posSelf = nodeSelfPosMap[p64Node->uuid];
      f.setPos(posSelf + 2); // after type and out count
      auto &outgoing = nodeOutgoingMap[p64Node->uuid];
      for(size_t i = 0; i < outgoing.size(); ++i)
      {
        int32_t relPos = nodeSelfPosMap[outgoing[i]] - posSelf;
        f.write<int16_t>(relPos);
      }
    }

    return f;
  }
}
