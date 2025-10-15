/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "viewport3D.h"

#include "imgui.h"
#include "ImViewGuizmo.h"
#include "../../../context.h"
#include "../../../renderer/scene.h"
#include "../../../renderer/uniforms.h"
#include "SDL3/SDL_gpu.h"

namespace
{
  constinit uint32_t nextPassId{0};

  std::vector<Renderer::Vertex> vertices{};
  Renderer::VertBuffer *vertBuff{nullptr};

  Renderer::UniformsObject uniObj{};


static bool ProjectClippedLineToScreen(
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::mat4& view,
    const glm::mat4& proj,
    ImVec2& outA,
    ImVec2& outB,
    const ImVec2& viewport_size)
{
    glm::vec4 ca = proj * view * glm::vec4(a, 1.0f);
    glm::vec4 cb = proj * view * glm::vec4(b, 1.0f);

    if (ca.w <= 0.0f && cb.w <= 0.0f)
        return false;

    // Clip against near plane
    if (ca.w <= 0.0f || cb.w <= 0.0f)
    {
        glm::vec4 da = cb - ca;
        float t = (0.0001f - ca.w) / (da.w);
        ca = ca + da * t;
        if (ca.w <= 0.0f) return false;
    }

    glm::vec3 na = glm::vec3(ca) / ca.w;
    glm::vec3 nb = glm::vec3(cb) / cb.w;

    outA.x = (na.x * 0.5f + 0.5f) * viewport_size.x;
    outA.y = (-na.y * 0.5f + 0.5f) * viewport_size.y;
    outB.x = (nb.x * 0.5f + 0.5f) * viewport_size.x;
    outB.y = (-nb.y * 0.5f + 0.5f) * viewport_size.y;
    return true;
}

void Draw3DGridAndAxes(
    ImDrawList* draw_list,
    const glm::mat4& view,
    const glm::mat4& proj,
    const ImVec2& viewport_size,
    ImVec2 screen_offset = ImVec2(0, 0),
    float grid_size = 10.0f,
    int grid_lines = 20,
    float line_thickness = 1.0f
)
{
    if (!draw_list) return;

    ImU32 grid_color = IM_COL32(180, 180, 180, 100);
    ImU32 x_color = IM_COL32(255, 80, 80, 255);
    ImU32 y_color = IM_COL32(80, 255, 80, 255);
    ImU32 z_color = IM_COL32(80, 80, 255, 255);

    float half = grid_size * 0.5f;
    float step = grid_size / float(grid_lines);

    ImVec2 s1, s2;

    // --- Grid (XZ plane)
    for (int i = 0; i <= grid_lines; i++)
    {
        float x = -half + i * step;
        glm::vec3 p1(x, 0, -half), p2(x, 0, half);
        if (ProjectClippedLineToScreen(p1, p2, view, proj, s1, s2, viewport_size))
            draw_list->AddLine({s1.x + screen_offset.x, s1.y + screen_offset.y}, {s2.x + screen_offset.x, s2.y + screen_offset.y}, grid_color, line_thickness);

        float z = -half + i * step;
        glm::vec3 p3(-half, 0, z), p4(half, 0, z);
        if (ProjectClippedLineToScreen(p3, p4, view, proj, s1, s2, viewport_size))
            draw_list->AddLine({s1.x + screen_offset.x, s1.y + screen_offset.y}, {s2.x + screen_offset.x, s2.y + screen_offset.y}, grid_color, line_thickness);
    }

    // --- Axes
    auto DrawAxis = [&](glm::vec3 a, glm::vec3 b, ImU32 color)
    {
        ImVec2 s1, s2;
        if (ProjectClippedLineToScreen(a, b, view, proj, s1, s2, viewport_size))
            draw_list->AddLine({s1.x + screen_offset.x, s1.y + screen_offset.y}, {s2.x + screen_offset.x, s2.y + screen_offset.y}, color, line_thickness * 2.0f);
    };

    float axis_len = grid_size * 0.6f;
    DrawAxis(glm::vec3(0), glm::vec3(axis_len, 0, 0), x_color);
    DrawAxis(glm::vec3(0), glm::vec3(0, axis_len, 0), y_color);
    DrawAxis(glm::vec3(0), glm::vec3(0, 0, axis_len), z_color);
}
}

