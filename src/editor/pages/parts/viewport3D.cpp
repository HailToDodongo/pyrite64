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
#include "../../../utils/colors.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include "SDL3/SDL_gpu.h"
#include "IconsMaterialDesignIcons.h"
#include "../../undoRedo.h"
#include "../../selectionUtils.h"

namespace
{
  constinit uint32_t nextPassId{0};

  constexpr ImGuizmo::OPERATION GIZMO_OPS[3] {
    ImGuizmo::OPERATION::TRANSLATE,
    ImGuizmo::OPERATION::ROTATE,
    ImGuizmo::OPERATION::SCALE
  };
  constinit bool isTransWorld = true;
  constinit bool overRotGizmo = false;

  // A toggleable "connected" button (like in toolbars)
  bool ConnectedToggleButton(const char* text, bool active, bool first, bool last, ImVec2 size = ImVec2(20, 20))
  {
    ImGuiStyle& style = ImGui::GetStyle();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Remove spacing so buttons touch
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushID(text); // ensure unique id for InvisibleButton

    // Create an invisible button to get interaction & layout
    bool pressed = ImGui::InvisibleButton("##invis", size);
    ImGui::PopID();

    // Get item rect
    ImVec2 a = ImGui::GetItemRectMin();
    ImVec2 b = ImGui::GetItemRectMax();

    // Choose background color based on active / hovered / held
    ImU32 col;
    if (active) {
        col = ImGui::GetColorU32(ImGuiCol_ButtonActive);
    } else if (ImGui::IsItemActive()) {
        col = ImGui::GetColorU32(ImGuiCol_ButtonActive); // pressed
    } else if (ImGui::IsItemHovered()) {
        col = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
    } else {
        col = ImGui::GetColorU32(ImGuiCol_Button);
    }

    // Corner rounding amount
    float rounding = style.FrameRounding;
    if (rounding <= 0.0f) rounding = 0.0f;

    // Decide which corners to round (ImDrawFlags_RoundCornersXXX)
    ImDrawFlags round_flags = ImDrawFlags_RoundCornersNone;
    if (first && last) {
        // single button -> round all corners
        round_flags = ImDrawFlags_RoundCornersAll;
    } else if (first) {
        round_flags = (ImDrawFlags)(ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersBottomLeft);
    } else if (last) {
        round_flags = (ImDrawFlags)(ImDrawFlags_RoundCornersTopRight | ImDrawFlags_RoundCornersBottomRight);
    } else {
        round_flags = ImDrawFlags_RoundCornersNone;
    }

    // Draw filled background with chosen rounded corners
    draw_list->AddRectFilled(a, b, col, rounding, (int)round_flags);

    // Optional border
    if (style.FrameBorderSize > 0.0f) {
        ImU32 border_col = ImGui::GetColorU32(ImGuiCol_Border);
        draw_list->AddRect(a, b, border_col, rounding, (int)round_flags, style.FrameBorderSize);
    }

    // Draw the label text centered inside the rect
    ImVec2 text_size = ImGui::CalcTextSize(text);
    ImVec2 text_pos = ImVec2( (a.x + b.x - text_size.x) * 0.5f, (a.y + b.y - text_size.y) * 0.5f );

    // respect style.FramePadding vertically/horizontally if size.x == 0 (auto width)
    // but invisible button size could be 0 -> then rect height is determined by style.FramePadding
    draw_list->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), text);

    // Restore spacing style
    ImGui::PopStyleVar();

    // If not last, put next button on same line with zero spacing
    if (!last) ImGui::SameLine(0, 0);

    return pressed;
  }

  std::shared_ptr<Renderer::Texture> sprites{};
  uint32_t spritesRefCount{0};

  void iterateObjects(
    Project::Object& parent,
    std::function<void(Project::Object&, Project::Component::Entry*)> callback
  )
  {
    for(auto& child : parent.children)
    {
      if(!child->enabled)continue;

      auto srcObj = child.get();
      if(child->isPrefabInstance()) {
        auto prefab = ctx.project->getAssets().getPrefabByUUID(child->uuidPrefab.value);
        if(prefab)srcObj = &prefab->obj;
      }

      for(auto &comp : srcObj->components) {
        callback(*child, &comp);
      }
      callback(*child, nullptr);

      iterateObjects(*child, callback);
    }
  }

  void applyDeltaToChildren(
    Project::Object &obj,
    const std::unordered_map<uint64_t, glm::vec3> &relPosMap,
    const glm::mat4 &mat
  ) {
    for(auto& child : obj.children)
    {
      // if child itself is selected, skip (already transformed with it)
      if(ctx.isObjectSelected(child->uuid))continue;

      auto it = relPosMap.find(child->uuid);
      if(it == relPosMap.end())continue;
      child->pos.resolve(child->propOverrides) = mat * glm::vec4(it->second, 1.0f);
    }
  }
}

