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
    PROP_BOOL(fog);
    PROP_U32(fogColorMode);
    PROP_VEC4(fogColor);
    PROP_FLOAT(fogMin);
    PROP_FLOAT(fogMax);
  };

  struct SceneConf
  {
    PROP_STRING(name);
    int fbWidth{320};
    int fbHeight{240};
    int fbFormat{0};
    PROP_VEC4(clearColor);
    PROP_BOOL(doClearColor);
    PROP_BOOL(doClearDepth);
    PROP_S32(renderPipeline);
    PROP_S32(frameLimit);
    PROP_S32(filter);
    PROP_S32(audioFreq);

    std::vector<LayerConf> layers3D{};
    std::vector<LayerConf> layersPtx{};
    std::vector<LayerConf> layers2D{};

    nlohmann::json serialize() const;
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

      std::shared_ptr<Object> addObject(std::string &objJson, uint64_t parentUUID = 0);
      std::shared_ptr<Object> addObject(Object &parent);
      std::shared_ptr<Object> addObject(Object &parent, std::shared_ptr<Object> obj, bool generateIDs = false);

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

      std::string serialize(bool minify = false);

      void resetLayers();

      void deserialize(const std::string &data);

      uint16_t getFreeObjectId();
  };
}
