/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "editorScene.h"

#include <cstdio>

#include "imgui.h"

namespace
{
  bool show_demo_window{};
  bool show_another_window{};
  ImVec4 clear_color{0.45f, 0.55f, 0.60f, 1.00f};
  int counter{0};
}

void Editor::Scene::draw()
{
  auto &io = ImGui::GetIO();

  float offsetY = 20;
  ImGui::SetNextWindowPos({0,offsetY}, ImGuiCond_Appearing, {0.0f, 0.0f});
  ImGui::SetNextWindowSize({300, io.DisplaySize.y-offsetY}, ImGuiCond_Always);
  ImGui::Begin("Hello, world!", 0,
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar
  );

  ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
  ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
  ImGui::Checkbox("Another Window", &show_another_window);

  //ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
  ImGui::ColorEdit3("Clear-color", (float*)&clear_color, ImGuiColorEditFlags_NoInputs); // Edit 3 floats representing a color

  if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
    counter++;
  ImGui::SameLine();
  ImGui::Text("counter = %d", counter);

  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
  ImGui::End();

  // Top bar
  ImGui::SetNextWindowPos({0,0}, ImGuiCond_Appearing, {0.0f, 0.0f});
  //ImGui::SetNextWindowSize({io.DisplaySize.x, 32}, ImGuiCond_Always);
  ImGui::Begin("TOP_BAR", 0,
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar
    | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBackground
  );

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File"))
    {
      if (ImGui::MenuItem("Open Project")) {
        std::printf("Open Project\n");
      }
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }
  ImGui::End();
}
