/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "../components.h"
#include "../../../context.h"
#include "../../../editor/imgui/helper.h"
#include "../../../utils/json.h"
#include "../../../utils/jsonBuilder.h"
#include "../../../utils/binaryFile.h"
#include "../../../utils/logger.h"
#include "../../assetManager.h"
#include "../../../editor/pages/parts/viewport3D.h"
#include "../../../renderer/scene.h"
#include "../../../utils/meshGen.h"
#include "../../../editor/imgui/lang.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"

namespace
{
  /*constexpr char* const LIGHT_TYPES[LIGHT_TYPE_COUNT] = {
    "Ambient",
    "Directional",
    "Point"
  };*/

  glm::vec3 rotToDir(const Project::Object &obj) {
    return glm::normalize(obj.rot.value * glm::vec3{0,0,-1});
  }
}

namespace Project::Component::Camera
{
  struct Data
  {
    PROP_IVEC2(vpOffset);
    PROP_IVEC2(vpSize);
    PROP_FLOAT(fov);
    PROP_FLOAT(near);
    PROP_FLOAT(far);
    PROP_FLOAT(aspect);
  };

  std::shared_ptr<void> init(Object &obj) {
    auto data = std::make_shared<Data>();
    return data;
  }

  nlohmann::json serialize(const Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    Utils::JSON::Builder builder{};

    builder.set(data.vpOffset);
    builder.set(data.vpSize);
    builder.set(data.fov);
    builder.set(data.near);
    builder.set(data.far);
    builder.set(data.aspect);
    return builder.doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->vpOffset);
    Utils::JSON::readProp(doc, data->vpSize);
    Utils::JSON::readProp(doc, data->fov, glm::radians(70.0f));
    Utils::JSON::readProp(doc, data->near, 100.0f);
    Utils::JSON::readProp(doc, data->far, 1000.0f);
    Utils::JSON::readProp(doc, data->aspect, 0.0f);
    return data;
  }

  void build(Object& obj, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    ctx.fileObj.writeArray(glm::value_ptr(data.vpOffset.resolve(obj)), 2);
    ctx.fileObj.writeArray(glm::value_ptr(data.vpSize.resolve(obj)), 2);
    ctx.fileObj.write<float>(glm::radians(data.fov.resolve(obj)));
    ctx.fileObj.write<float>(data.near.resolve(obj));
    ctx.fileObj.write<float>(data.far.resolve(obj));
    ctx.fileObj.write<float>(data.aspect.resolve(obj));
  }

  void update(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
  }

  void draw(Object &obj, Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());

    if (ImTable::start("Comp", &obj))
    {
      auto scene = ctx.project->getScenes().getLoadedScene();
      assert(scene);
      /*auto &vpSize = data.vpSize.resolve(obj);
      if(vpSize.x == 0) vpSize.x = scene->conf.fbWidth;
      if(vpSize.y == 0) vpSize.y = scene->conf.fbHeight;*/

      ImTable::add(Editor::message(MSG_OBJECT_CAMERA_NAME), entry.name);
      ImTable::addObjProp(Editor::message(MSG_OBJECT_CAMERA_OFFSET), data.vpOffset);
      ImTable::addObjProp(Editor::message(MSG_OBJECT_CAMERA_SIZE), data.vpSize);

      //float fov = glm::degrees(data.fov.resolve(obj));
      ImTable::addObjProp(Editor::message(MSG_OBJECT_CAMERA_FOV), data.fov);
      //data.fov.resolve(obj) = glm::radians(fov);


      ImTable::addObjProp(Editor::message(MSG_OBJECT_CAMERA_NEAR), data.near);
      ImTable::addObjProp(Editor::message(MSG_OBJECT_CAMERA_FAR), data.far);

      ImTable::addObjProp(Editor::message(MSG_OBJECT_CAMERA_ASPECT), data.aspect);
      //ImTable::addComboBox("Type", data.type, LIGHT_TYPES, LIGHT_TYPE_COUNT);
      ImTable::end();
    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    constexpr float BOX_SIZE = 0.125f;
    constexpr float LINE_LEN = 0.7f;
    glm::u8vec4 col{0xFF};

    bool isSelected = ctx.selObjectUUID == obj.uuid;
/*
    if(isSelected)
    {
      Utils::Mesh::addLineBox(*vp.getLines(), obj.pos, {BOX_SIZE, BOX_SIZE, BOX_SIZE}, col);
      if(data.type == LIGHT_TYPE_DIRECTIONAL)
      {
        glm::vec3 dir = rotToDir(obj);
        Utils::Mesh::addLine(*vp.getLines(), obj.pos, obj.pos + (dir * -LINE_LEN), col);
      }
    }
*/
    Utils::Mesh::addSprite(*vp.getSprites(), obj.pos.resolve(obj.propOverrides), obj.uuid, 3, col);
  }
}
