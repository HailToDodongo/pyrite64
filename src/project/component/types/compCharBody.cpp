/**
 * @copyright 2026 - Max Bebök
 * @license MIT
 */
#include "../components.h"
#include "../../../context.h"
#include "../../../editor/imgui/helper.h"
#include "../../../utils/json.h"
#include "../../../utils/jsonBuilder.h"
#include "../../../utils/binaryFile.h"
#include "../../../editor/pages/parts/viewport3D.h"
#include "../../../utils/meshGen.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>

namespace
{
  constexpr uint32_t COLLTYPE_MESH    = 1;
  constexpr uint32_t COLLTYPE_BODIES  = 2;
  constexpr uint32_t COLLTYPE_ALL     = 0xFF;
}

namespace Project::Component::CharBody
{
  struct Data
  {
    PROP_VEC3(up);
    PROP_VEC3(centerOffset);
    PROP_FLOAT(gravity);
    PROP_FLOAT(maxFallSpeed);
    PROP_FLOAT(floorMaxAngle);
    PROP_FLOAT(stepHeight);
    PROP_FLOAT(floorSnapDistance);
    PROP_FLOAT(radius);
    PROP_FLOAT(height);
    PROP_U32(collTypes);
    PROP_U32(maxSlides);
    PROP_U32(readMask);
    PROP_BOOL(followFloor);
  };

  std::shared_ptr<Data> makeDefault() {
    auto data = std::make_shared<Data>();
    data->up.value            = {0.0f, 1.0f, 0.0f};
    data->centerOffset.value  = {0.0f, 0.0f, 0.0f};
    data->gravity.value       = 30.0f;
    data->maxFallSpeed.value  = 55.0f;
    data->floorMaxAngle.value = 0.785398f; // 45 deg in radians
    data->stepHeight.value    = 0.25f;
    data->floorSnapDistance.value = 0.30f;
    data->radius.value        = 0.5f;
    data->height.value        = 2.0f;
    data->collTypes.value     = COLLTYPE_MESH;
    data->maxSlides.value     = 4;
    data->readMask.value      = 0x1;
    data->followFloor.value   = true;
    return data;
  }

  std::shared_ptr<void> init(Object &obj) {
    return makeDefault();
  }

  void update(Object& obj, Entry &entry) {}

  nlohmann::json serialize(const Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    return Utils::JSON::Builder{}
      .set(data.up)
      .set(data.centerOffset)
      .set(data.gravity)
      .set(data.maxFallSpeed)
      .set(data.floorMaxAngle)
      .set(data.stepHeight)
      .set(data.floorSnapDistance)
      .set(data.radius)
      .set(data.height)
      .set(data.collTypes)
      .set(data.maxSlides)
      .set(data.readMask)
      .set(data.followFloor)
      .doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = makeDefault();
    Utils::JSON::readProp(doc, data->up,              data->up.value);
    Utils::JSON::readProp(doc, data->centerOffset,    data->centerOffset.value);
    Utils::JSON::readProp(doc, data->gravity,         data->gravity.value);
    Utils::JSON::readProp(doc, data->maxFallSpeed,    data->maxFallSpeed.value);
    Utils::JSON::readProp(doc, data->floorMaxAngle,   data->floorMaxAngle.value);
    Utils::JSON::readProp(doc, data->stepHeight,      data->stepHeight.value);
    Utils::JSON::readProp(doc, data->floorSnapDistance, data->floorSnapDistance.value);
    Utils::JSON::readProp(doc, data->radius,          data->radius.value);
    Utils::JSON::readProp(doc, data->height,          data->height.value);
    Utils::JSON::readProp(doc, data->collTypes,       data->collTypes.value);
    Utils::JSON::readProp(doc, data->maxSlides,       data->maxSlides.value);
    Utils::JSON::readProp(doc, data->readMask,        data->readMask.value);
    Utils::JSON::readProp(doc, data->followFloor,     data->followFloor.value);
    return data;
  }