Editor::Viewport3D::Viewport3D()
{
  passId = ++nextPassId;
  ctx.scene->addRenderPass(passId, [this](SDL_GPUCommandBuffer* cmdBuff, SDL_GPUGraphicsPipeline* pipeline) {
    onRenderPass(cmdBuff, pipeline);
  });
  ctx.scene->addCopyPass(passId, [this](SDL_GPUCommandBuffer* cmdBuff, SDL_GPUCopyPass *copyPass) {
    onCopyPass(cmdBuff, copyPass);
  });

  vertices.clear();
  // cube:

  // large floor
  vertices.push_back({{-10,0,-10}, {0,1,0}, {1,0,0,1}, {0,0}});
  vertices.push_back({{ 10,0, 10}, {0,1,0}, {0,1,0,1}, {1,1}});
  vertices.push_back({{ 10,0,-10}, {0,1,0}, {0,0,1,1}, {1,0}});
  vertices.push_back({{-10,0,-10}, {0,1,0}, {1,0,0,1}, {0,0}});
  vertices.push_back({{-10,0, 10}, {0,1,0}, {1,1,0,1}, {0,1}});
  vertices.push_back({{ 10,0, 10}, {0,1,0}, {0,1,0,1}, {1,1}});

  vertices.push_back({{-1,-1, -1}, {0,0,-1}, {1,0,0,1}, {0,0}});
  vertices.push_back({{ 1, 1, -1}, {0,0,-1}, {0,1,0,1}, {1,1}});
  vertices.push_back({{ 1,-1, -1}, {0,0,-1}, {0,0,1,1}, {1,0}});
  vertices.push_back({{-1,-1, -1}, {0,0,-1}, {1,0,0,1}, {0,0}});
  vertices.push_back({{-1, 1, -1}, {0,0,-1}, {1,1,0,1}, {0,1}});
  vertices.push_back({{ 1, 1, -1}, {0,0,-1}, {0,1,0,1}, {1,1}});

  vertices.push_back({{-1,-1, 1}, {0,0,-1}, {1,0,0,1}, {0,0}});
  vertices.push_back({{ 1, 1, 1}, {0,0,-1}, {0,1,0,1}, {1,1}});
  vertices.push_back({{ 1,-1, 1}, {0,0,-1}, {0,0,1,1}, {1,0}});
  vertices.push_back({{-1,-1, 1}, {0,0,-1}, {1,0,0,1}, {0,0}});
  vertices.push_back({{-1, 1, 1}, {0,0,-1}, {1,1,0,1}, {0,1}});
  vertices.push_back({{ 1, 1, 1}, {0,0,-1}, {0,1,0,1}, {1,1}});


  vertBuff = new Renderer::VertBuffer({sizeof(vertices), ctx.gpu});
  vertBuff->setData(vertices);

  auto &gizStyle = ImViewGuizmo::GetStyle();
  gizStyle.scale = 0.5f;
  gizStyle.circleRadius = 19.0f;
  gizStyle.labelSize = 1.9f;
  gizStyle.labelColor = IM_COL32(0,0,0,0xFF);

  camera.pos = {0,0,0};
}

Editor::Viewport3D::~Viewport3D() {
  ctx.scene->removeRenderPass(passId);
  ctx.scene->removeCopyPass(passId);
}

void Editor::Viewport3D::onRenderPass(SDL_GPUCommandBuffer* cmdBuff, SDL_GPUGraphicsPipeline* pipeline)
{
  SDL_GPURenderPass* renderPass3D = SDL_BeginGPURenderPass(cmdBuff, &fb.getTargetInfo(), 1, nullptr);
  SDL_BindGPUGraphicsPipeline(renderPass3D, pipeline);

  camera.apply(uniGlobal);
  SDL_PushGPUVertexUniformData(cmdBuff, 0, &uniGlobal, sizeof(uniGlobal));

  //uniObj.modelMat = glm::mat4(1.0f);
  uniObj.modelMat = glm::scale(glm::mat4(1.0f), {0.1f, 0.1f, 0.1f});
  SDL_PushGPUVertexUniformData(cmdBuff, 1, &uniObj, sizeof(uniObj));


  // bind the vertex buffer
  SDL_GPUBufferBinding bufferBindings[1];
  vertBuff->addBinding(bufferBindings[0]);
  SDL_BindGPUVertexBuffers(renderPass3D, 0, bufferBindings, 1); // bind one buffer starting from slot 0

  //SDL_SetGPUScissor(renderPass, &scissor3D);
  SDL_DrawGPUPrimitives(renderPass3D, vertices.size(), 1, 0, 0);

  SDL_EndGPURenderPass(renderPass3D);
}