Editor::Viewport3D::Viewport3D()
{
  if(spritesRefCount == 0) {
    sprites = std::make_shared<Renderer::Texture>(ctx.gpu, "data/img/icons/sprites.png");
  }
  ++spritesRefCount;

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

  meshGrid = std::make_shared<Renderer::Mesh>();
  Utils::Mesh::generateGrid(*meshGrid, 20);
  meshGrid->recreate(*ctx.scene);
  objGrid.setMesh(meshGrid);
  objGrid.setScale(50);

  meshLines = std::make_shared<Renderer::Mesh>();
  objLines.setMesh(meshLines);

  meshSprites = std::make_shared<Renderer::Mesh>();
  objSprites.setMesh(meshSprites);

  auto &gizStyle = ImViewGuizmo::GetStyle();
  gizStyle.scale = 0.5f;
  gizStyle.circleRadius = 19.0f;
  gizStyle.labelSize = 1.9f;
  gizStyle.labelColor = IM_COL32(0,0,0,0xFF);
}

Editor::Viewport3D::~Viewport3D() {
  ctx.scene->removeRenderPass(passId);
  ctx.scene->removeCopyPass(passId);
  ctx.scene->removePostRenderCallback(passId);

  if(--spritesRefCount == 0) {
    sprites = nullptr;
  }
}

