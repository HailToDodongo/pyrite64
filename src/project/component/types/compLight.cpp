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
#include "../../../utils/colors.h"
#include "../../assetManager.h"
#include "../../../editor/pages/parts/viewport3D.h"
#include "../../../renderer/scene.h"
#include "../../../utils/meshGen.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"

namespace
{
  constexpr int LIGHT_TYPE_AMBIENT = 0;
  constexpr int LIGHT_TYPE_DIRECTIONAL = 1;
  constexpr int LIGHT_TYPE_POINT = 2;
  constexpr int LIGHT_TYPE_COUNT = 3;

  constexpr const char* const LIGHT_TYPES[LIGHT_TYPE_COUNT] = {
    "Ambient",
    "Directional",
    "Point"
  };

  glm::vec3 rotToDir(Project::Object &obj) {
    return glm::normalize(obj.rot.resolve(obj.propOverrides) * glm::vec3{0,0,-1});
  }
}

namespace Project::Component::Light
{
  struct Data
  {
    PROP_VEC4(color);
    PROP_S32(index);
    PROP_S32(type);
  };

  std::shared_ptr<void> init(Object &obj) {
    auto data = std::make_shared<Data>();
    return data;
  }

  nlohmann::json serialize(const Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    Utils::JSON::Builder builder{};
    builder.set(data.index);
    builder.set(data.type);
    builder.set(data.color);
    return builder.doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->index);
    Utils::JSON::readProp(doc, data->type);
    Utils::JSON::readProp(doc, data->color);
    return data;
  }

  void build(Object& obj, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    auto dir = rotToDir(obj) * 127.0f;

    ctx.fileObj.writeRGBA(data.color.resolve(obj.propOverrides));
    ctx.fileObj.write<uint8_t>(data.index.resolve(obj.propOverrides));
    ctx.fileObj.write<uint8_t>(data.type.resolve(obj.propOverrides));

    ctx.fileObj.write<int8_t>(dir.x);
    ctx.fileObj.write<int8_t>(dir.y);
    ctx.fileObj.write<int8_t>(dir.z);
  }

  void update(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    ctx.scene->addLight(Renderer::Light{
      .color = data.color.resolve(obj.propOverrides),
      .pos = glm::vec4{obj.pos.resolve(obj.propOverrides), 0.0f},
      .dir = rotToDir(obj),
      .type = data.type.resolve(obj.propOverrides),
    });
  }

  void draw(Object &obj, Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());

    if (ImTable::start("Comp", &obj))
    {
      ImTable::add("Name", entry.name);
      ImTable::addComboBox("Type", data.type.value, LIGHT_TYPES, LIGHT_TYPE_COUNT);
      ImTable::add("Index", data.index.value);
      ImTable::addColor("Color", data.color.value, true);

      ImTable::end();
    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    constexpr float BOX_SIZE = 0.125f;
    constexpr float LINE_LEN = 75.0f;
    glm::u8vec4 col = data.color.resolve(obj.propOverrides) * 255.0f;

    bool isSelected = ctx.isObjectSelected(obj.uuid);

    auto pos = obj.pos.resolve(obj.propOverrides);
    if(isSelected)
    {
      Utils::Mesh::addLineBox(*vp.getLines(), pos, {BOX_SIZE, BOX_SIZE, BOX_SIZE}, col);
      if(data.type.resolve(obj.propOverrides) == LIGHT_TYPE_DIRECTIONAL)
      {
        glm::vec3 dir = rotToDir(obj);
        Utils::Mesh::addLine(*vp.getLines(), pos, pos + (dir * -LINE_LEN), col);
      }
      col = Utils::Colors::kSelectionTint;
    }
    Utils::Mesh::addSprite(*vp.getSprites(), pos, obj.uuid, data.type.resolve(obj.propOverrides), col);
  }
}