  void build(Object& obj, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    ctx.fileObj.write(data.up.resolve(obj.propOverrides));
    ctx.fileObj.write(data.centerOffset.resolve(obj.propOverrides));
    ctx.fileObj.write(data.gravity.resolve(obj.propOverrides));
    ctx.fileObj.write(data.maxFallSpeed.resolve(obj.propOverrides));
    ctx.fileObj.write(data.floorMaxAngle.resolve(obj.propOverrides));
    ctx.fileObj.write(data.stepHeight.resolve(obj.propOverrides));
    ctx.fileObj.write(data.floorSnapDistance.resolve(obj.propOverrides));
    ctx.fileObj.write(data.radius.resolve(obj.propOverrides));
    ctx.fileObj.write(data.height.resolve(obj.propOverrides));
    ctx.fileObj.write<uint8_t>(data.collTypes.resolve(obj.propOverrides));
    ctx.fileObj.write<uint8_t>(data.maxSlides.resolve(obj.propOverrides));
    ctx.fileObj.write<uint8_t>(data.readMask.resolve(obj.propOverrides));
    ctx.fileObj.write<uint8_t>(data.followFloor.resolve(obj.propOverrides) ? 1 : 0);
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    if (ImTable::start("Comp", &obj)) {
      ImTable::add("Name", entry.name);

      if(ImTable::add("Radius", data.radius.value)) {
        data.radius.value = std::max(0.01f, data.radius.value);
      }
      if(ImTable::add("Height", data.height.value)) {
        data.height.value = std::max(data.radius.value * 2.0f, data.height.value);
      }
      ImTable::addObjProp("Offset", data.centerOffset);

      // --- physics ---
      auto &stepH = data.stepHeight.resolve(obj.propOverrides);
      auto &snapD = data.floorSnapDistance.resolve(obj.propOverrides);
      float halfH  = data.height.resolve(obj.propOverrides) * 0.5f;
      float innerH = halfH - data.radius.resolve(obj.propOverrides);
      float maxStep = std::min(innerH, snapD);

      if(ImTable::add("Step Height", stepH)) {
        stepH = std::clamp(stepH, 0.0f, maxStep);
      }
      if(ImTable::add("Floor Snap Dist.", snapD)) {
        snapD = std::max(snapD, stepH);
      }

      if(ImTable::add("Gravity", data.gravity.value)) {
        data.gravity.value = std::max(0.0f, data.gravity.value);
      }
      if(ImTable::add("Max Fall Speed", data.maxFallSpeed.value)) {
        data.maxFallSpeed.value = std::max(0.0f, data.maxFallSpeed.value);
      }

      float angleDeg = glm::degrees(data.floorMaxAngle.resolve(obj.propOverrides));
      if(ImTable::add("Floor Max Angle", angleDeg)) {
        angleDeg = std::clamp(angleDeg, 0.0f, 90.0f);
        data.floorMaxAngle.value = glm::radians(angleDeg);
      }

      auto &slides = data.maxSlides.resolve(obj.propOverrides);
      int slideInt = (int)slides;
      if(ImTable::add("Max Slides", slideInt)) {
        slideInt = std::clamp(slideInt, 1, 8);
        slides = (uint32_t)slideInt;
      }

      ImTable::addObjProp("Follow Floor", data.followFloor);

      ImTable::addObjProp("Up Direction", data.up);

      // --- collision ---
      ImTable::addMultiSelectMask8("Read Mask", data.readMask.resolve(obj), ctx.project->conf.collLayerNames, "<Nothing>");

      std::vector<const char*> collTypeItems{"Mesh Colliders", "Collider Bodies", "All"};
      int collTypeSel = 0;
      uint32_t ct = data.collTypes.resolve(obj.propOverrides);
      if(ct == COLLTYPE_BODIES)      collTypeSel = 1;
      else if(ct == COLLTYPE_ALL)    collTypeSel = 2;
      if(ImTable::addComboBox("Collider Types", collTypeSel, collTypeItems)) {
        if(collTypeSel == 0)      data.collTypes.value = COLLTYPE_MESH;
        else if(collTypeSel == 1) data.collTypes.value = COLLTYPE_BODIES;
        else                      data.collTypes.value = COLLTYPE_ALL;
      }

      ImTable::end();
    }
  }

  void draw3D(Object& obj, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass)
  {
    auto *scene = ctx.project->getScenes().getLoadedScene();
    if(!scene) return;

    Data &data = *static_cast<Data*>(entry.data.get());
    auto &objPos   = obj.pos.resolve(obj.propOverrides);
    auto &objRot   = obj.rot.resolve(obj.propOverrides);
    auto &objScale = obj.scale.resolve(obj.propOverrides);

    float r      = data.radius.resolve(obj.propOverrides);
    float h      = data.height.resolve(obj.propOverrides);
    float hh     = std::max(h * 0.5f, r);
    float ih     = hh - r;
    float stepH  = std::min(std::min(data.stepHeight.resolve(obj.propOverrides), ih), data.floorSnapDistance.resolve(obj.propOverrides));
    float ihPhys = ih - stepH;
    float snapD  = data.floorSnapDistance.resolve(obj.propOverrides);
    glm::vec3 upDir = glm::normalize(data.up.resolve(obj.propOverrides));

    float vuPerMeter = scene->conf.visualUnitsPerMeter.value;
    float toVis = vuPerMeter * (objScale.x + objScale.y + objScale.z) / 3.0f;

    glm::vec3 localOff = data.centerOffset.resolve(obj.propOverrides);
    glm::vec3 center   = objPos + (objRot * (localOff * toVis));

    // Full logical capsule (dim grey outline)
    Utils::Mesh::addLineCapsule(*vp.getLines(),
      center,
      glm::vec3{r, ih, r} * toVis,
      glm::u8vec4{0x60, 0x60, 0x60, 0xFF},
      objRot
    );

    // Physics capsule (green, shortened by stepHeight)
    glm::u8vec4 physColor{0x00, 0xCC, 0x40, 0xFF};
    Utils::Mesh::addLineCapsule(*vp.getLines(),
      center,
      glm::vec3{r, ihPhys, r} * toVis,
      physColor,
      objRot
    );

    // Step zone indicator: horizontal cross at physics capsule bottom
    if(stepH > 0.001f) {
      glm::vec3 physBottom = center - upDir * ((ihPhys + r) * toVis);
      glm::vec3 perpA = glm::normalize(glm::vec3{upDir.y, upDir.z, upDir.x}) * r * toVis;
      glm::vec3 perpB = glm::normalize(glm::cross(upDir, perpA)) * r * toVis;
      glm::u8vec4 stepCol{0xFF, 0xCC, 0x00, 0xFF};
      Utils::Mesh::addLine(*vp.getLines(), physBottom - perpA, physBottom + perpA, stepCol);
      Utils::Mesh::addLine(*vp.getLines(), physBottom - perpB, physBottom + perpB, stepCol);
    }

    // Floor snap probe line (blue, from center downward)
    float probeLen = (hh + snapD) * toVis;
    glm::vec3 probeEnd = center - upDir * probeLen;
    Utils::Mesh::addLine(*vp.getLines(), center, probeEnd, glm::u8vec4{0x60, 0x60, 0xFF, 0xFF});
  }

  void drawCopyPass(Object&, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPUCopyPass* pass) {}
  Utils::AABB getAABB(Object &obj, Entry &entry) { return {}; }
}
