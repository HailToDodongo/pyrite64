/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "viewport3D.h"

#include "imgui.h"
#include "ImGuizmo.h"
#include "ImViewGuizmo.h"
#include "../../../context.h"
#include "../../../renderer/mesh.h"
#include "../../../renderer/object.h"
#include "../../../renderer/scene.h"
#include "../../../renderer/uniforms.h"
#include "../../../utils/meshGen.h"
#include "SDL3/SDL_gpu.h"

namespace
{
  constinit uint32_t nextPassId{0};
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

  mesh = std::make_shared<Renderer::Mesh>();
  mesh->vertices.clear();
  Utils::Mesh::generateCube(*mesh);
  mesh->recreate(*ctx.scene);
  obj.setMesh(mesh);
  obj2.setMesh(mesh);

  obj2.setPos({3,0,1});

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
  SDL_GPURenderPass* renderPass3D = SDL_BeginGPURenderPass(
    cmdBuff, &fb.getTargetInfo(), 1, &fb.getDepthTargetInfo()
  );
  SDL_BindGPUGraphicsPipeline(renderPass3D, pipeline);

  camera.apply(uniGlobal);
  SDL_PushGPUVertexUniformData(cmdBuff, 0, &uniGlobal, sizeof(uniGlobal));

  obj.draw(renderPass3D, cmdBuff);
  obj2.draw(renderPass3D, cmdBuff);

  SDL_EndGPURenderPass(renderPass3D);
}

void Editor::Viewport3D::onCopyPass(SDL_GPUCommandBuffer* cmdBuff, SDL_GPUCopyPass *copyPass) {
  //vertBuff->upload(*copyPass);
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

  currPos = ImGui::GetCursorScreenPos();

  ImGui::Image(ImTextureID(fb.getTexture()), {currSize.x, currSize.y});
  isMouseHover = ImGui::IsItemHovered();

  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  glm::mat4 unit = glm::mat4(1.0f);
  ImGuizmo::SetDrawlist(draw_list);
  ImGuizmo::SetRect(currPos.x, currPos.y, currSize.x, currSize.y);
  ImGuizmo::DrawGrid(
    glm::value_ptr(uniGlobal.cameraMat),
    glm::value_ptr(uniGlobal.projMat),
    glm::value_ptr(unit),
    10.0f
  );

  if (ImViewGuizmo::Rotate(camera.posOffset, camera.rot, gizPos)) {

  }
}
