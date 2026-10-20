/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "components.h"
#include "../scene/object.h"
#include "../scene/scene.h"
#include "../../renderer/uniforms.h"
#include "../../shader/defines.h"
#include "../../utils/logger.h"
#include "imgui.h"

namespace Project::Component
{
  std::array<CompInfo, TABLE.size()> TABLE_SORTED_BY_NAME{};

  glm::mat4 makeModelMatrix(Object &obj, float vertexScale)
  {
    glm::vec3 skew{0,0,0};
    glm::vec4 persp{0,0,0,1};
    // Vertices are quantized model units, the scene works in meters, so the model's
    // vertex scale is folded into the object scale here.
    auto trans = obj.getDisplayTrans();
    return glm::recompose(trans.scale * vertexScale, trans.rot, trans.pos, skew, persp);
  }

  bool layerCombo(uint32_t &ref, const std::vector<LayerConf> &layers)
  {
    int idx = Layer::findIndex(layers, ref);
    bool changed = false;
    if(ImGui::BeginCombo("##", idx < 0 ? "<Missing Layer>" : layers[idx].name.value.c_str())) {
      for(int i=0; i<(int)layers.size(); ++i) {
        ImGui::PushID(i);
        if(ImGui::Selectable(layers[i].name.value.c_str(), i == idx)) {
          ref = Layer::refFromIndex(layers, i);
          changed = true;
        }
        ImGui::PopID();
      }
      ImGui::EndCombo();
    }
    return changed;
  }

  void checkLayerRef(Build::SceneCtx &ctx, const Object &obj, uint32_t ref)
  {
    if(!ctx.scene || Layer::findIndex(ctx.scene->conf.layers3D, ref) >= 0)return;
    Utils::Logger::log("Object '" + obj.name + "' uses a draw-layer that doesn't exist in scene '"
      + ctx.scene->getName() + "', the default layer is used instead", Utils::Logger::LEVEL_WARN);
  }

  void applyLayerFog(Renderer::UniformN64Material &mat, const SceneConf &conf, int layerIdx, Object &obj, bool enabled)
  {
    mat.flags &= ~(FOG_TO_ALPHA | FOG_BLEND);
    if(!enabled)return;

    auto &layers = conf.layers3D;
    if(layerIdx < 0 || layerIdx >= (int)layers.size())return;
    auto &layer = layers[layerIdx];
    if(!layer.fog.resolve(obj))return;

    mat.flags |= FOG_TO_ALPHA | FOG_BLEND;

    // same checks as in "t3d_fog_set_range"
    float fogMin = layer.fogMin.resolve(obj);
    float fogDiff = layer.fogMax.resolve(obj) - fogMin;
    float minDiff = 1.5f / std::max(conf.renderScale.value, 0.001f);
    if(std::abs(fogDiff) < minDiff)fogDiff = minDiff;
    mat.fogRange = {fogMin, 1.0f / fogDiff};

    // scan through all layers to also handle the "keep unchange" setting
    mat.fogColor = {0.0f, 0.0f, 0.0f, 0.0f};
    for(int i=0; i<=layerIdx; ++i) {
      auto &l = layers[i];
      if(!l.fog.value)continue;
      if(l.fogColorMode.value == 1)mat.fogColor = conf.clearColor.value;
      if(l.fogColorMode.value == 2)mat.fogColor = l.fogColor.value;
    }
  }
}

void Project::Component::init() {
  TABLE_SORTED_BY_NAME = TABLE;
  std::ranges::sort(TABLE_SORTED_BY_NAME, [](const CompInfo &a, const CompInfo &b) {
    return strcmp(a.name, b.name) < 0;
  });
}
