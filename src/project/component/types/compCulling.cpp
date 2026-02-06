/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "../components.h"
#include "../../../context.h"
#include "../../../editor/imgui/helper.h"
#include "../../../editor/imgui/lang.h"
#include "../../../utils/json.h"
#include "../../../utils/jsonBuilder.h"
#include "../../../utils/binaryFile.h"
#include "../../assetManager.h"
#include "../../../editor/pages/parts/viewport3D.h"
#include "../../../renderer/scene.h"
#include "../../../utils/meshGen.h"

#include "../../../../n64/engine/include/collision/flags.h"

namespace
{
  constexpr int32_t TYPE_BOX      = 0;
  constexpr int32_t TYPE_SPHERE   = 1;
}

namespace Project::Component::Culling
{
  struct Data
  {
    PROP_VEC3(halfExtend);
    PROP_VEC3(offset);
    PROP_S32(type);
  };

  std::shared_ptr<void> init(Object &obj) {
    auto data = std::make_shared<Data>();
    return data;
  }

  nlohmann::json serialize(const Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    return Utils::JSON::Builder{}
      .set(data.halfExtend)
      .set(data.offset)
      .set(data.type)
      .doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->halfExtend, glm::vec3{1.0f, 1.0f, 1.0f});
    Utils::JSON::readProp(doc, data->offset);
    Utils::JSON::readProp(doc, data->type);
    return data;
  }

  void build(Object& obj, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    ctx.fileObj.write(data.halfExtend.resolve(obj.propOverrides));
    ctx.fileObj.write(data.offset.resolve(obj.propOverrides));
    ctx.fileObj.write<uint8_t>(data.type.resolve(obj.propOverrides));
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    if (ImTable::start("Comp", &obj)) {
      ImTable::add(Editor::message(MSG_OBJECT_COMPONENT_CULLING_NAME), entry.name);

      auto &ext = data.halfExtend.resolve(obj.propOverrides);

      ImTable::addComboBox(Editor::message(MSG_OBJECT_COMPONENT_CULLING_TYPE), data.type.value, {
        Editor::message(MSG_OBJECT_COMPONENT_CULLING_TYPES_BOX),
        Editor::message(MSG_OBJECT_COMPONENT_CULLING_TYPES_SPHERE)
      });
      if(data.type.resolve(obj.propOverrides) == TYPE_SPHERE) {
        ImTable::add(Editor::message(MSG_OBJECT_COMPONENT_CULLING_SIZE), ext.y);
        ext.x = ext.y;
        ext.z = ext.y;
      } else {
        ImTable::addObjProp(Editor::message(MSG_OBJECT_COMPONENT_CULLING_SIZE), data.halfExtend);
      }
      ImTable::addObjProp(Editor::message(MSG_OBJECT_COMPONENT_CULLING_OFFSET), data.offset);
      ImTable::end();
    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    auto &objPos = obj.pos.resolve(obj.propOverrides);
    auto &objScale = obj.scale.resolve(obj.propOverrides);

    glm::vec3 halfExt = data.halfExtend.resolve(obj.propOverrides) * objScale;
    glm::vec3 center = objPos + data.offset.resolve(obj.propOverrides);
    auto type = data.type.resolve(obj.propOverrides);

    glm::vec4 aabbCol{1.0f, 0.0f, 0.0f, 1.0f};

    if(type == TYPE_BOX)
    {
      Utils::Mesh::addLineBox(*vp.getLines(), center, halfExt, aabbCol);
      Utils::Mesh::addLineBox(*vp.getLines(), center, halfExt + 0.002f, aabbCol);
    } else if(type == TYPE_SPHERE)
    {
      Utils::Mesh::addLineSphere(*vp.getLines(), center, halfExt, aabbCol);
    }
  }
}
