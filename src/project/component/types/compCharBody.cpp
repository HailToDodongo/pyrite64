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
#include <cmath>

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

    // non-saved data:
    double uiStepPulseUntil = 0.0;
    double uiSnapPulseUntil = 0.0;
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
      // Keep the step ring pulsing while this field is focused/edited
      if(ImGui::IsItemActive()) data.uiStepPulseUntil = ImGui::GetTime() + 0.15;

      if(ImTable::add("Floor Snap Dist.", snapD)) {
        snapD = std::max(snapD, stepH);
      }
      if(ImGui::IsItemActive()) data.uiSnapPulseUntil = ImGui::GetTime() + 0.15;

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

    // Step-height & snap-distance visualization:
    // Key levels along the up axis (distance below the capsule center):
    glm::vec3 footBottom = center - upDir * (hh * toVis);            // logical capsule foot (full reach)
    glm::vec3 stepTop    = center - upDir * ((hh - stepH) * toVis);  // physics capsule bottom = top of the climbable step
    glm::vec3 snapReach  = center - upDir * ((hh + snapD) * toVis);  // how far the floor-snap probe reaches below the foot
    float ringR = r * toVis;

    glm::u8vec4 footCol{0xC0, 0xC0, 0xC0, 0xFF};
    glm::u8vec4 stepCol{0xFF, 0xCC, 0x00, 0xFF};
    glm::u8vec4 snapCol{0x40, 0x90, 0xFF, 0xFF};

    // Pulse the band whose inspector field is currently being edited: lerp the
    // color toward white on a sine so the active zone visibly throbs.
    double now = ImGui::GetTime();
    float pulse = 0.5f + 0.5f * (float)std::sin(now * 8.0); // 0..1
    auto pulseCol = [&](glm::u8vec4 c, bool active) -> glm::u8vec4 {
      if(!active) return c;
      return glm::u8vec4{
        (uint8_t)(c.r + (255 - c.r) * pulse),
        (uint8_t)(c.g + (255 - c.g) * pulse),
        (uint8_t)(c.b + (255 - c.b) * pulse),
        c.a
      };
    };
    glm::u8vec4 stepDraw = pulseCol(stepCol, now < data.uiStepPulseUntil);
    glm::u8vec4 snapDraw = pulseCol(snapCol, now < data.uiSnapPulseUntil);

    glm::vec3 axisA = glm::normalize(glm::vec3{upDir.y, upDir.z, upDir.x});
    axisA = glm::normalize(axisA - upDir * glm::dot(axisA, upDir));
    glm::vec3 axisB = glm::normalize(glm::cross(upDir, axisA));

    auto addRing = [&](const glm::vec3 &c, float rad, const glm::u8vec4 &col) {
      constexpr int N = 20;
      glm::vec3 prev = c + axisA * rad;
      for(int i = 1; i <= N; ++i) {
        float t = (float)i / N * 6.28318530718f;
        glm::vec3 p = c + (axisA * std::cos(t) + axisB * std::sin(t)) * rad;
        Utils::Mesh::addLine(*vp.getLines(), prev, p, col);
        prev = p;
      }
    };
    auto addTicks = [&](const glm::vec3 &top, const glm::vec3 &bot, float rad, const glm::u8vec4 &col) {
      for(const glm::vec3 &dir : { axisA, -axisA, axisB, -axisB }) {
        glm::vec3 off = dir * rad;
        Utils::Mesh::addLine(*vp.getLines(), top + off, bot + off, col);
      }
    };

    // Foot reference ring (where the full capsule meets the ground).
    addRing(footBottom, ringR, footCol);

    // Step height: band between the physics-capsule bottom (step top) and the foot.
    // Obstacles within this band are stepped over.
    if(stepH > 0.001f) {
      addRing(stepTop, ringR, stepDraw);
      addTicks(stepTop, footBottom, ringR, stepDraw);
    }

    // Floor snap distance: band reaching from the foot down to the probe end.
    if(snapD > 0.001f) {
      addRing(snapReach, ringR, snapDraw);
      addTicks(footBottom, snapReach, ringR, snapDraw);
    }

    // Central floor-snap probe line (center down to the probe reach).
    Utils::Mesh::addLine(*vp.getLines(), center, snapReach, glm::u8vec4{0x60, 0x60, 0xFF, 0xFF});
  }

  void drawCopyPass(Object&, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPUCopyPass* pass) {}
  Utils::AABB getAABB(Object &obj, Entry &entry) { return {}; }
}