void Editor::Viewport3D::onCopyPass(SDL_GPUCommandBuffer* cmdBuff, SDL_GPUCopyPass *copyPass) {
  vertBuff->upload(*copyPass);
}

void Editor::Viewport3D::draw() {
  camera.update();

  auto currSize = ImGui::GetContentRegionAvail();
  auto currPos = ImGui::GetWindowPos();
  if (currSize.x < 64)currSize.x = 64;
  if (currSize.y < 64)currSize.y = 64;
  currSize.y -= 24;

  fb.resize((int)currSize.x, (int)currSize.y);
  camera.screenSize = {currSize.x, currSize.y};

  ImVec2 gizPos{currPos.x + currSize.x - 40, currPos.y + 104};

  // mouse pos
  ImVec2 screenPos = ImGui::GetCursorScreenPos();
  mousePos = {ImGui::GetMousePos().x, ImGui::GetMousePos().y};
  mousePos.x -= screenPos.x;
  mousePos.y -= screenPos.y - 20;

  bool newMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle) || ImGui::IsMouseDown(ImGuiMouseButton_Right);
  bool isShiftDown = ImGui::GetIO().KeyShift;

  if (ImGui::GetIO().KeysData[ImGuiKey_W-ImGuiKey_NamedKey_BEGIN].Down) {
    camera.pos += camera.rot * glm::vec3(0,0,-0.1f);
  }
  if (ImGui::GetIO().KeysData[ImGuiKey_S-ImGuiKey_NamedKey_BEGIN].Down) {
    camera.pos += camera.rot * glm::vec3(0,0,0.1f);
  }
  if (ImGui::GetIO().KeysData[ImGuiKey_A-ImGuiKey_NamedKey_BEGIN].Down) {
    camera.pos += camera.rot * glm::vec3(-0.1f,0,0);
  }
  if (ImGui::GetIO().KeysData[ImGuiKey_D-ImGuiKey_NamedKey_BEGIN].Down) {
    camera.pos += camera.rot * glm::vec3(0.1f,0,0);
  }
  if (ImGui::GetIO().KeysData[ImGuiKey_Q-ImGuiKey_NamedKey_BEGIN].Down) {
    camera.pos += camera.rot * glm::vec3(0,-0.1f,0);
  }
  if (ImGui::GetIO().KeysData[ImGuiKey_E-ImGuiKey_NamedKey_BEGIN].Down) {
    camera.pos += camera.rot * glm::vec3(0,0.1f,0);
  }

  if (isMouseHover && !ImViewGuizmo::IsOver())
  {
    if(!isMouseDown && newMouseDown) {
      mousePosStart = mousePos;
    }
    isMouseDown = newMouseDown;
  }
  ImGui::Text("Viewport: %f | %f | %d", mousePos.x, mousePos.y, isShiftDown);

  auto dragDelta = mousePos - mousePosStart;
  if (isMouseDown) {
    if (isShiftDown) {
      camera.stopRotateDelta();
      camera.moveDelta(dragDelta);
    } else {
      camera.stopMoveDelta();
      camera.rotateDelta(dragDelta);
    }
  } else {
    camera.stopRotateDelta();
    camera.stopMoveDelta();
    mousePosStart = mousePos = {0,0};
  }
  if (!newMouseDown)isMouseDown = false;

  ImGui::Image(ImTextureID(fb.getTexture()), {currSize.x, currSize.y});
  isMouseHover = ImGui::IsItemHovered();

  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // Assume you have glm::mat4 view, proj from your camera
  currPos.y += 50;
  currPos.x += 8;
  Draw3DGridAndAxes(draw_list, uniGlobal.cameraMat, uniGlobal.projMat, currSize, currPos);

  if (ImViewGuizmo::Rotate(camera.posOffset, camera.rot, gizPos)) {

  }
}
