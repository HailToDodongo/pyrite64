/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "object.h"

namespace Project
{
  struct LayerConf
  {
    PROP_STRING(name);
    PROP_BOOL(depthCompare);
    PROP_BOOL(depthWrite);
    PROP_U32(blender);
  };

  struct SceneConf
  {
    PROP_STRING(name);
    int fbWidth{320};
    int fbHeight{240};
    int fbFormat{0};
    glm::vec4 clearColor{};
    PROP_BOOL(doClearColor);
    PROP_BOOL(doClearDepth);
    PROP_S32(renderPipeline);

    std::vector<LayerConf> layers3D{};
    std::vector<LayerConf> layersPtx{};
    std::vector<LayerConf> layers2D{};

    std::string serialize() const;
  };

  class Scene
  {
    private:
      int id{};
      Object root{};
      std::string scenePath{};

    public:
      SceneConf conf{};

      Scene(int id_, const std::string &projectPath);

      int getId() const { return id; }
      const std::string &getName() const { return conf.name.value; }

      void save();
      Object& getRootObject() { return root; }

      std::unordered_map<uint32_t, std::shared_ptr<Object>> objectsMap{};

      std::shared_ptr<Object> addObject(std::string &objJson);
      std::shared_ptr<Object> addObject(Object &parent);
      std::shared_ptr<Object> addObject(Object &parent, std::shared_ptr<Object> obj);

      std::shared_ptr<Object> addPrefabInstance(uint64_t prefabUUID);

      void removeObject(Object &obj);
      void removeAllObjects();

      bool moveObject(uint32_t uuidObject, uint32_t uuidTarget, bool asChild);

      std::shared_ptr<Object> getObjectByUUID(uint32_t uuid) {
        if (objectsMap.contains(uuid)) {
          return objectsMap[uuid];
        }
        return nullptr;
      }

      uint32_t createPrefabFromObject(uint32_t uuid);

      std::string serialize();

      void resetLayers();

      void deserialize(const std::string &data);

      uint16_t getFreeObjectId();
  };
}