void Editor::Viewport3D::onRenderPass(SDL_GPUCommandBuffer* cmdBuff, Renderer::Scene& renderScene)
{
  if(fb.getTexture() == nullptr)return;
  meshLines->vertLines.clear();
  meshLines->indices.clear();

  meshSprites->vertLines.clear();
  meshSprites->indices.clear();

  auto scene = ctx.project->getScenes().getLoadedScene();
  if (!scene)return;

  ctx.sanitizeObjectSelection(scene);

  SDL_GPURenderPass* renderPass3D = SDL_BeginGPURenderPass(
    cmdBuff, fb.getTargetInfo(), fb.getTargetInfoCount(), &fb.getDepthTargetInfo()
  );
  renderScene.getPipeline("n64").bind(renderPass3D);

  camera.apply(uniGlobal);
  uniGlobal.screenSize = glm::vec2{(float)fb.getWidth(), (float)fb.getHeight()};
  SDL_PushGPUVertexUniformData(cmdBuff, 0, &uniGlobal, sizeof(uniGlobal));
  auto &rootObj = scene->getRootObject();

  bool hadDraw = false;
  iterateObjects(rootObj, [&](Project::Object &obj, Project::Component::Entry *comp) {
    if(!comp)
    {
      if(!hadDraw) {
        glm::u8vec4 spriteCol{0xFF, 0xFF, 0xFF, 0xFF};
        if (ctx.isObjectSelected(obj.uuid)) {
          spriteCol = Utils::Colors::kSelectionTint;
        }
        Utils::Mesh::addSprite(*getSprites(), obj.pos.resolve(obj.propOverrides), obj.uuid, 2, spriteCol);
      }
      hadDraw = false;
      return;
    }
    auto &def = Project::Component::TABLE[comp->id];

    // @TODO: use flag in component
    if(!showCollMesh && comp->id == 4)return;
    if(!showCollObj && comp->id == 5)return;

    if(def.funcDraw3D) {
      def.funcDraw3D(obj, *comp, *this, cmdBuff, renderPass3D);
      hadDraw = true;
    }
  });

  iterateObjects(rootObj, [&](Project::Object &obj, Project::Component::Entry *comp) {
    if(!comp)return;
    auto &def = Project::Component::TABLE[comp->id];

    // @TODO: use flag in component
    if(!showCollMesh && comp->id == 4)return;
    if(!showCollObj && comp->id == 5)return;

    if(def.funcDrawPost3D) {
      def.funcDrawPost3D(obj, *comp, *this, cmdBuff, renderPass3D);
    }
  });

  meshLines->recreate(renderScene);
  meshSprites->recreate(renderScene);

  renderScene.getPipeline("lines").bind(renderPass3D);
  if (showGrid) {
    objGrid.draw(renderPass3D, cmdBuff);
  }
  objLines.draw(renderPass3D, cmdBuff);

  renderScene.getPipeline("sprites").bind(renderPass3D);

  sprites->bind(renderPass3D);
  objSprites.draw(renderPass3D, cmdBuff);

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

void Editor::Viewport3D::draw()
{
  camera.update();

  auto scene = ctx.project->getScenes().getLoadedScene();
  if (!scene)return;

  ctx.scene->clearLights();
  auto &rootObj = scene->getRootObject();

  iterateObjects(rootObj, [&](Project::Object &obj, Project::Component::Entry *comp) {
    if(!comp)return;
    auto &def = Project::Component::TABLE[comp->id];
    if(def.funcUpdate)def.funcUpdate(obj, *comp);
  });

  fb.setClearColor(scene->conf.clearColor.value);

  if(pickedObjID.hasResult())
  {
    uint32_t newUUID = pickedObjID.consume();
    auto newObj = scene->getObjectByUUID(newUUID);
    if(newObj && !newObj->selectable) {
      newUUID = 0;
    }

    if (newUUID == 0) {
      if (!pickAdditive) {
        ctx.clearObjectSelection();
      }
    } else {
      if (pickAdditive) {
        ctx.toggleObjectSelection(newUUID);
      } else {
        ctx.setObjectSelection(newUUID);
      }
    }
  }
  auto obj = scene->getObjectByUUID(ctx.selObjectUUID);

  constexpr float BAR_HEIGHT = 26.0f;

  auto currSize = ImGui::GetContentRegionAvail();
  auto currPos = ImGui::GetWindowPos();
  if (currSize.x < 64)currSize.x = 64;
  if (currSize.y < 64)currSize.y = 64;
  currSize.y -= BAR_HEIGHT;

  fb.resize((int)currSize.x, (int)currSize.y);
  camera.screenSize = {currSize.x, currSize.y};

  auto &io = ImGui::GetIO();
  float deltaTime = io.DeltaTime;

  ImVec2 gizPos{currPos.x + currSize.x - 50, currPos.y + 104};

  // mouse pos
  ImVec2 screenPos = ImGui::GetCursorScreenPos();
  mousePos = {ImGui::GetMousePos().x, ImGui::GetMousePos().y};
  mousePos.x -= screenPos.x;
  mousePos.y -= vpOffsetY;

  float moveSpeed = 120.0f * deltaTime;

  bool mouseHeldLeft = ImGui::IsMouseDown(ImGuiMouseButton_Left);
  bool mouseHeldRight = ImGui::IsMouseDown(ImGuiMouseButton_Right);
  bool mouseHeldMiddle = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
  bool newMouseDown = mouseHeldLeft || mouseHeldMiddle || mouseHeldRight;
  bool isAltDown = ImGui::GetIO().KeyAlt;
  bool isShiftDown = ImGui::GetIO().KeyShift;
  if(isShiftDown)moveSpeed *= 4.0f;

  bool hasSelection = !ctx.getSelectedObjectUUIDs().empty();
  bool overGizmo = hasSelection && ImGuizmo::IsOver();

  bool leftClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  bool leftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
  bool leftReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

  if (!overGizmo && isMouseHover && leftClicked && !isAltDown && !overRotGizmo) {
    selectionPending = true;
    selectionDragging = false;
    selectionStart = mousePos;
    selectionEnd = mousePos;
  }

  if (selectionPending && leftDown) {
    selectionEnd = mousePos;
    if (!selectionDragging) {
      glm::vec2 delta = selectionEnd - selectionStart;
      if (glm::length(delta) > 4.0f) {
        selectionDragging = true;
        pickAdditive = ImGui::GetIO().KeyCtrl;
      }
    }
  }

  if (selectionPending && leftReleased) {
    bool additiveSelect = ImGui::GetIO().KeyCtrl;
    if (selectionDragging) {
      glm::vec2 rectMin = glm::min(selectionStart, selectionEnd);
      glm::vec2 rectMax = glm::max(selectionStart, selectionEnd);
      glm::vec2 viewportSize{currSize.x, currSize.y};
      rectMin = glm::clamp(rectMin, glm::vec2{0,0}, viewportSize);
      rectMax = glm::clamp(rectMax, glm::vec2{0,0}, viewportSize);

      if (!additiveSelect) {
        ctx.clearObjectSelection();
      }

      auto &rootObj = scene->getRootObject();
      glm::vec4 viewport{0.0f, 0.0f, currSize.x, currSize.y};
      iterateObjects(rootObj, [&](Project::Object &objIter, Project::Component::Entry *comp) {
        if (comp) return;
        if (!objIter.selectable) return;

        glm::vec3 worldPos = objIter.pos.resolve(objIter.propOverrides);
        glm::vec3 proj = glm::project(worldPos, uniGlobal.cameraMat, uniGlobal.projMat, viewport);
        if (proj.z < 0.0f || proj.z > 1.0f) return;

        glm::vec2 screenPos{proj.x, currSize.y - proj.y};
        if (screenPos.x >= rectMin.x && screenPos.x <= rectMax.x
            && screenPos.y >= rectMin.y && screenPos.y <= rectMax.y) {
          ctx.addObjectSelection(objIter.uuid);
        }
      });
    } else {
      pickedObjID.request();
      mousePosClick = mousePos;
      pickAdditive = additiveSelect;
    }
    selectionPending = false;
    selectionDragging = false;
  }

  if(isMouseHover)
  {
    ImGui::SetMouseCursor(
      mouseHeldRight ? ImGuiMouseCursor_None : ImGuiMouseCursor_Arrow
    );
  }

  if(!ImGui::GetIO().WantTextInput)
  {
    if(ImGui::IsKeyPressed(ctx.keymap.toggleOrtho))
    {
      camera.isOrtho = !camera.isOrtho;
    }

    // Handle object deletion when Delete is pressed while the viewport is focused and an object is selected
    bool deletedSelection = false;
    if (ImGui::IsWindowFocused() && obj && ImGui::IsKeyPressed(ctx.keymap.deleteObject)) {
      UndoRedo::getHistory().markChanged("Delete Object");
      if (Editor::SelectionUtils::deleteSelectedObjects(*scene)) {
        deletedSelection = true;
      }
      obj = nullptr;
    }

    if (deletedSelection) {
      hasSelection = false;
    }

    if (newMouseDown) {
      glm::vec3 moveDir = {0,0,0};
      if (ImGui::IsKeyDown(ctx.keymap.moveForward))moveDir.z = -moveSpeed;
      if (ImGui::IsKeyDown(ctx.keymap.moveBack))moveDir.z = moveSpeed;
      if (ImGui::IsKeyDown(ctx.keymap.moveLeft))moveDir.x = -moveSpeed;
      if (ImGui::IsKeyDown(ctx.keymap.moveRight))moveDir.x = moveSpeed;
      if (ImGui::IsKeyDown(ctx.keymap.moveDown))moveDir.y = -moveSpeed;
      if (ImGui::IsKeyDown(ctx.keymap.moveUp))moveDir.y = moveSpeed;

      if(moveDir != glm::vec3{0,0,0}) {
        camera.velocity = camera.rot * moveDir;
      }
    } else {
      if(!ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
      {
        if (ImGui::IsKeyDown(ctx.keymap.gizmoTranslate))gizmoOp = 0;
        if (ImGui::IsKeyDown(ctx.keymap.gizmoRotate))gizmoOp = 1;
        if (ImGui::IsKeyDown(ctx.keymap.gizmoScale))gizmoOp = 2;
        if (ImGui::IsKeyPressed(ctx.keymap.focusObject))camera.focusSelection(ctx);
      }
    }
  }

  if (isMouseHover && !overRotGizmo) {
    //multitouch trackpads don't generate touch or pinch events on windows
    //instead, we have to rely on the fact that trackpads move in fractional amounts
    glm::vec2 wheel = glm::vec2(io.MouseWheelH, io.MouseWheel);
    bool usesWheel = wheel != glm::vec2{0,0};

    if(usesWheel)
    {
      if (std::abs(wheel.x) == 1 || std::abs(wheel.y) == 1) {
        //actual wheel or pinch gesture
        float wheelSpeed = (isShiftDown ? 4.0f : 1.0f) * 30.0f;
        camera.zoomSpeed += wheel.y * wheelSpeed;
      } else {
        //two finger swipe on trackpad
        if (isShiftDown) {
          camera.moveDelta(wheel * -30.0f);
        } else {
          camera.orbitDelta(wheel * 10.0f);
        }
      }
    }

    if(!isMouseDown && newMouseDown) {
      mousePosStart = mousePos;
    }
    isMouseDown = newMouseDown;
  }

  currPos = ImGui::GetCursorPos();

  //ImGui::Text("Viewport: %f | %f | %08X", mousePos.x, mousePos.y, ctx.selObjectUUID);

  constexpr const char* const GIZMO_LABELS[3] = {ICON_MDI_CURSOR_MOVE, ICON_MDI_ROTATE_360, ICON_MDI_ARROW_EXPAND};
  for (int i=0; i<3; ++i) {
    if (ConnectedToggleButton(
      GIZMO_LABELS[i],
      gizmoOp == i,
      i == 0, i == 2,
      ImVec2(32,24)
    )) {
      gizmoOp = i;
    }
  }

  ImGui::SameLine();

  if (ConnectedToggleButton(ICON_MDI_WEB, isTransWorld, true, true, ImVec2(32,24))) {
    isTransWorld = !isTransWorld;
  }

  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12);

  if(ConnectedToggleButton(ICON_MDI_GRID, showGrid, true, true, ImVec2(32,24))) {
    showGrid = !showGrid;
  }

  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 4);
  if(ConnectedToggleButton(ICON_MDI_LANDSLIDE_OUTLINE, showCollMesh, true, true, ImVec2(32,24))) {
    showCollMesh = !showCollMesh;
  }

  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 4);
  if(ConnectedToggleButton(ICON_MDI_CYLINDER, showCollObj, true, true, ImVec2(32,24))) {
    showCollObj = !showCollObj;
  }

  ImGui::SetCursorPosY(currPos.y + BAR_HEIGHT);

  auto dragDelta = mousePos - mousePosStart;
  if (isMouseDown) {
    if (isAltDown && mouseHeldLeft) {
      camera.stopMoveDelta();
      camera.orbitDelta(dragDelta);
    } else if (mouseHeldMiddle) {
      camera.stopRotateDelta();
      camera.moveDelta(-dragDelta * 3.0f);
    } else if (mouseHeldRight) {
      camera.stopMoveDelta();
      camera.lookDelta(dragDelta);
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

  if (ImGui::BeginDragDropTarget())
  {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET"))
    {
      uint64_t prefabUUID = *((uint64_t*)payload->Data);
      auto prefab = ctx.project->getAssets().getPrefabByUUID(prefabUUID);
      if(prefab) {
        UndoRedo::getHistory().markChanged("Add Prefab");
        auto newObj = scene->addPrefabInstance(prefabUUID);
        if (newObj) {
          // place in front of camera view
          glm::vec3 camForward = camera.rot * glm::vec3{0,0,-1};
          glm::vec3 camPos = camera.pos;
          newObj->pos.resolve(newObj->propOverrides) = camPos + camForward * 150.0f;

          ctx.setObjectSelection(newObj->uuid);
        }
      }
    }
    ImGui::EndDragDropTarget();
  }

  isMouseHover = ImGui::IsItemHovered();

  if (selectionDragging) {
    glm::vec2 rectMin = glm::min(selectionStart, selectionEnd);
    glm::vec2 rectMax = glm::max(selectionStart, selectionEnd);
    glm::vec2 viewportSize{currSize.x, currSize.y};

    rectMin = glm::clamp(rectMin, glm::vec2{0,0}, viewportSize);
    rectMax = glm::clamp(rectMax, glm::vec2{0,0}, viewportSize);

    ImVec2 rectStartScreen{currPos.x + rectMin.x, currPos.y + rectMin.y};
    ImVec2 rectEndScreen{currPos.x + rectMax.x, currPos.y + rectMax.y};
    auto drawList = ImGui::GetWindowDrawList();
    ImU32 fillCol = ImGui::GetColorU32(ImGuiCol_DragDropTarget, 0.15f);
    ImU32 borderCol = ImGui::GetColorU32(ImGuiCol_DragDropTarget, 0.85f);
    drawList->AddRectFilled(rectStartScreen, rectEndScreen, fillCol);
    drawList->AddRect(rectStartScreen, rectEndScreen, borderCol, 0.0f, 0, 1.5f);
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  ImGuizmo::SetDrawlist(draw_list);
  ImGuizmo::SetRect(currPos.x, currPos.y, currSize.x, currSize.y);

  if (hasSelection) {
    auto selectedObjects = Editor::SelectionUtils::collectSelectedObjects(*scene);
    if (!selectedObjects.empty()) {
      obj = scene->getObjectByUUID(selectedObjects.back()->uuid);

      glm::mat4 gizmoMat{};
      glm::vec3 skew{0,0,0};
      glm::vec4 persp{0,0,0,1};

      bool isMultiSelect = selectedObjects.size() > 1;
      bool isOverride = false;

      glm::vec3 center{0.0f, 0.0f, 0.0f};
      if (!isMultiSelect) {
        gizmoMat = glm::recompose(
          obj->scale.resolve(obj->propOverrides, &isOverride),
          obj->rot.resolve(obj->propOverrides),
          obj->pos.resolve(obj->propOverrides),
          skew, persp);
      } else {
        for (auto *selObj : selectedObjects) {
          center += selObj->pos.resolve(selObj->propOverrides);
        }
        center /= (float)selectedObjects.size();
        gizmoMat = glm::recompose(
          glm::vec3{1.0f},
          glm::quat{1,0,0,0},
          center,
          skew,
          persp
        );
      }

      glm::mat4 oldGizmoMat = gizmoMat;

      glm::vec3 snap(10.0f);
      if (gizmoOp == 1) { // rotate
        snap = glm::vec3(90.0f / 4.0f);
      } else if (gizmoOp == 2) { // scale
        snap = glm::vec3(0.125f);
      }
      bool isSnap = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
      bool isOnlySelf = ImGui::IsKeyDown(ImGuiKey_LeftShift);

      // snap object to absolute grid
      if(ImGui::IsKeyDown(ImGuiKey_LeftShift) && ImGui::IsKeyPressed(ctx.keymap.snapObject))
      {
        glm::vec3 pos = obj->pos.resolve(obj->propOverrides);
        pos.x = std::round(pos.x / snap.x) * snap.x;
        pos.y = std::round(pos.y / snap.y) * snap.y;
        pos.z = std::round(pos.z / snap.z) * snap.z;
        obj->pos.resolve(obj->propOverrides) = pos;
      }

      if(ImGuizmo::Manipulate(
        glm::value_ptr(uniGlobal.cameraMat),
        glm::value_ptr(uniGlobal.projMat),
        GIZMO_OPS[gizmoOp],
        isTransWorld ? ImGuizmo::MODE::WORLD : ImGuizmo::MODE::LOCAL,
        glm::value_ptr(gizmoMat),
        nullptr,
        isSnap ? glm::value_ptr(snap) : nullptr
      )) {
        gizmoTransformActive = true;

        auto ensureOverride = [](Project::Object *selObj, auto &prop) {
          if (selObj->propOverrides.find(prop.id) == selObj->propOverrides.end()) {
            selObj->addPropOverride(prop);
          }
        };

        if (!isMultiSelect) {
          if(!obj->uuidPrefab.value || isOverride)
          {
            std::unordered_map<uint64_t, glm::vec3> relPosMap{};
            if(!isOnlySelf)
            {
              auto oldObjMat = glm::recompose(
                obj->scale.resolve(obj->propOverrides),
                obj->rot.resolve(obj->propOverrides),
                obj->pos.resolve(obj->propOverrides),
                skew, persp);

              for(auto& child : obj->children)
              {
                relPosMap[child->uuid] = glm::inverse(oldObjMat) * glm::vec4(
                  child->pos.resolve(child->propOverrides), 1.0f
                );
              }
            }

            glm::decompose(
              gizmoMat,
              obj->scale.resolve(obj->propOverrides),
              obj->rot.resolve(obj->propOverrides),
              obj->pos.resolve(obj->propOverrides),
              skew, persp
            );

            if(!isOnlySelf)
            {
              applyDeltaToChildren(*obj, relPosMap, gizmoMat);
            }
          }
        } else {
          auto deltaMat = gizmoMat * glm::inverse(oldGizmoMat);

          if (gizmoOp == 2) {
            glm::vec3 gizScaleOld{1.0f};
            glm::vec3 gizScaleNew{1.0f};
            glm::vec3 gizPosOld{0.0f};
            glm::vec3 gizPosNew{0.0f};
            glm::quat gizRotOld{};
            glm::quat gizRotNew{};
            glm::vec3 gizSkew{0.0f};
            glm::vec4 gizPersp{0.0f, 0.0f, 0.0f, 1.0f};

            glm::decompose(oldGizmoMat, gizScaleOld, gizRotOld, gizPosOld, gizSkew, gizPersp);
            glm::decompose(gizmoMat, gizScaleNew, gizRotNew, gizPosNew, gizSkew, gizPersp);

            auto safeDiv = [](float a, float b) {
              return (std::abs(b) > 0.000001f) ? (a / b) : 1.0f;
            };
            glm::vec3 scaleDelta{
              safeDiv(gizScaleNew.x, gizScaleOld.x),
              safeDiv(gizScaleNew.y, gizScaleOld.y),
              safeDiv(gizScaleNew.z, gizScaleOld.z)
            };

            for (auto *selObj : selectedObjects) {
              if (selObj->isPrefabInstance() && !selObj->isPrefabEdit) {
                ensureOverride(selObj, selObj->pos);
                ensureOverride(selObj, selObj->rot);
                ensureOverride(selObj, selObj->scale);
              }

              auto &objPos = selObj->pos.resolve(selObj->propOverrides);
              auto &objScale = selObj->scale.resolve(selObj->propOverrides);
              auto &objRot = selObj->rot.resolve(selObj->propOverrides);

              glm::vec3 oldPos = objPos;
              glm::vec3 oldScale = objScale;
              glm::quat oldRot = objRot;

              std::unordered_map<uint64_t, glm::vec3> relPosMap{};
              if(!isOnlySelf)
              {
                auto oldObjMat = glm::recompose(oldScale, oldRot, oldPos, skew, persp);

                for(auto& child : selObj->children)
                {
                  relPosMap[child->uuid] = glm::inverse(oldObjMat) * glm::vec4(
                    child->pos.resolve(child->propOverrides), 1.0f
                  );
                }
              }

              objPos = center + ((oldPos - center) * scaleDelta);
              objScale = oldScale * scaleDelta;

              if(!isOnlySelf)
              {
                auto newObjMat = glm::recompose(objScale, objRot, objPos, skew, persp);
                applyDeltaToChildren(*selObj, relPosMap, newObjMat);
              }
            }
          } else {
            for (auto *selObj : selectedObjects) {
              if (selObj->isPrefabInstance() && !selObj->isPrefabEdit) {
                ensureOverride(selObj, selObj->pos);
                ensureOverride(selObj, selObj->rot);
                ensureOverride(selObj, selObj->scale);
              }

              std::unordered_map<uint64_t, glm::vec3> relPosMap{};
              if(!isOnlySelf)
              {
                auto oldObjMat = glm::recompose(
                  selObj->scale.resolve(selObj->propOverrides),
                  selObj->rot.resolve(selObj->propOverrides),
                  selObj->pos.resolve(selObj->propOverrides),
                  skew, persp);

                for(auto& child : selObj->children)
                {
                  relPosMap[child->uuid] = glm::inverse(oldObjMat) * glm::vec4(
                    child->pos.resolve(child->propOverrides), 1.0f
                  );
                }
              }

              auto oldObjMat = glm::recompose(
                selObj->scale.resolve(selObj->propOverrides),
                selObj->rot.resolve(selObj->propOverrides),
                selObj->pos.resolve(selObj->propOverrides),
                skew, persp);
              auto newObjMat = deltaMat * oldObjMat;

              glm::decompose(
                newObjMat,
                selObj->scale.resolve(selObj->propOverrides),
                selObj->rot.resolve(selObj->propOverrides),
                selObj->pos.resolve(selObj->propOverrides),
                skew, persp
              );

              if(!isOnlySelf) {
                applyDeltaToChildren(*selObj, relPosMap, newObjMat);
              }
            }
          }
        }
      }
    }
  }

  // If the gizmo was active but is no longer being used, end the transform snapshot
  if (gizmoTransformActive && (!ImGuizmo::IsUsing() || !obj)) {
    UndoRedo::getHistory().markChanged("Transform Object");
    gizmoTransformActive = false;
  }

  glm::vec3 posOffset = camera.pos - camera.pivot;
  float camDist = glm::length(posOffset);
  if (ImViewGuizmo::Rotate(posOffset, camera.rot, gizPos, camDist)) {
    camera.pos = camera.pivot + posOffset;
  }
  overRotGizmo = ImViewGuizmo::IsOver();
}
