/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "scene.h"
#include "object.h"
#include <functional>
#include "../../utils/json.h"
#include "../../context.h"
#include "../../utils/hash.h"
#include "../../utils/jsonBuilder.h"
#include "../../utils/logger.h"

#define __LIBDRAGON_N64SYS_H 1
#define PhysicalAddr(a) (uint64_t)(a)
#include "../graph/nodes/baseNode.h"
#include "include/rdpq_macros.h"
#include "include/rdpq_mode.h"

namespace
{
  constexpr float DEF_MODEL_SCALE = 1.0f;

  /**
   * Checks whether a target object belongs to the subtree of a given ancestor.
   *
   * @param allegedDescendant Object to check if is a descendant.
   * @param allegedAncestorUUID UUID of the node that may be an ancestor of target.
   * @return True when target is the ancestor itself or one of its descendants.
   */
  bool isDescendantOf(const Project::Object* allegedDescendant, uint32_t allegedAncestorUUID)
  {
    const Project::Object* current = allegedDescendant;

    // Walk from the target up to the root looking for the ancestor UUID
    while (current) {
      // Found the ancestor in the parent chain
      if (current->uuid == allegedAncestorUUID)
        return true;
      // Jump up to the parent
      current = current->parent;
    }
    // Reached the root without finding the ancestor
    return false;
  }
}

nlohmann::json Project::SceneConf::serialize() const {

  auto writeLayer = [](Utils::JSON::Builder &b, const LayerConf &layer) {
    b.set(layer.name);
    b.set(layer.depthCompare);
    b.set(layer.depthWrite);
    b.set(layer.blender);
    b.set(layer.fog);
    b.set(layer.fogColorMode);
    b.set(layer.fogColor);
    b.set(layer.fogMin);
    b.set(layer.fogMax);
    b.set(layer.lightMode);
  };

  Utils::JSON::Builder builder{};
  builder.set(name)
    .set("fbWidth", fbWidth)
    .set("fbHeight", fbHeight)
    .set("fbFormat", fbFormat)
    .set(clearColor)
    .set(doClearColor)
    .set(doClearDepth)
    .set(renderPipeline)
    .set(frameLimit)
    .set(filter)
    .set(audioFreq)
    .set(physicsTickRate)
    .set(gravity)
    .set(visualUnitsPerMeter)
    .set(velocitySolverIterations)
    .set(positionSolverIterations)
    .set(interpolatePhysicsTransforms)
    .setArray<LayerConf>("layers3D", layers3D, writeLayer)
    .setArray<LayerConf>("layersPtx", layersPtx, writeLayer)
    .setArray<LayerConf>("layers2D", layers2D, writeLayer);

  return builder.doc;
}

Project::Scene::Scene(int id_, const std::string &projectPath)
  : id{id_}
{
  Utils::Logger::log("Loading scene: " + std::to_string(id));
  scenePath = projectPath + "/data/scenes/" + std::to_string(id);

  deserialize(Utils::FS::loadTextFile(scenePath + "/scene.json"));

  root.runtimeId = 0;
  root.name = "Scene";
  root.uuid = Utils::Hash::sha256_64bit(root.name);
}

std::shared_ptr<Project::Object> Project::Scene::addObject(std::string &objJson, uint64_t parentUUID)
{
  auto p = getObjectByUUID(parentUUID);
  Object *parent = p ? p.get() : &root;

  auto json = nlohmann::json::parse(objJson, nullptr, false);
  auto obj = std::make_shared<Object>(*parent);
  obj->deserialize(this, json);
  return addObject(*parent, obj, true);
}

std::shared_ptr<Project::Object> Project::Scene::addObject(Object &parent) {
  auto child = std::make_shared<Object>(parent);
  child->name = "New Object";
  child->scale.value = {DEF_MODEL_SCALE, DEF_MODEL_SCALE, DEF_MODEL_SCALE};
  child->rot.value = {0,0,0,1};
  return addObject(parent, child, true);
}

std::shared_ptr<Project::Object> Project::Scene::addObject(Object&parent, std::shared_ptr<Object> obj, bool generateUUID) {
  parent.children.push_back(obj);

  auto setChildUUIDs = [this, generateUUID](const std::shared_ptr<Object> &objChild, auto& setChildUIDsRef) -> void
  {
    if(generateUUID)
    {
      objChild->uuid = Utils::Hash::randomU64();
    }

    objectsMap[objChild->uuid] = objChild;
    for(const auto& child : objChild->children) {
      setChildUIDsRef(child, setChildUIDsRef);
    }
  };

  setChildUUIDs(obj, setChildUUIDs);
  return obj;
}

