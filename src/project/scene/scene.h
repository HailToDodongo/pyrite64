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
    PROP_S32(lightMode);
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
    PROP_FLOAT(audioMasterVolume);
    PROP_BOOL(audioReverb);
    PROP_FLOAT(audioReverbGain);
    PROP_FLOAT(audioReverbFeedback);
    PROP_FLOAT(audioReverbCross);
    PROP_FLOAT(audioReverbSendScale);
    PROP_S32(physicsTickRate);
    PROP_VEC3(gravity);
    PROP_FLOAT(visualUnitsPerMeter);
    PROP_S32(velocitySolverIterations);
    PROP_S32(positionSolverIterations);
    PROP_BOOL(interpolatePhysicsTransforms);

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
      std::shared_ptr<Object> addObject(Object &parent, std::shared_ptr<Object> obj, bool generateUUID = false);

      std::shared_ptr<Object> addPrefabInstance(uint64_t prefabUUID);

      /**
       * Creates an object with a static or animated Model component for a 3D model asset.
       * @param modelUUID UUID of the 3D model asset.
       * @return Created scene object, or null when the asset is not a 3D model.
       */
      std::shared_ptr<Object> addModelObject(uint64_t modelUUID);

      void removeObject(Object &obj);
      void removeAllObjects();

      /**
       * Moves an object relative to another scene object or the scene root.
       *
       * @param uuidObject UUID of the object to move.
       * @param uuidTarget UUID of the destination object or scene root.
       * @param asChild Whether to append the object as a child of the destination.
       * @param insertBefore Whether sibling insertion should occur before the destination (this is the only way to insert as first child when there are already child elements).
       * @return True when the object was moved.
       */
      bool moveObject(uint32_t uuidObject, uint32_t uuidTarget, bool asChild, bool insertBefore = false);

      std::shared_ptr<Object> getObjectByUUID(uint32_t uuid) {
        if (objectsMap.contains(uuid)) {
          return objectsMap[uuid];
        }
        return nullptr;
      }

      uint64_t createPrefabFromObject(uint32_t uuid, const std::string &subDir = {});

      // Unpacks a prefab instance (shallow) into real, editable scene objects
      void unpackPrefabInstance(uint32_t uuid);

      std::string serialize(bool minify = false);

      void resetLayers();

      void deserialize(const std::string &data);

      // Assigns the runtime object ids (uint16_t) for the whole tree.
      // Build-time only: must be called before serializing objects to the runtime format.
      // Returns the first free id (base for build-time expanded prefab-instance children).
      uint32_t assignRuntimeIds();
  };
}
