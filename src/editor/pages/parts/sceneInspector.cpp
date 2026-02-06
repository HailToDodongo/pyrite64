/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "sceneInspector.h"
#include "../../../context.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "../../imgui/helper.h"
#include "../../imgui/lang.h"

Editor::SceneInspector::SceneInspector() {
}

void Editor::SceneInspector::draw() {
  auto scene = ctx.project->getScenes().getLoadedScene();
  if(!scene)return;

  if (ImGui::CollapsingHeader(message(MSG_SCENE_SETTINGS), ImGuiTreeNodeFlags_DefaultOpen)) {
    ImTable::start("Settings");
    ImTable::addProp(message(MSG_SCENE_NAME), scene->conf.name);

    const char* OPTIONS[] = {
      message(MSG_SCENE_PIPELINES_DEFAULT),
      message(MSG_SCENE_PIPELINES_HDR_BLOOM),
      message(MSG_SCENE_PIPELINES_HIRES_TEX)
    };
    ImTable::addComboBox(
      message(MSG_SCENE_PIPELINE),
      scene->conf.renderPipeline.value,
      OPTIONS, 3
    );

    std::vector<ImTable::ComboEntry> fpsEntries{
      {0, message(MSG_SCENE_FPS_UNLIMITED)},
      {1, "30 / 25"},
      {2, "20 / 16.6"},
      {3, "15 / 12.5"},
    };
    ImTable::addVecComboBox(message(MSG_SCENE_FPS_LIMIT), fpsEntries, scene->conf.frameLimit.value);

    ImTable::end();
  }

  bool fbDisabled = false;
  if(scene->conf.renderPipeline.value != 0)
  {
    // HDR/Bloom and bigtex both need those specific settings to work:
    scene->conf.fbWidth = 320;
    scene->conf.fbHeight = 240;
    scene->conf.fbFormat = 0;
    scene->conf.doClearColor.value = false;
    fbDisabled = true;
  }

  if (ImGui::CollapsingHeader(message(MSG_SCENE_FRAMEBUFFER), ImGuiTreeNodeFlags_DefaultOpen)) {
    ImTable::start("Framebuffer");

    if(fbDisabled)ImGui::BeginDisabled();
    ImTable::add(message(MSG_SCENE_WIDTH), scene->conf.fbWidth);
    ImTable::add(message(MSG_SCENE_HEIGHT), scene->conf.fbHeight);

    constexpr const char* const FORMATS[] = {"RGBA16","RGBA32"};
    ImTable::addComboBox(message(MSG_SCENE_FORMAT), scene->conf.fbFormat, FORMATS, 2);

    if(fbDisabled)ImGui::EndDisabled();
      ImTable::addColor(message(MSG_SCENE_COLOR), scene->conf.clearColor.value, false);
      scene->conf.clearColor.value.a = 1.0f;
    if(fbDisabled)ImGui::BeginDisabled();

    ImTable::addProp(message(MSG_SCENE_CLEAR_COLOR), scene->conf.doClearColor);

    if(fbDisabled)ImGui::EndDisabled();

    ImTable::addProp(message(MSG_SCENE_CLEAR_DEPTH), scene->conf.doClearDepth);

    const std::array<const char*, 5> FILTERS = {
      message(MSG_SCENE_FILTERS_NONE),
      message(MSG_SCENE_FILTERS_RESAMPLE),
      message(MSG_SCENE_FILTERS_DEDITHER),
      message(MSG_SCENE_FILTERS_RESAMPLE_AA),
      message(MSG_SCENE_FILTERS_RESAMPLE_AA_DEDITHER)
    };
    ImTable::addComboBox(message(MSG_SCENE_FILTER), scene->conf.filter.value, FILTERS.data(), FILTERS.size());

    ImTable::end();
  }
}