std::shared_ptr<Project::Object> Project::Scene::addPrefabInstance(uint64_t prefabUUID)
{
  auto prefab = ctx.project->getAssets().getPrefabByUUID(prefabUUID);
  if (!prefab)return nullptr;

  auto obj = std::make_shared<Object>(root);
  obj->name += prefab->obj.name;
  obj->uuid = Utils::Hash::randomU32();
  obj->pos = prefab->obj.pos;
  obj->rot = prefab->obj.rot;
  obj->scale = prefab->obj.scale;

  obj->uuidPrefab.value = prefab->uuid.value; // Link to prefab
  obj->addPropOverride(obj->pos); // by default allow transforming the instance
  obj->addPropOverride(obj->rot);
  obj->addPropOverride(obj->scale);

  return addObject(root, obj);
}

void Project::Scene::removeObject(Object &obj) {
  ctx.removeObjectSelection(obj.uuid);

  std::erase_if(
    obj.parent->children,
    [&obj](const std::shared_ptr<Object> &ref) { return ref->uuid == obj.uuid; }
  );
  objectsMap.erase(obj.uuid);
}

void Project::Scene::removeAllObjects() {
  objectsMap.clear();
  root.children.clear();
}

bool Project::Scene::moveObject(uint32_t uuidObject, uint32_t uuidTarget, bool asChild)
{
  if(uuidObject == uuidTarget) {
    return false;
  }

  auto objIt = objectsMap.find(uuidObject);
  auto targetIt = objectsMap.find(uuidTarget);
  bool targetIsRoot = uuidTarget == root.uuid;
  if (objIt == objectsMap.end() || (!targetIsRoot && targetIt == objectsMap.end())) {
    return false;
  }

  auto obj = objIt->second;
  auto target = targetIsRoot ? std::shared_ptr<Object>{} : targetIt->second;

  // Moving object into descendant --> Disallow
  if (!targetIsRoot && isDescendantOf(target.get(), uuidObject))
    return false;

  // Remove from current parent
  if (obj->parent) {
    std::erase_if(
      obj->parent->children,
      [&obj](const std::shared_ptr<Object> &ref) { return ref->uuid == obj->uuid; }
    );
  }

  if (asChild) {
    // Add as child to target (or root)
    if (targetIsRoot) {
      root.children.push_back(obj);
      obj->parent = &root;
    } else {
      target->children.push_back(obj);
      obj->parent = target.get();
    }
  } else {
    // Special case: insert at top if dropping above root
    if (uuidTarget == root.uuid) {
      root.children.insert(root.children.begin(), obj);
      obj->parent = &root;
    } else {
      // Add as sibling to target
      auto parent = target->parent;
      if (parent) {
        // insert after target
        auto &siblings = parent->children;
        auto it = std::find_if(
          siblings.begin(), siblings.end(),
          [&target](const std::shared_ptr<Object> &ref) { return ref->uuid == target->uuid; }
        );
        if (it != siblings.end())
        {
          siblings.insert(it + 1, obj);
          obj->parent = parent;
        }
      }
    }
  }

  return true;
}

void Project::Scene::save()
{
  Utils::FS::saveTextFile(scenePath + "/scene.json", serialize());
}

uint32_t Project::Scene::createPrefabFromObject(uint32_t uuid)
{
  auto obj = getObjectByUUID(uuid);
  if(!obj)return 0;

  Prefab prefab{};
  prefab.uuid.value = Utils::Hash::randomU64();

  // Scene objects are world-positioned, the engine has no transform hierarchy. Re-base
  // the subtree so each descendant's transform is relative to its parent. Otherwise
  // expanding the prefab at an instance's transform would double-count world positions
  // and place nested objects far away.
  std::function<void(Object&, glm::vec3, glm::quat, glm::vec3)> rebase =
    [&](Object &node, glm::vec3 pPos, glm::quat pRot, glm::vec3 pScale) {
      glm::quat pRotInv = glm::inverse(pRot);
      for(auto &child : node.children) {
        glm::vec3 &cp = child->pos.resolve(child->propOverrides);
        glm::quat &cr = child->rot.resolve(child->propOverrides);
        glm::vec3 &cs = child->scale.resolve(child->propOverrides);
        glm::vec3 cwPos = cp, cwScale = cs;
        glm::quat cwRot = cr;
        cp = (pRotInv * (cwPos - pPos)) / pScale;
        cr = pRotInv * cwRot;
        cs = cwScale / pScale;
        rebase(*child, cwPos, cwRot, cwScale);
      }
    };
  rebase(*obj,
    obj->pos.resolve(obj->propOverrides),
    obj->rot.resolve(obj->propOverrides),
    obj->scale.resolve(obj->propOverrides));

  auto prefabJson = prefab.serialize(*obj);

  std::string name = obj->name;

  name.erase(std::remove_if(name.begin(), name.end(),
    [](char c) { return !std::isalnum(c) && c != '_'; }
  ), name.end());
  if(name.empty())name = "prefab " + std::to_string(prefab.uuid.value);

  Utils::FS::saveTextFile(
    ctx.project->getPath() + "/assets/" + name + ".prefab",
    prefabJson
  );

  ctx.project->getAssets().reload();

  // Convert the source object into a thin instance of the new prefab, so its content
  // now comes from the prefab definition (single source of truth) and the inspector
  // shows it as a prefab instance. Its placement is kept via transform overrides.
  std::function<void(Object&)> unregister = [&](Object &o) {
    for(auto &child : o.children) unregister(*child);
    ctx.removeObjectSelection(o.uuid);
    objectsMap.erase(o.uuid);
  };
  for(auto &child : obj->children) unregister(*child);

  obj->children.clear();
  obj->components.clear();
  obj->propOverrides.clear();
  obj->uuidPrefab.value = prefab.uuid.value;
  obj->addPropOverride(obj->pos);
  obj->addPropOverride(obj->rot);
  obj->addPropOverride(obj->scale);
  return 0;
}

