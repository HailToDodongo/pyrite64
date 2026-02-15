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

      ImTable::add("Name", entry.name);
      ImTable::addObjProp("Offset", data.vpOffset);
      ImTable::addObjProp("Size", data.vpSize);

      //float fov = glm::degrees(data.fov.resolve(obj));
      ImTable::addObjProp("FOV", data.fov);
      //data.fov.resolve(obj) = glm::radians(fov);


      ImTable::addObjProp("Near", data.near);
      ImTable::addObjProp("Far", data.far);

      ImTable::addObjProp("Aspect", data.aspect);
      //ImTable::addComboBox("Type", data.type, LIGHT_TYPES, LIGHT_TYPE_COUNT);
      ImTable::end();
    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    auto pos = obj.pos.resolve(obj);

    bool isSelected = ctx.isObjectSelected(obj.uuid);

    // calculate frustum corners in world space
    float fovY = glm::radians(data.fov.resolve(obj));
    float aspect = data.aspect.resolve(obj);
    float nearDist = data.near.resolve(obj);
    float farDist = nearDist + 85;//data.far.resolve(obj);
    if (aspect <= 0.0f) {
      aspect = (float)data.vpSize.resolve(obj).x / (float)data.vpSize.resolve(obj).y;
    }

    float tanFovY = tanf(fovY * 0.5f);
    float nearHeight = 2.0f * tanFovY * nearDist;
    float nearWidth = nearHeight * aspect;
    float farHeight = 2.0f * tanFovY * farDist;
    float farWidth = farHeight * aspect;

    glm::vec3 camPos = pos;
    auto rot = obj.rot.resolve(obj);
    glm::vec3 forward = rot * glm::vec3(0,0,-1);
    glm::vec3 up = rot * glm::vec3(0,1,0);
    glm::vec3 right = rot * glm::vec3(1,0,0);

    glm::vec3 nc = camPos + forward * nearDist;
    glm::vec3 fc = camPos + forward * farDist;

    // Near plane corners
    glm::vec3 ntl = nc + (up * (nearHeight/2.0f)) - (right * (nearWidth/2.0f));
    glm::vec3 ntr = nc + (up * (nearHeight/2.0f)) + (right * (nearWidth/2.0f));
    glm::vec3 nbl = nc - (up * (nearHeight/2.0f)) - (right * (nearWidth/2.0f));
    glm::vec3 nbr = nc - (up * (nearHeight/2.0f)) + (right * (nearWidth/2.0f));
    // Far plane corners
    glm::vec3 ftl = fc + (up * (farHeight/2.0f)) - (right * (farWidth/2.0f));
    glm::vec3 ftr = fc + (up * (farHeight/2.0f)) + (right * (farWidth/2.0f));
    glm::vec3 fbl = fc - (up * (farHeight/2.0f)) - (right * (farWidth/2.0f));
    glm::vec3 fbr = fc - (up * (farHeight/2.0f)) + (right * (farWidth/2.0f));

    // Draw frustum edges
    glm::u8vec4 col = isSelected ? Utils::Colors::kSelectionTint : glm::u8vec4{0xFF};
    // Near plane
    Utils::Mesh::addLine(*vp.getLines(), ntl, ntr, col);
    Utils::Mesh::addLine(*vp.getLines(), ntr, nbr, col);
    Utils::Mesh::addLine(*vp.getLines(), nbr, nbl, col);
    Utils::Mesh::addLine(*vp.getLines(), nbl, ntl, col);
    // Far plane
    Utils::Mesh::addLine(*vp.getLines(), ftl, ftr, col);
    Utils::Mesh::addLine(*vp.getLines(), ftr, fbr, col);
    Utils::Mesh::addLine(*vp.getLines(), fbr, fbl, col);
    Utils::Mesh::addLine(*vp.getLines(), fbl, ftl, col);
    // Connect near and far
    Utils::Mesh::addLine(*vp.getLines(), ntl, ftl, col);
    Utils::Mesh::addLine(*vp.getLines(), ntr, ftr, col);
    Utils::Mesh::addLine(*vp.getLines(), nbl, fbl, col);
    Utils::Mesh::addLine(*vp.getLines(), nbr, fbr, col);


    // little triangle marker on top of the upper edge of the far plane
    glm::vec3 lineDist = ftr - ftl;
    glm::vec3 triCenter = ftl + (lineDist * 0.5f);
    triCenter += up * 10.0f;
    glm::vec3 triLeft = triCenter - (right * 30.0f);
    glm::vec3 triRight = triCenter + (right * 30.0f);
    triCenter += up * 30.0f;
    Utils::Mesh::addLine(*vp.getLines(), triCenter, triLeft, col);
    Utils::Mesh::addLine(*vp.getLines(), triCenter, triRight, col);
    Utils::Mesh::addLine(*vp.getLines(), triLeft, triRight, col);


    // Connect near plane and camera pos
    col = glm::u8vec4{0xCC, 0xAA, 0xAA, 0xFF};
    Utils::Mesh::addLine(*vp.getLines(), camPos, ntl, col);
    Utils::Mesh::addLine(*vp.getLines(), camPos, ntr, col);
    Utils::Mesh::addLine(*vp.getLines(), camPos, nbl, col);
    Utils::Mesh::addLine(*vp.getLines(), camPos, nbr, col);

    auto spriteCol = isSelected ? Utils::Colors::kSelectionTint : glm::u8vec4{0xFF};
    Utils::Mesh::addSprite(*vp.getSprites(), pos, obj.uuid, 3, spriteCol);
  }
}
