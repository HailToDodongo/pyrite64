/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "editorMain.h"

#include <cstdio>

#include "imgui.h"

namespace
{
  bool show_demo_window{};
  bool show_another_window{};
  ImVec4 clear_color{0.45f, 0.55f, 0.60f, 1.00f};
  int counter{0};
}

Editor::Main::Main(SDL_GPUDevice* device)
  : texTest{device, "data/img/titleLogo.png"}
{
}

Editor::Main::~Main() {
}

void Editor::Main::draw()
{
  auto &io = ImGui::GetIO();

  ImGui::SetNextWindowPos({0,0}, ImGuiCond_Appearing, {0.0f, 0.0f});
  ImGui::SetNextWindowSize({io.DisplaySize.x, io.DisplaySize.y}, ImGuiCond_Always);
  ImGui::Begin("WIN_MAIN", 0,
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar
    | ImGuiWindowFlags_NoBackground
  );

  ImGui::Text("Pyrite64 [v0.0.0-alpha]");               // Display some text (you can use a format strings too)

  auto logoSize = ImVec2(texTest.getWidth(), texTest.getHeight());
  logoSize.x *= 0.75f;
  logoSize.y *= 0.75f;

  ImGui::SetCursorPosX(io.DisplaySize.x / 2 - (logoSize.x/2));
  ImGui::SetCursorPosY(100);
  ImGui::Image(ImTextureID(texTest.getGPUTex()),logoSize);

  ImGui::End();
}
