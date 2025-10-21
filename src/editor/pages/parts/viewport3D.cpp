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
#include "glm/gtx/matrix_decompose.hpp"
#include "SDL3/SDL_gpu.h"

namespace
{
  constinit uint32_t nextPassId{0};
}

Editor::Viewport3D::Viewport3D()
{
  passId = ++nextPassId;
  ctx.scene->addRenderPass(passId, [this](SDL_GPUCommandBuffer* cmdBuff, Renderer::Scene& renderScene) {
    onRenderPass(cmdBuff, renderScene);
  });
  ctx.scene->addCopyPass(passId, [this](SDL_GPUCommandBuffer* cmdBuff, SDL_GPUCopyPass *copyPass) {
    onCopyPass(cmdBuff, copyPass);
  });
  ctx.scene->addPostRenderCallback(passId, [this](Renderer::Scene& renderScene) {
    onPostRender(renderScene);
  });

  auto meshAsset = ctx.project->getAssets().getByName("model.glb");
  meshAsset->mesh3D->recreate(*ctx.scene);

  auto meshAssetB = ctx.project->getAssets().getByName("box.glb");
  meshAssetB->mesh3D->recreate(*ctx.scene);

  meshGrid = std::make_shared<Renderer::Mesh>();
  Utils::Mesh::generateGrid(*meshGrid, 10);
  meshGrid->recreate(*ctx.scene);
  objGrid.setMesh(meshGrid);

  meshLines = std::make_shared<Renderer::Mesh>();
  objLines.setMesh(meshLines);

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
  ctx.scene->removePostRenderCallback(passId);
}

void Editor::Viewport3D::onRenderPass(SDL_GPUCommandBuffer* cmdBuff, Renderer::Scene& renderScene)
{
  meshLines->vertLines.clear();
  meshLines->indices.clear();

  SDL_GPURenderPass* renderPass3D = SDL_BeginGPURenderPass(
    cmdBuff, fb.getTargetInfo(), fb.getTargetInfoCount(), &fb.getDepthTargetInfo()
  );
  renderScene.getPipeline("n64").bind(renderPass3D);

  camera.apply(uniGlobal);
  SDL_PushGPUVertexUniformData(cmdBuff, 0, &uniGlobal, sizeof(uniGlobal));

  auto scene = ctx.project->getScenes().getLoadedScene();
  auto &rootObj = scene->getRootObject();
  for(auto& child : rootObj.children)
  {
    for(auto &comp : child->components)
    {
      auto &def = Project::Component::TABLE[comp.id];
      if (def.funcDraw3D) {
        def.funcDraw3D(*child, comp, *this, cmdBuff, renderPass3D);
      }
    }
    //child.draw(renderPass3D, cmdBuff);
  }

  meshLines->recreate(renderScene);

  renderScene.getPipeline("lines").bind(renderPass3D);
  objGrid.draw(renderPass3D, cmdBuff);
  objLines.draw(renderPass3D, cmdBuff);

  SDL_EndGPURenderPass(renderPass3D);
}

void Editor::Viewport3D::onCopyPass(SDL_GPUCommandBuffer* cmdBuff, SDL_GPUCopyPass *copyPass) {
  //vertBuff->upload(*copyPass);
}

void Editor::Viewport3D::onPostRender(Renderer::Scene &renderScene) {
  if (pickedObjID.isRequested()) {
    pickedObjID.setResult(fb.readObjectID(mousePosClick.x, mousePosClick.y));
  }
}

