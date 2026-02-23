/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "sceneInspector.h"
#include "../../../context.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "../../imgui/helper.h"

Editor::SceneInspector::SceneInspector() {
}

void Editor::SceneInspector::draw() {
  auto scene = ctx.project->getScenes().getLoadedScene();
  if(!scene)return;

  if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImTable::start("Settings");
    ImTable::addProp("Name", scene->conf.name);

    constexpr const char* OPTIONS[] = {"Default", "HDR-Bloom", "HiRes-Tex (256x)"};
    ImTable::addComboBox(
      "Pipeline",
      scene->conf.renderPipeline.value,
      OPTIONS, 3
    );

    std::vector<ImTable::ComboEntry> fpsEntries{
      {0, "Unlimited"},
      {1, "30 / 25"},
      {2, "20 / 16.6"},
      {3, "15 / 12.5"},
    };
    ImTable::addVecComboBox("FPS-Limit", fpsEntries, scene->conf.frameLimit.value);

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

  if (ImGui::CollapsingHeader("Framebuffer", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImTable::start("Framebuffer");

    if(fbDisabled)ImGui::BeginDisabled();
    ImTable::add("Width", scene->conf.fbWidth);
    ImTable::add("Height", scene->conf.fbHeight);

    constexpr const char* const FORMATS[] = {"RGBA16","RGBA32"};
    ImTable::addComboBox("Format", scene->conf.fbFormat, FORMATS, 2);

    if(fbDisabled)ImGui::EndDisabled();
      ImTable::addColor("Color", scene->conf.clearColor.value, false);
      scene->conf.clearColor.value.a = 1.0f;
    if(fbDisabled)ImGui::BeginDisabled();

    ImTable::addProp("Clear Color", scene->conf.doClearColor);

    if(fbDisabled)ImGui::EndDisabled();

    ImTable::addProp("Clear Depth", scene->conf.doClearDepth);

    constexpr std::array<const char*, 5> FILTERS = {
      "None",
      "Resample",
      "Dedither",
      "Resample / AA",
      "Resample / AA / Dedither"
    };
    ImTable::addComboBox("Filter", scene->conf.filter.value, FILTERS.data(), FILTERS.size());

    ImTable::end();
  }

  if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen))
  {
    ImTable::start("Audio");

    ImTable::addVecComboBox<ImTable::ComboEntry>("Mixer Freq.", {
      { 8000, "8000 Hz" },
      { 11025, "11025 Hz" },
      { 16000, "16000 Hz" },
      { 22050, "22050 Hz" },
      { 32000, "32000 Hz" },
      { 44100, "44100 Hz" },
      { 48000, "48000 Hz" },
    }, scene->conf.audioFreq.value);

    ImTable::end();
  }
}