void Project::Scene::unpackPrefabInstance(uint32_t uuid)
{
  auto inst = getObjectByUUID(uuid);
  if(!inst || !inst->isPrefabInstance())return;
  auto prefab = ctx.project->getAssets().getPrefabByUUID(inst->uuidPrefab.value);
  if(!prefab)return;
  auto &def = prefab->obj;

  // The instance keeps its uuid, placement and root-level component overrides (same
  // component uuids resolve), so it just gains the prefab root's components.
  glm::vec3 wPos = inst->pos.resolve(inst->propOverrides);
  glm::quat wRot = inst->rot.resolve(inst->propOverrides);
  glm::vec3 wScale = inst->scale.resolve(inst->propOverrides);

  auto cloneComponents = [](Object &dst, const Object &src) {
    for(const auto &comp : src.components) {
      auto &cdef = Component::TABLE[comp.id];
      auto data = cdef.funcSerialize(comp);
      dst.components.push_back(Component::Entry{
        .id = comp.id, .uuid = comp.uuid, .name = comp.name,
        .data = cdef.funcDeserialize(data)
      });
    }
  };
  cloneComponents(*inst, def);

  // Materialize the prefab's child tree with composed world transforms. Scene objects
  // are world-positioned, so each node bakes its world transform. Nested prefab
  // instances stay thin (keep uuidPrefab + their prefab-authored overrides).
  std::function<void(Object&, std::shared_ptr<Object>, glm::vec3, glm::quat, glm::vec3)> build =
    [&](Object &defNode, const std::shared_ptr<Object> &parent,
        glm::vec3 pPos, glm::quat pRot, glm::vec3 pScale)
  {
    auto child = std::make_shared<Object>(*parent);
    child->name = defNode.name;
    child->uuid = Utils::Hash::randomU64();
    child->enabled = defNode.enabled;
    child->selectable = defNode.selectable;

    glm::vec3 lpos = defNode.pos.resolve(defNode.propOverrides);
    glm::quat lrot = defNode.rot.resolve(defNode.propOverrides);
    glm::vec3 lscale = defNode.scale.resolve(defNode.propOverrides);
    glm::vec3 cwPos = pPos + pRot * (pScale * lpos);
    glm::quat cwRot = pRot * lrot;
    glm::vec3 cwScale = pScale * lscale;
    child->pos.value = cwPos;
    child->rot.value = cwRot;
    child->scale.value = cwScale;

    if(defNode.isPrefabInstance()) {
      // Stays a prefab instance, now placed in world space.
      child->uuidPrefab.value = defNode.uuidPrefab.value;
      child->propOverrides = defNode.propOverrides; // prefab-authored overrides
      child->addPropOverride(child->pos);
      child->addPropOverride(child->rot);
      child->addPropOverride(child->scale);
    } else {
      cloneComponents(*child, defNode);
    }

    addObject(*parent, child);

    if(!defNode.isPrefabInstance()) {
      for(const auto &gc : defNode.children) {
        build(*gc, child, cwPos, cwRot, cwScale);
      }
    }
  };

  for(const auto &defChild : def.children) {
    build(*defChild, inst, wPos, wRot, wScale);
  }

  inst->uuidPrefab.value = 0;
}

std::string Project::Scene::serialize(bool minify) {
  nlohmann::json doc{};
  doc["conf"] = conf.serialize();
  doc["graph"] = root.serialize();
  return doc.dump(minify ? -1 : 2);
}