void Editor::Viewport3D::draw() {
  camera.update();
  auto scene = ctx.project->getScenes().getLoadedScene();
  fb.setClearColor(scene->conf.clearColor);

  if (pickedObjID.hasResult()) {
    ctx.selObjectUUID = pickedObjID.consume();
  }

  auto currSize = ImGui::GetContentRegionAvail();
  auto currPos = ImGui::GetWindowPos();
  if (currSize.x < 64)currSize.x = 64;
  if (currSize.y < 64)currSize.y = 64;
  currSize.y -= 24;

  fb.resize((int)currSize.x, (int)currSize.y);
  camera.screenSize = {currSize.x, currSize.y};

  auto &io = ImGui::GetIO();
  float deltaTime = io.DeltaTime;

  ImVec2 gizPos{currPos.x + currSize.x - 40, currPos.y + 104};

  // mouse pos
  ImVec2 screenPos = ImGui::GetCursorScreenPos();
  mousePos = {ImGui::GetMousePos().x, ImGui::GetMousePos().y};
  mousePos.x -= screenPos.x;
  mousePos.y -= vpOffsetY;

  float moveSpeed = 2.5f * deltaTime;

  bool mouseHeldRight = ImGui::IsMouseDown(ImGuiMouseButton_Right);
  bool newMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle) || mouseHeldRight;
  bool isShiftDown = ImGui::GetIO().KeyShift;

  if (!ImGuizmo::IsOver() && isMouseHover && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    pickedObjID.request();
    mousePosClick = mousePos;
  }

  ImGui::SetMouseCursor(
    mouseHeldRight ? ImGuiMouseCursor_None : ImGuiMouseCursor_Arrow
  );

  if (newMouseDown) {
    if (ImGui::IsKeyDown(ImGuiKey_W))camera.pos += camera.rot * glm::vec3(0,0,-moveSpeed);
    if (ImGui::IsKeyDown(ImGuiKey_S))camera.pos += camera.rot * glm::vec3(0,0,moveSpeed);
    if (ImGui::IsKeyDown(ImGuiKey_A))camera.pos += camera.rot * glm::vec3(-moveSpeed,0,0);
    if (ImGui::IsKeyDown(ImGuiKey_D))camera.pos += camera.rot * glm::vec3(moveSpeed,0,0);

    if (ImGui::IsKeyDown(ImGuiKey_Q))camera.pos += camera.rot * glm::vec3(0,-moveSpeed,0);
    if (ImGui::IsKeyDown(ImGuiKey_E))camera.pos += camera.rot * glm::vec3(0,moveSpeed,0);
  }

  if (isMouseHover && !ImViewGuizmo::IsOver())
  {
    if(!isMouseDown && newMouseDown) {
      mousePosStart = mousePos;
    }
    isMouseDown = newMouseDown;
  }
  ImGui::Text("Viewport: %f | %f | %08X", mousePos.x, mousePos.y, pickedObjID);

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
  vpOffsetY = currPos.y;

  ImGui::Image(ImTextureID(fb.getTexture()), {currSize.x, currSize.y});

  isMouseHover = ImGui::IsItemHovered();

  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  glm::mat4 unit = glm::mat4(1.0f);
  ImGuizmo::SetDrawlist(draw_list);
  ImGuizmo::SetRect(currPos.x, currPos.y, currSize.x, currSize.y);

  auto obj = scene->getObjectByUUID(ctx.selObjectUUID);
  if (obj) {
    glm::mat4 gizmoMat{};
    glm::vec3 skew{0,0,0};
    glm::vec4 persp{0,0,0,1};

    gizmoMat = glm::recompose(obj->scale, obj->rot, obj->pos, skew, persp);

    ImGuizmo::Manipulate(
      glm::value_ptr(uniGlobal.cameraMat),
      glm::value_ptr(uniGlobal.projMat),
      ImGuizmo::OPERATION::TRANSLATE,
      ImGuizmo::MODE::WORLD,
      glm::value_ptr(gizmoMat)
    );

    /*ImGuizmo::DecomposeMatrixToComponents(
      glm::value_ptr(gizmoMat),
      glm::value_ptr(obj->pos),
      glm::value_ptr(dummyRot),
      glm::value_ptr(obj->scale)
    );*/
    glm::decompose(
      gizmoMat,
      obj->scale,
      obj->rot,
      obj->pos,
      skew, persp
    );
  }

  /*ImGuizmo::DrawGrid(
    glm::value_ptr(uniGlobal.cameraMat),
    glm::value_ptr(uniGlobal.projMat),
    glm::value_ptr(unit),
    10.0f
  );*/

  if (ImViewGuizmo::Rotate(camera.posOffset, camera.rot, gizPos)) {

  }
}
