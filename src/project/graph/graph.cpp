/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "graph.h"

#include "json.hpp"
#include "../../utils/string.h"

#include <functional>
#include <memory>
#include <unordered_set>
#include <algorithm>

#include "nodeRegistry.h"
#include "nodes/scriptNode.h"
#include "valueTypes.h"

namespace
{
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

namespace Project::Graph::Node
{
  std::shared_ptr<ImFlow::PinStyle> PIN_STYLE_LOGIC = ImFlow::PinStyle::green();
  std::shared_ptr<ImFlow::PinStyle> PIN_STYLE_VALUE = ImFlow::PinStyle::brown();
}

namespace Project::Graph
{
  std::vector<VarLayoutEntry> layoutVariables(const std::vector<GraphVar> &vars)
  {
    std::vector<VarLayoutEntry> out{};
    uint32_t offset = 0;
    for(const auto &v : vars) {
      uint32_t size = (uint32_t)Node::byteSizeOf(v.type);
      out.push_back({v.name, v.type, offset, size});
      offset += size; // type sizes are 4-byte multiples, so offsets stay aligned
    }
    return out;
  }

  uint32_t varBlobBytes(const std::vector<GraphVar> &vars)
  {
    auto layout = layoutVariables(vars);
    return layout.empty() ? 0 : (layout.back().offset + layout.back().size);
  }

  std::vector<GraphVar> Graph::getVariables(const std::string &jsonData)
  {
    std::vector<GraphVar> out{};
    auto data = nlohmann::json::parse(jsonData, nullptr, false);
    if(!data.is_object() || !data.contains("variables"))return out;
    for(auto &v : data["variables"]) {
      out.push_back({
        v.value("name", std::string{}),
        v.value("type", std::string{"i32"}),
      });
    }
    return out;
  }

  std::vector<ObjRefParam> Graph::getObjectRefs(const std::string &jsonData)
  {
    std::vector<ObjRefParam> out{};
    auto data = nlohmann::json::parse(jsonData, nullptr, false);
    if(!data.is_object() || !data.contains("nodes"))return out;

    for(auto &node : data["nodes"]) {
      if(!node.contains("objRefSlot"))continue;
      out.push_back({
        .slot = node.value<uint16_t>("objRefSlot", 0),
        .name = node.value("objRefName", std::string{"Object"}),
      });
    }
    return out;
  }

  std::shared_ptr<Node::Base> Graph::addNode(const std::string &typeId, const ImVec2 &pos)
  {
    const auto* spec = Node::findSpec(typeId);
    assert(spec && "Unknown node typeId in graph addNode");
    if(!spec)return nullptr;
    return graph.addNode<Node::ScriptNode>(pos, spec);
  }

  bool Graph::deserialize(const std::string &jsonData)
  {
    auto nodeData = nlohmann::json::parse(jsonData);

    repeatable = nodeData.value("repeatable", false);

    if(nodeData.contains("view") && nodeData["view"].size() == 3) {
      auto &v = nodeData["view"];
      graph.setScroll({v[0].get<float>(), v[1].get<float>()});
      graph.setScale(v[2].get<float>());
    }

    variables.clear();
    if(nodeData.contains("variables")) {
      for(auto &v : nodeData["variables"]) {
        variables.push_back({v.value("name", std::string{}), v.value("type", std::string{"i32"})});
      }
    }

    std::unordered_map<uint64_t, std::shared_ptr<Node::Base>> newNodes{};
    for(auto &savedNode : nodeData["nodes"]) {
      // Prefer the stable string id; fall back to the legacy integer index.
      const Node::NodeSpec* spec = nullptr;
      if(savedNode.contains("typeId")) {
        // Unknown ids become placeholders so the node + its data are preserved.
        spec = Node::findOrCreatePlaceholder(savedNode["typeId"].get<std::string>());
      } else if(savedNode.contains("type")) {
        spec = Node::findSpecByLegacyType(savedNode["type"].get<uint32_t>());
      }
      if(!spec)continue; // legacy index out of range: nothing we can preserve

      auto newNode = graph.addNode<Node::ScriptNode>({}, spec);
      newNode->uuid = savedNode["uuid"];
      newNode->deserialize(savedNode);
      newNode->setPos({savedNode["pos"][0], savedNode["pos"][1]});
      newNodes[newNode->uuid] = newNode;
    }

    // Set var pin types before linking, so a later retype can't drop restored links.
    {
      auto varTypeOf = [this](const std::string &name) -> std::string {
        for(const auto &v : variables) if(v.name == name) return v.type;
        return "i32";
      };
      for(auto &[uuid, node] : newNodes) {
        auto *sn = static_cast<Node::ScriptNode*>(node.get());
        Node::applyVarPinTypes(*sn, varTypeOf(sn->getStr("var")));
      }
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
          // Force past the type filter to preserve a saved link.
          pinA->createLink(pinB, true);

          // Restore editable routing waypoints (the just-created link is the newest one).
          if(savedLink.contains("points")) {
            auto &links = graph.getLinks();
            if(!links.empty()) {
              if(auto lk = links.back().lock()) {
                lk->waypoints().clear();
                for(const auto &p : savedLink["points"]) {
                  lk->waypoints().push_back(ImVec2(p[0].get<float>(), p[1].get<float>()));
                }
              }
            }
          }
        }
      }
    }