void Project::Scene::resetLayers()
{
  conf.layers3D.clear();
  conf.layersPtx.clear();
  conf.layers2D.clear();

  LayerConf layer{};
  layer.name.value = "3D Opaque";
  layer.depthCompare.value = true;
  layer.depthWrite.value = true;
  layer.blender.value = 0;
  conf.layers3D.push_back(layer);

  layer.name.value = "3D Transp.";
  layer.depthCompare.value = true;
  layer.depthWrite.value = false;
  layer.blender.value = RDPQ_BLENDER_MULTIPLY;
  conf.layers3D.push_back(layer);

  layer.name.value = "PTX Opaque";
  layer.depthCompare.value = true;
  layer.depthWrite.value = true;
  layer.blender.value = 0;
  conf.layersPtx.push_back(layer);

  layer.name.value = "2D";
  layer.depthCompare.value = false;
  layer.depthWrite.value = false;
  layer.blender.value = 0;
  conf.layers2D.push_back(layer);
}

void Project::Scene::deserialize(const std::string &data)
{
  auto doc = nlohmann::json::parse(
    !data.empty() ? data : "{\"conf\": {}}",
    nullptr, false);
  if (!doc.is_object())return;

  auto &docConf = doc["conf"];
  {
    Utils::JSON::readProp(docConf, conf.name, std::string{"New Scene"});
    conf.fbWidth = docConf.value("fbWidth", 320);
    conf.fbHeight = docConf.value("fbHeight", 240);
    conf.fbFormat = docConf.value("fbFormat", 0);
    Utils::JSON::readProp(docConf, conf.clearColor);
    Utils::JSON::readProp(docConf, conf.doClearColor);
    Utils::JSON::readProp(docConf, conf.doClearDepth);
    Utils::JSON::readProp(docConf, conf.renderPipeline);
    Utils::JSON::readProp(docConf, conf.frameLimit, 0);
    Utils::JSON::readProp(docConf, conf.filter, 0);
    Utils::JSON::readProp(docConf, conf.audioFreq, 32000);
    Utils::JSON::readProp(docConf, conf.physicsTickRate, 50);
    Utils::JSON::readProp(docConf, conf.gravity, glm::vec3{0.0f, -9.81f, 0.0f});
    Utils::JSON::readProp(docConf, conf.visualUnitsPerMeter, 100.0f);
    Utils::JSON::readProp(docConf, conf.velocitySolverIterations, 7);
    Utils::JSON::readProp(docConf, conf.positionSolverIterations, 6);
    Utils::JSON::readProp(docConf, conf.interpolatePhysicsTransforms, true);

    auto readLayer = [](const nlohmann::json &dom) {
      LayerConf layer{};
      Utils::JSON::readProp(dom, layer.name);
      Utils::JSON::readProp(dom, layer.depthCompare, true);
      Utils::JSON::readProp(dom, layer.depthWrite, true);
      Utils::JSON::readProp(dom, layer.blender);
      Utils::JSON::readProp(dom, layer.fog, false);
      Utils::JSON::readProp(dom, layer.fogColorMode, 0u);
      Utils::JSON::readProp(dom, layer.fogColor);
      Utils::JSON::readProp(dom, layer.fogMin, 0.0f);
      Utils::JSON::readProp(dom, layer.fogMax, 0.0f);
      Utils::JSON::readProp(dom, layer.lightMode, 0);

      return layer;
    };

    conf.layers3D.clear();
    conf.layersPtx.clear();
    conf.layers2D.clear();
    for(auto &item : docConf["layers3D"]) {
      conf.layers3D.push_back(readLayer(item));
    }
    for(auto &item : docConf["layersPtx"]) {
      conf.layersPtx.push_back(readLayer(item));
    }
    for(auto &item : docConf["layers2D"]) {
      conf.layers2D.push_back(readLayer(item));
    }
    if(conf.layers3D.empty()) {
      resetLayers();
    }
  }

  removeAllObjects();
  if(!doc.contains("graph"))return;
  auto docGraph = doc["graph"];
  root.deserialize(this, docGraph);
}

uint32_t Project::Scene::assignRuntimeIds()
{
  // Pre-order traversal: parents get a lower id than their children, root stays 0.
  // Ids are unique per scene and only valid for this build.
  uint32_t nextId = 1;
  auto assign = [&nextId](const std::shared_ptr<Object> &obj, auto &assignRef) -> void
  {
    if(nextId > 0xFFFF) {
      Utils::Logger::log("Scene has more than 65535 objects, runtime ids overflow", Utils::Logger::LEVEL_ERROR);
      return;
    }
    obj->runtimeId = static_cast<uint16_t>(nextId++);
    for(const auto &child : obj->children) {
      assignRef(child, assignRef);
    }
  };

  root.runtimeId = 0;
  for(const auto &child : root.children) {
    assign(child, assign);
  }
  return nextId; // first free id, used as the base for expanded prefab-instance children
}