    graph.getGroups().clear();
    if(nodeData.contains("groups")) {
      for(auto &jg : nodeData["groups"]) {
        ImVec2 pos(jg["pos"][0].get<float>(), jg["pos"][1].get<float>());
        ImVec2 size(jg["size"][0].get<float>(), jg["size"][1].get<float>());
        graph.addGroup(jg.value("title", std::string{}), pos, size);
      }
    }
    return true;
  }

  std::string Graph::serialize(bool withView)
  {
    nlohmann::json data{};
    data["repeatable"] = repeatable;

    if(withView) {
      auto sc = graph.getScroll();
      data["view"] = {sc.x, sc.y, graph.getScale()};
    }

    data["variables"] = nlohmann::json::array();
    for(const auto &v : variables) {
      data["variables"].push_back({{"name", v.name}, {"type", v.type}});
    }

    data["nodes"] = nlohmann::json::array();
    for (const auto& [uid, node] : graph.getNodes()) {
      auto p64Node = (Node::Base*)node.get();

      nlohmann::json jNode{};
      jNode["uuid"] = p64Node->uuid;
      jNode["typeId"] = p64Node->typeId();
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

            const auto &wps = link->waypoints();
            if(!wps.empty()) {
              auto jPts = nlohmann::json::array();
              for(const auto &w : wps) jPts.push_back({w.x, w.y});
              jLink["points"] = jPts;
            }
            data["links"].push_back(jLink);
          }
        }
      }
    }

    data["groups"] = nlohmann::json::array();
    for(const auto &g : graph.getGroups()) {
      if(g->isDestroyed())continue;
      data["groups"].push_back({
        {"title", g->getTitle()},
        {"pos",  {g->getPos().x, g->getPos().y}},
        {"size", {g->getSize().x, g->getSize().y}},
      });
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
    f.write<uint16_t>(0); // padding to 4-byte-align the variable-blob size
    f.write<uint32_t>(varBlobBytes(variables)); // bytes the runtime allocates for inst->vars


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

    // Each variable resolves to a typed lvalue into inst->vars at a baked offset.
    {
      auto byName = std::make_shared<std::unordered_map<std::string, VarLayoutEntry>>();
      for(auto &e : layoutVariables(variables)) (*byName)[e.name] = e;
      nodeCtx.varLValue = [byName](const std::string &name) -> std::string {
        auto it = byName->find(name);
        if(it == byName->end())return "0";
        return "(*(" + Node::cTypeOf(it->second.type) + "*)((uint8_t*)inst->vars + "
               + std::to_string(it->second.offset) + "))";
      };
      nodeCtx.varTypeOf = [byName](const std::string &name) -> std::string {
        auto it = byName->find(name);
        return it == byName->end() ? std::string{} : it->second.type;
      };
    }

    // convert nodes to vector, and make sure the start node (type=0) is first
    std::vector<Node::Base*> nodeVec{};
    std::unordered_map<uint64_t, Node::Base*> nodeMap{};
    nodeVec.reserve(nodes.size());
    for(const auto &node : nodes | std::views::values)
    {
      auto p64Node = (Node::Base*)node.get();
      if(p64Node->isEntry()) {
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
      // Compact to value pins only, in value-pin order (parallel to valueInputTypes()).
      std::vector<uint64_t> filteredIngoingVals{};
      for(size_t i = 0; i < p64Node->inTypes.size(); ++i)
      {
        if(p64Node->isValueInput(i) && i < ingoingVals.size()){
          filteredIngoingVals.push_back(ingoingVals[i]);
        }
      }
      ingoingVals = filteredIngoingVals;
    }

    // Per-node value-input type lists, parallel to the compacted value-link lists.
    std::unordered_map<uint64_t, std::vector<std::string>> nodeValInTypes{};
    for(auto *node : nodeVec) nodeValInTypes[node->uuid] = node->valueInputTypes();

    std::unordered_map<uint64_t, std::vector<std::string>> nodeValInFallbacks{};
    for(auto *node : nodeVec) nodeValInFallbacks[node->uuid] = node->valueInputLiterals();

    // Back-propagation: a producer returns its inline value expression (recursing into
    // its own inputs), or falls back to its persisted "res_<uuid>".
    auto visited = std::make_shared<std::unordered_set<uint64_t>>();
    std::function<std::string(uint64_t)> resolveValue =
      [&, visited](uint64_t valUuid) -> std::string
    {
      auto it = nodeMap.find(valUuid);
      if(it == nodeMap.end())return "0";
      if(!visited->insert(valUuid).second)return "0"; // cycle guard

      auto* savedIn = nodeCtx.inValUUIDs;
      auto* savedTypes = nodeCtx.inValTypes;
      auto* savedFallbacks = nodeCtx.inValFallbacks;
      nodeCtx.inValUUIDs = &nodeIngoingValMap[valUuid];
      nodeCtx.inValTypes = &nodeValInTypes[valUuid];
      nodeCtx.inValFallbacks = &nodeValInFallbacks[valUuid];
      std::string expr = it->second->value(nodeCtx);
      nodeCtx.inValUUIDs = savedIn;
      nodeCtx.inValTypes = savedTypes;
      nodeCtx.inValFallbacks = savedFallbacks;

      visited->erase(valUuid);
      return expr.empty() ? ("res_" + Utils::toHex64(valUuid)) : expr;
    };
    nodeCtx.valueResolver = resolveValue;

    // Type of a producer's back-propagated value = its first value-output type.
    nodeCtx.valueTypeResolver = [&](uint64_t valUuid) -> std::string {
      auto it = nodeMap.find(valUuid);
      return it == nodeMap.end() ? std::string{} : it->second->firstValueOutType();
    };

    // Pre-pass: let nodes resolve dynamic pin types from this graph's variables
    // (e.g. Set/Get) before any codegen/value-resolution runs.
    for(auto *node : nodeVec) node->prepareBuild(nodeCtx);

    // Repeatable: a dead-ending path yields and restarts at Start instead of ending.
    uint64_t startUuid = 0;
    for(auto *node : nodeVec) if(node->isEntry()) { startUuid = node->uuid; break; }
    nodeCtx.flowEnd = (repeatable && startUuid)
      ? ("coro_yield(); inst->time += P64::VI::SwapChain::getDeltaTime(); goto NODE_" + Utils::toHex64(startUuid) + ";")
      : std::string{"return;"};

    auto nodeLabel = [&](uint64_t uuid) {
      return "NODE_" + Utils::toHex64(uuid);
    };

    // Run codegen first (building the body into nodeCtx.source), so nodes have a chance to
    // request extra includes via ctx.include() before the include block is written.
    for(const auto &node : nodeVec)
    {
      nodeCtx.outUUIDs = &nodeOutgoingMap[node->uuid];
      nodeCtx.inValUUIDs = &nodeIngoingValMap[node->uuid];
      nodeCtx.inValTypes = &nodeValInTypes[node->uuid];
      nodeCtx.inValFallbacks = &nodeValInFallbacks[node->uuid];

      nodeCtx.source += "  " + nodeLabel(node->uuid) + ": // " + node->getName() + "\n";
      nodeCtx.source += "  {\n";

      node->build(nodeCtx);

      if(nodeCtx.outUUIDs->empty()) {
        nodeCtx.line(nodeCtx.flowEnd);
      } else {
        nodeCtx.jump(0);
      }

      nodeCtx.source += "  }\n";
    }

    static const std::vector<std::string> baseIncludes = {
      "<script/nodeGraph.h>", "<scene/object.h>", "<scene/scene.h>",
      "<vi/swapChain.h>", // Delta Time node reads the global frame delta
      "<lib/logger.h>",
    };
    for(const auto &inc : baseIncludes) source += "#include " + inc + "\n";
    // Node-requested includes (e.g. custom value-type headers), skipping the base ones.
    for(const auto &inc : nodeCtx.includes) {
      if(std::find(baseIncludes.begin(), baseIncludes.end(), inc) == baseIncludes.end())
        source += "#include " + inc + "\n";
    }
    source += "\n";

    source += "namespace P64::NodeGraph::G" + Utils::toHex64(uuid) + " {\n";
    source += R"(void run(void* arg) {)" "\n";

    source += R"(  P64::NodeGraph::Instance* inst = (P64::NodeGraph::Instance*)arg; )" "\n";

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
