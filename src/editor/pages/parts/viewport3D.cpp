/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "viewport3D.h"

#include "imgui.h"
#include "../../imgui/theme.h"
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

#include "../../../utils/logger.h"
#include <memory>
namespace
{
  constinit uint32_t nextPassId{0};

  constexpr ImGuizmo::OPERATION GIZMO_OPS[3] {
    ImGuizmo::OPERATION::TRANSLATE,
    ImGuizmo::OPERATION::ROTATE,
    ImGuizmo::OPERATION::SCALE
  };
  constinit bool isTransWorld = true;

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

  // World transform for expanded prefab-instance children. The engine has no runtime
  // transform hierarchy, so the editor composes nested transforms here to preview them.
  struct EditorWorldTrans {
    glm::vec3 pos{0,0,0};
    glm::quat rot{glm::vec3(0.0f)}; // {1,0,0,0} would set x=1, not identity
    glm::vec3 scale{1,1,1};
  };

  // RAII used while drawing one nested prefab node. Component draws read obj.pos/rot/scale
  // and obj.uuid directly, so we temporarily write the node's world transform and a pick id,
  // then restore them on exit. fb() returns the actual transform slot we overwrite.
  struct NestedRenderPlacement {
    static glm::vec3& fb(Property<glm::vec3>& p, std::unordered_map<uint64_t, GenericValue>& ov) {
      auto it = ov.find(p.id);
      return it != ov.end() ? it->second.get<glm::vec3>() : p.value;
    }
    static glm::quat& fb(Property<glm::quat>& p, std::unordered_map<uint64_t, GenericValue>& ov) {
      auto it = ov.find(p.id);
      return it != ov.end() ? it->second.get<glm::quat>() : p.value;
    }

    Project::Object& node;
    uint32_t savedUuid;
    glm::vec3 &fPos, &fScale;
    glm::quat &fRot;
    glm::vec3 sPos, sScale;
    glm::quat sRot;

    NestedRenderPlacement(Project::Object& n, uint32_t pickId, const EditorWorldTrans& world)
      : node{n}, savedUuid{n.uuid},
        fPos{fb(n.pos, n.propOverrides)}, fScale{fb(n.scale, n.propOverrides)},
        fRot{fb(n.rot, n.propOverrides)}, sPos{fPos}, sScale{fScale}, sRot{fRot}
    {
      node.uuid = pickId; // component draw writes this via setObjectID -> pickable
      fPos = world.pos; fRot = world.rot; fScale = world.scale;
    }
    ~NestedRenderPlacement() {
      fPos = sPos; fRot = sRot; fScale = sScale;
      node.uuid = savedUuid;
    }
  };

  using IterCallback = std::function<void(Project::Object&, Project::Component::Entry*)>;

  // Maps a generated pick id -> (instance uuid, path) so nested objects can be picked
  // in the viewport (they share definition uuids across instances). Rebuilt each render.
  inline std::unordered_map<uint32_t, std::pair<uint32_t, std::vector<uint32_t>>> nestedPickReg{};

  uint32_t nestedPickId(uint32_t rootUuid, const std::vector<uint32_t>& path)
  {
    uint64_t h = PropScope::combine(rootUuid, 0x9E3779B9u);
    for(uint32_t u : path) h = PropScope::combine(h, u);
    uint32_t id = static_cast<uint32_t>(h ^ (h >> 32));
    return id ? id : 1u;
  }

  /**
   * Renders the children of a prefab instance (the prefab definition tree) at their
   * composed world transforms. Component draw reads obj.pos/rot/scale, so we briefly
   * place the node at its world transform for the callbacks, then restore it.
   * The node uuid is briefly swapped for a per-instance pick id so it's pickable.
   */
  void renderNestedPrefab(Project::Object& node, const EditorWorldTrans& parentWorld,
                          const IterCallback& callback, int depth,
                          uint32_t rootUuid, std::vector<uint32_t> path)
  {
    if(depth > (int)PropScope::MAX_DEPTH || !node.enabled)return; // self-referencing prefabs
    path.push_back(node.uuid);

    Project::Object* src = Editor::SelectionUtils::prefabDefOf(&node);

    // Match the build's cascade. A prefab-instance node contributes its own override
    // layer, so its component overrides resolve here too. Without it the preview would
    // diverge from the built ROM.
    std::optional<PropScope::PrefabLayer> nodeLayer;
    if(node.isPrefabInstance()) nodeLayer.emplace(node.propOverrides);

    // node's local (parent-relative) transform, resolved with the scene-instance
    // cascade (so a nested transform override is picked up). Used to compose world.
    glm::vec3 lpos   = node.pos.resolve(node.propOverrides);
    glm::quat lrot   = node.rot.resolve(node.propOverrides);
    glm::vec3 lscale = node.scale.resolve(node.propOverrides);

    EditorWorldTrans world{
      parentWorld.pos + parentWorld.rot * (parentWorld.scale * lpos),
      parentWorld.rot * lrot,
      parentWorld.scale * lscale
    };

    uint32_t pickId = nestedPickId(rootUuid, path);
    nestedPickReg[pickId] = {rootUuid, path};

    {
      NestedRenderPlacement place{node, pickId, world};
      for(auto &comp : src->components) {
        PropScope::Path compPath(comp.uuid); // so scene-instance overrides on nested props resolve
        callback(node, &comp);
      }
      callback(node, nullptr);
    } // node transform + uuid restored here

    // nodeLayer stays active for the children. Its keys are path-precise, so an override only
    // resolves for its exact target. Matches the build and lets nested-prefab overrides show.
    for(auto &child : src->children) {
      PropScope::Path childPath(child->uuid);
      renderNestedPrefab(*child, world, callback, depth + 1, rootUuid, path);
    }
  }

  void iterateObjects(Project::Object& parent, const IterCallback& callback)
  {
    for(auto& child : parent.children)
    {
      if(!child->enabled)continue;

      auto srcObj = child.get();
      bool isInstance = false;
      if(child->isPrefabInstance()) {
        auto prefab = ctx.project->getAssets().getPrefabByUUID(child->uuidPrefab.value);
        if(prefab) { srcObj = &prefab->obj; isInstance = true; }
      }

      for(auto &comp : srcObj->components) {
        callback(*child, &comp);
      }
      callback(*child, nullptr);

      // Expand the prefab definition tree (composed onto the instance's world transform).
      if(isInstance) {
        EditorWorldTrans instWorld{
          child->pos.resolve(child->propOverrides),
          child->rot.resolve(child->propOverrides),
          child->scale.resolve(child->propOverrides)
        };
        // Push the scene instance's overrides as the most-derived cascade layer, matching
        // the build, so scene-level overrides on nested props (e.g. flame color) preview.
        PropScope::PrefabLayer sceneLayer(child->propOverrides);
        for(auto &defChild : srcObj->children) {
          PropScope::Path nodePath(defChild->uuid);
          renderNestedPrefab(*defChild, instWorld, callback, 0, child->uuid, {});
        }
      }

      iterateObjects(*child, callback);
    }
  }

  // Builds the cascade scope for a known node path under a prefab instance. The instance's
  // override map is the outermost authoring layer, then one Path per node uuid down the
  // chain, pushed all at once. The build and inspector produce the same keys by pushing
  // the same primitives incrementally. See PropScope's scope-logic note.
  struct NestedPathScope {
    PropScope::PrefabLayer layer;
    std::vector<std::unique_ptr<PropScope::Path>> steps;
    NestedPathScope(const std::unordered_map<uint64_t, GenericValue>& instanceOverrides,
                    const std::vector<uint32_t>& path)
      : layer{instanceOverrides}
    {
      for(uint32_t uid : path) steps.push_back(std::make_unique<PropScope::Path>(uid));
    }
  };

  // Resolves the currently selected nested object (ctx.selSubPath under a prefab instance)
  // to the definition node, the instance overrides are authored on, the node's parent
  // world transform, and the node's own world. This descends the path, reading each
  // intermediate transform to compose world, so it pushes Path incrementally rather than
  // via NestedPathScope.
  struct NestedTarget {
    Project::Object* node{};
    Project::Object* rootInstance{};
    EditorWorldTrans parentWorld{};
    EditorWorldTrans world{};
    std::vector<Project::Object*> nodes{}; // resolved def node for each selSubPath element
    bool valid{false};
  };

  NestedTarget resolveNestedTarget(Project::Scene& scene)
  {
    NestedTarget out;
    if(ctx.selSubPath.empty())return out;
    auto root = scene.getObjectByUUID(ctx.selObjectUUID);
    if(!root || !root->isPrefabInstance())return out;
    auto pf = ctx.project->getAssets().getPrefabByUUID(root->uuidPrefab.value);
    if(!pf)return out;

    EditorWorldTrans world{
      root->pos.resolve(root->propOverrides),
      root->rot.resolve(root->propOverrides),
      root->scale.resolve(root->propOverrides)
    };

    // Cascade context: every prefab-instance node along the path keeps its own layer active
    // for the rest of the descent (matches the build). Keys are path-precise, so an override
    // only resolves for its exact target, and a compound prefab can reach into the prefabs it
    // contains. The layers accumulate rather than popping per step.
    PropScope::PrefabLayer sceneLayer(root->propOverrides);
    std::vector<std::unique_ptr<PropScope::Path>> pathScopes;
    std::vector<std::unique_ptr<PropScope::PrefabLayer>> nodeLayers;

    Project::Object* node = &pf->obj;
    for(uint32_t uid : ctx.selSubPath) {
      Project::Object* defParent = Editor::SelectionUtils::prefabDefOf(node);
      Project::Object* next = nullptr;
      for(auto &c : defParent->children) { if(c->uuid == uid) { next = c.get(); break; } }
      if(!next)return out; // stale path

      out.parentWorld = world;
      out.nodes.push_back(next);
      pathScopes.push_back(std::make_unique<PropScope::Path>(uid));

      if(next->isPrefabInstance())
        nodeLayers.push_back(std::make_unique<PropScope::PrefabLayer>(next->propOverrides));

      glm::vec3 lpos = next->pos.resolve(next->propOverrides);
      glm::quat lrot = next->rot.resolve(next->propOverrides);
      glm::vec3 lscale = next->scale.resolve(next->propOverrides);
      world = EditorWorldTrans{
        world.pos + world.rot * (world.scale * lpos),
        world.rot * lrot,
        world.scale * lscale
      };
      node = next;
    }

    out.node = node;
    out.rootInstance = root.get();
    out.world = world;
    out.valid = true;
    return out;
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

  /**
   * Ensures a property has an override entry before writing to it on an object instance.
   * @tparam T Value type stored by the property.
   * @param obj Object whose override map will be updated.
   * @param prop Property that may need an override entry.
   */
  template<typename T>
  void ensurePropertyOverride(Project::Object *obj, Property<T> &prop)
  {
    if (!obj->hasPropOverride(prop)) {
      obj->addPropOverride(prop);
    }
  }

  struct CamRef { uint64_t uuid; std::string name; };

  // Collect all scene objects (incl. prefab instances) carrying a Camera component.
  void collectCameras(Project::Object &parent, std::vector<CamRef> &out)
  {
    for (auto &child : parent.children) {
      auto srcObj = Editor::SelectionUtils::prefabDefOf(child.get());
      for (auto &comp : srcObj->components) {
        if (comp.id == 3) { // Camera
          out.push_back({child->uuid, child->name.empty() ? "Camera" : child->name});
          break;
        }
      }
      collectCameras(*child, out);
    }
  }
}

Editor::Viewport3D::Viewport3D(uint32_t winId_)
  : dummySkeleton{ctx.gpu}, winId{winId_}
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
    dummySkeleton.update(*copyPass);
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
}

void Editor::Viewport3D::detach() {
  if(detached)return;
  detached = true;
  ctx.scene->removeRenderPass(passId);
  ctx.scene->removeCopyPass(passId);
  ctx.scene->removePostRenderCallback(passId);
}

Editor::Viewport3D::~Viewport3D() {
  detach();

  if(--spritesRefCount == 0) {
    sprites = nullptr;
  }
}

nlohmann::json Editor::Viewport3D::saveState() const
{
  // Note: the camera transform is intentionally not persisted, each viewport opens
  // with the default view so a stale orientation can't carry over between sessions.
  return {
    {"winId", winId},
    {"showGrid", showGrid},
    {"showCollMesh", showCollMesh},
    {"showCollObj", showCollObj},
    {"showIcons", showIcons},
    {"boundCam", boundCameraUUID},
    {"camRes", useCameraRes},
  };
}

void Editor::Viewport3D::loadState(const nlohmann::json &j)
{
  showGrid = j.value("showGrid", showGrid);
  showCollMesh = j.value("showCollMesh", showCollMesh);
  showCollObj = j.value("showCollObj", showCollObj);
  showIcons = j.value("showIcons", showIcons);
  boundCameraUUID = j.value("boundCam", (uint64_t)0);
  useCameraRes = j.value("camRes", false);
}

bool Editor::Viewport3D::alignFocusedObjectToCamera()
{
  auto scene = ctx.project ? ctx.project->getScenes().getLoadedScene() : nullptr;
  // No scene loaded or no object selected --> Abort
  if (!scene || ctx.selObjectUUID == 0)return false;

  auto obj = scene->getObjectByUUID(ctx.selObjectUUID);
  // Cannot get selected object --> Abort
  if (!obj)return false;

  // Prefab instances store transform edits in overrides, so create them before writing position or rotation
  if (obj->isPrefabInstance() && !ctx.isPrefabEditing(obj->uuid)) {
    ensurePropertyOverride(obj.get(), obj->pos);
    ensurePropertyOverride(obj.get(), obj->rot);
  }

  // Read current transform to preserve child offsets after moving the parent to the editor camera
  glm::vec3 skew{0.0f};
  glm::vec4 persp{0.0f, 0.0f, 0.0f, 1.0f};
  glm::vec3 objScale = obj->scale.resolve(obj->propOverrides);
  glm::quat objRot = obj->rot.resolve(obj->propOverrides);
  glm::vec3 objPos = obj->pos.resolve(obj->propOverrides);

  // Rebuild current object matrix so child transforms can be converted into the old local space
  auto oldObjMatrix = glm::recompose(objScale, objRot, objPos, skew, persp);

  // Cache each child in the local space of the old transform so they can be rebuilt relative to the new one
  std::unordered_map<uint64_t, glm::vec3> relPosMap{};
  for (auto& child : obj->children)
  {
    relPosMap[child->uuid] = glm::inverse(oldObjMatrix) * glm::vec4(
      child->pos.resolve(child->propOverrides), 1.0f
    );
  }

  // Copy editor camera transform to focused object
  obj->pos.resolve(obj->propOverrides) = camera.pos;
  obj->rot.resolve(obj->propOverrides) = glm::normalize(camera.rot);

  // Recompose new object matrix so child world positions can be updated consistently
  auto newObjMatrix = glm::recompose(
    obj->scale.resolve(obj->propOverrides),
    obj->rot.resolve(obj->propOverrides),
    obj->pos.resolve(obj->propOverrides),
    skew,
    persp
  );

  // Re-apply cached child offsets relative to new parent transform
  applyDeltaToChildren(*obj, relPosMap, newObjMatrix);

  // Add to history
  UndoRedo::getHistory().markChanged("Align object to camera");
  
  return true;
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

  dummySkeleton.use(renderPass3D);

  camera.apply(uniGlobal);
  uniGlobal.screenSize = glm::vec2{(float)fb.getWidth(), (float)fb.getHeight()};
  SDL_PushGPUVertexUniformData(cmdBuff, 0, &uniGlobal, sizeof(uniGlobal));
  auto &rootObj = scene->getRootObject();

  if(ctx.debugMode)SDL_PushGPUDebugGroup(cmdBuff, "3D Objects");

  bool hadDraw = false;
  iterateObjects(rootObj, [&](Project::Object &obj, Project::Component::Entry *comp) {
    // Don't draw the camera we are looking through: its icon/frustum sits on the lens.
    if(boundCameraUUID && obj.uuid == boundCameraUUID) { hadDraw = false; return; }
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
      PropScope::Dispatch dispatchScope(obj.propOverrides, comp->uuid);
      def.funcDraw3D(obj, *comp, *this, cmdBuff, renderPass3D);
      hadDraw = true;
    }
  });

  iterateObjects(rootObj, [&](Project::Object &obj, Project::Component::Entry *comp) {
    if(!comp)return;
    if(boundCameraUUID && obj.uuid == boundCameraUUID)return;
    auto &def = Project::Component::TABLE[comp->id];

    // @TODO: use flag in component
    if(!showCollMesh && comp->id == 4)return;
    if(!showCollObj && comp->id == 5)return;

    if(def.funcDrawPost3D) {
      PropScope::Dispatch dispatchScope(obj.propOverrides, comp->uuid);
      def.funcDrawPost3D(obj, *comp, *this, cmdBuff, renderPass3D);
    }
  });

  if(ctx.debugMode)SDL_PopGPUDebugGroup(cmdBuff);

  meshLines->recreate(renderScene);
  meshSprites->recreate(renderScene);

  if(!cleanPreview)
  {
    if(ctx.debugMode)SDL_PushGPUDebugGroup(cmdBuff, "3D Lines");
    renderScene.getPipeline("lines").bind(renderPass3D);

    if(showGrid)objGrid.draw(renderPass3D, cmdBuff);
    objLines.draw(renderPass3D, cmdBuff);

    // hack to get thicker lines with AA, just draw again with a 1px offset in screen-space
    if(ctx.prefs.renderFactorAA > 1.0f) {
      auto oldMat = uniGlobal.projMat[2];
      uniGlobal.projMat[2][0] += 1.0f / uniGlobal.screenSize.x;
      uniGlobal.projMat[2][1] -= 1.0f / uniGlobal.screenSize.y;
      SDL_PushGPUVertexUniformData(cmdBuff, 0, &uniGlobal, sizeof(uniGlobal));

      if(showGrid)objGrid.draw(renderPass3D, cmdBuff);
      objLines.draw(renderPass3D, cmdBuff);

      uniGlobal.projMat[2] = oldMat;
      SDL_PushGPUVertexUniformData(cmdBuff, 0, &uniGlobal, sizeof(uniGlobal));
    }
    if(ctx.debugMode)SDL_PopGPUDebugGroup(cmdBuff);
  }

  if(iconsVisible)
  {
    if(ctx.debugMode)SDL_PushGPUDebugGroup(cmdBuff, "3D Sprites");

    renderScene.getPipeline("sprites").bind(renderPass3D);

    sprites->bind(renderPass3D);
    objSprites.draw(renderPass3D, cmdBuff);

    if(ctx.debugMode)SDL_PopGPUDebugGroup(cmdBuff);
  }

  SDL_EndGPURenderPass(renderPass3D);
}

void Editor::Viewport3D::onCopyPass(SDL_GPUCommandBuffer* cmdBuff, SDL_GPUCopyPass *copyPass) {
  //vertBuff->upload(*copyPass);

  if(!ctx.project)return;
  auto scene = ctx.project->getScenes().getLoadedScene();
  if (!scene)return;

  if(ctx.debugMode)SDL_PushGPUDebugGroup(cmdBuff, "Object Copy-Pass");

  auto &rootObj = scene->getRootObject();
  iterateObjects(rootObj, [&](Project::Object &obj, Project::Component::Entry *comp) {
    if(!comp)return;
    auto &def = Project::Component::TABLE[comp->id];
    if(def.funcDrawCopyPass) {
      PropScope::Dispatch dispatchScope(obj.propOverrides, comp->uuid);
      def.funcDrawCopyPass(obj, *comp, *this, cmdBuff, copyPass);
    }
  });

  if(ctx.debugMode)SDL_PopGPUDebugGroup(cmdBuff);
}

void Editor::Viewport3D::onPostRender(Renderer::Scene &renderScene) {
  if (pickedObjID.isRequested()) {
    pickedObjID.setResult(fb.readObjectID(
      mousePosClick.x * fbScale,
      mousePosClick.y * fbScale
    ));
  }
}

void Editor::Viewport3D::draw()
{
  auto &gizStyle = ImViewGuizmo::GetStyle();
  gizStyle.scale = 0.5f * ImGui::Theme::zoomFactor;
  gizStyle.circleRadius = 19.0f;
  gizStyle.labelSize = 1.9f / ImGui::Theme::zoomFactor;
  gizStyle.labelColor = IM_COL32(0,0,0,0xFF);

  camera.update();

  auto scene = ctx.project->getScenes().getLoadedScene();
  if (!scene)return;

  ctx.scene->clearLights();
  auto &rootObj = scene->getRootObject();

  iterateObjects(rootObj, [&](Project::Object &obj, Project::Component::Entry *comp) {
    if(!comp)return;
    auto &def = Project::Component::TABLE[comp->id];
    if(def.funcUpdate) {
      PropScope::Dispatch dispatchScope(obj.propOverrides, comp->uuid);
      def.funcUpdate(obj, *comp);
    }
  });

  fb.setClearColor(scene->conf.clearColor.value);

  if(pickedObjID.hasResult())
  {
    uint32_t newUUID = pickedObjID.consume();

    auto regIt = nestedPickReg.find(newUUID);
    if (newUUID != 0 && regIt != nestedPickReg.end()) {
      // Picked a nested prefab object: select it as a nested override target, unless edit
      // mode restricts it (only the edited prefab's own definition is selectable).
      if (Editor::SelectionUtils::isSelectionAllowed(*scene, regIt->second.first, regIt->second.second)) {
        ctx.setNestedSelection(regIt->second.first, regIt->second.second);
      }
    } else {
      auto newObj = scene->getObjectByUUID(newUUID);
      if(newObj && !newObj->selectable) {
        newUUID = 0;
      }
      // In prefab-edit mode only the edited instance is pickable. Ignore other picks
      // instead of clearing, so the edit selection is kept.
      if(newUUID != 0 && !Editor::SelectionUtils::isSelectionAllowed(*scene, newUUID, {})) {
        // disallowed pick, leave selection unchanged
      } else if (newUUID == 0) {
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
  }
  auto obj = scene->getObjectByUUID(ctx.selObjectUUID);

  float BAR_HEIGHT = 26_px;

  auto availSize = ImGui::GetContentRegionAvail();

  auto currPos = ImGui::GetWindowPos();
  if (availSize.x < 64_px)availSize.x = 64_px;
  if (availSize.y < 64_px)availSize.y = 64_px;
  availSize.y -= BAR_HEIGHT;

  availSize.x = floorf(availSize.x);
  availSize.y = floorf(availSize.y);

  // Resolve the bound scene camera (if any): mirror its transform + aspect onto the editor view.
  bool camLocked = false;
  Project::Component::Camera::View camView{};
  if (boundCameraUUID) {
    if (auto camObj = scene->getObjectByUUID(boundCameraUUID)) {
      auto srcObj = Editor::SelectionUtils::prefabDefOf(camObj.get());
      for (auto &comp : srcObj->components) {
        if (comp.id == 3) {
          camView = Project::Component::Camera::getView(*camObj, comp);
          camera.pos = camObj->pos.resolve(camObj->propOverrides);
          camera.rot = glm::normalize(camObj->rot.resolve(camObj->propOverrides));
          camera.fov = camView.fov;
          camera.velocity = {0,0,0};
          camera.zoomSpeed = 0.0f;
          camLocked = true;
          break;
        }
      }
      if (!camLocked) boundCameraUUID = 0; // object lost its camera component
    } else {
      boundCameraUUID = 0; // bound object no longer exists
    }
  }
  if(!camLocked) camera.fov = 70.0f; // restore the editor default when free-flying

  // Displayed image size; letterboxed to the camera aspect when locked, otherwise fills the area.
  ImVec2 viewSize = availSize;
  float offX = 0.0f, offY = 0.0f;
  if (camLocked) {
    float aspect = camView.aspect > 0.0f ? camView.aspect : 4.0f/3.0f;
    if (availSize.x / availSize.y > aspect) viewSize.x = floorf(availSize.y * aspect);
    else                                    viewSize.y = floorf(availSize.x / aspect);
    offX = floorf((availSize.x - viewSize.x) * 0.5f);
    offY = floorf((availSize.y - viewSize.y) * 0.5f);
  }
  auto currSize = viewSize;

  // Free-res renders at the displayed pixel density; camera-res renders at the cart resolution.
  float aa = ctx.prefs.renderFactorAA;
  ImVec2 renderSize = (camLocked && useCameraRes)
    ? ImVec2(camView.resX * aa, camView.resY * aa)
    : viewSize * aa;
  fbScale = renderSize.x / viewSize.x;

  // Camera-resolution mode is a clean preview: only the rendered image, no editor overlays.
  cleanPreview = camLocked && useCameraRes;
  iconsVisible = showIcons && !cleanPreview;

  fb.resize((int)renderSize.x, (int)renderSize.y);
  camera.screenSize = {renderSize.x, renderSize.y};

  auto &io = ImGui::GetIO();
  float deltaTime = io.DeltaTime;

  ImVec2 gizPos{currPos.x + currSize.x - 50_px, currPos.y + 104_px};

  // mouse pos
  ImVec2 screenPos = ImGui::GetCursorScreenPos();
  mousePos = {ImGui::GetMousePos().x, ImGui::GetMousePos().y};
  mousePos.x -= (screenPos.x + offX);
  mousePos.y -= vpOffsetY; // already includes the letterbox offset (set at image placement)

  if (!ctx.prefs.mouseWheelModifiesSpeed) moveSpeedModifier = 1.0f;
  float moveSpeed = (ctx.prefs.moveSpeed * moveSpeedModifier) * deltaTime;

  bool mouseHeldLeft = ImGui::IsMouseDown(ImGuiMouseButton_Left);
  bool mouseHeldRight = ImGui::IsMouseDown(ImGuiMouseButton_Right);
  bool mouseHeldMiddle = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
  bool newMouseDown = mouseHeldLeft || mouseHeldMiddle || mouseHeldRight;

  // Capture camera control in the viewport where the drag started, so a second open
  // viewport doesn't move in lockstep from the same global mouse/keyboard state.
  bool anyMouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left)
    || ImGui::IsMouseClicked(ImGuiMouseButton_Right)
    || ImGui::IsMouseClicked(ImGuiMouseButton_Middle);
  if (!newMouseDown) inputActive = false;
  else if (anyMouseClicked && isMouseHover) inputActive = true;

  bool isCameraFlying = false;
  bool isAltDown = ImGui::GetIO().KeyAlt;
  bool isShiftDown = ImGui::GetIO().KeyShift;
  if(isShiftDown)moveSpeed *= 4.0f;

  bool hasSelection = !ctx.getSelectedObjectUUIDs().empty();
  // Query under this viewport's gizmo id, else IsUsing()/IsOver() read the wrong id
  ImGuizmo::PushID((int)winId);
  bool overGizmo = hasSelection && ImGuizmo::IsOver();
  ImGuizmo::PopID();

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
    if(ImGui::IsKeyPressed(ctx.prefs.keymap.toggleOrtho))
    {
      camera.isOrtho = !camera.isOrtho;
    }

    // Handle object deletion when Delete is pressed while the viewport is focused and an object is selected
    bool deletedSelection = false;
    if (ImGui::IsWindowFocused() && obj && ImGui::IsKeyPressed(ctx.prefs.keymap.deleteObject)) {
      UndoRedo::getHistory().markChanged("Delete Object");
      if (Editor::SelectionUtils::deleteSelectedObjects(*scene)) {
        deletedSelection = true;
      }
      obj = nullptr;
    }

    isCameraFlying = mouseHeldRight && inputActive;

    if (deletedSelection) {
      hasSelection = false;
    }

    if (newMouseDown && !camLocked && inputActive) {
      glm::vec3 moveDir = {0,0,0};
      if (ImGui::IsKeyDown(ctx.prefs.keymap.moveForward))moveDir.z = -moveSpeed;
      if (ImGui::IsKeyDown(ctx.prefs.keymap.moveBack))moveDir.z = moveSpeed;
      if (ImGui::IsKeyDown(ctx.prefs.keymap.moveLeft))moveDir.x = -moveSpeed;
      if (ImGui::IsKeyDown(ctx.prefs.keymap.moveRight))moveDir.x = moveSpeed;
      if (ImGui::IsKeyDown(ctx.prefs.keymap.moveDown))moveDir.y = -moveSpeed;
      if (ImGui::IsKeyDown(ctx.prefs.keymap.moveUp))moveDir.y = moveSpeed;

      if(moveDir != glm::vec3{0,0,0}) {
        camera.velocity = camera.rot * moveDir;
      }
    } else {
      if(!ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
      {
        if (ImGui::IsKeyDown(ctx.prefs.keymap.gizmoTranslate))gizmoOp = 0;
        if (ImGui::IsKeyDown(ctx.prefs.keymap.gizmoRotate))gizmoOp = 1;
        if (ImGui::IsKeyDown(ctx.prefs.keymap.gizmoScale))gizmoOp = 2;
        if (!camLocked && ImGui::IsKeyPressed(ctx.prefs.keymap.focusObject))camera.focusSelection(ctx);
      }
    }
  }

  if ((isMouseHover || isCameraFlying) && !overRotGizmo && !camLocked) {
    //multitouch trackpads don't generate touch or pinch events on windows
    //instead, we have to rely on the fact that trackpads move in fractional amounts
    glm::vec2 wheel = glm::vec2(io.MouseWheelH, io.MouseWheel);
    bool usesWheel = wheel != glm::vec2{0,0};
    
    if(usesWheel)
    {
      // We override the normal mouse wheel functionality if the preference is set + mouse is held
      // (...a more robust handling of editor state would probably also help with controlling parts of the 
      // viewport while the mouse is moving out of the window's focus)
      if(ctx.prefs.mouseWheelModifiesSpeed && mouseHeldRight) {
        moveSpeedModifier = std::clamp(moveSpeedModifier + (wheel.y * 0.125f), 0.125f, 4.0f);
      } else {
        if (std::fmod(std::abs(wheel.x), 1.0f) == 0 && std::fmod(std::abs(wheel.y), 1.0f) == 0) {
        //actual wheel or pinch gesture
        float wheelSpeed = (isShiftDown ? 4.0f : 1.0f) * ctx.prefs.zoomSpeed;
        camera.zoomSpeed += wheel.y * wheelSpeed;
        } else {
          if (ctx.prefs.invertWheelY) wheel.y *= -1;
          //two finger swipe on trackpad
          if (isShiftDown) {
            camera.moveDelta(wheel * ctx.prefs.panSpeed);
          } else {
            camera.orbitDelta(wheel * ctx.prefs.lookSpeed);
          }
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
  constexpr const char* const GIZMO_TOOLTIPS[3] = {"Translate", "Rotate", "Scale"};
  for (int i=0; i<3; ++i) {
    if (ConnectedToggleButton(
      GIZMO_LABELS[i],
      gizmoOp == i,
      i == 0, i == 2,
      ImVec2(32_px,24_px)
    )) {
      gizmoOp = i;
    }
    ImGui::SetItemTooltip("%s", GIZMO_TOOLTIPS[i]);
  }

  ImGui::SameLine();

  if (ConnectedToggleButton(ICON_MDI_WEB, isTransWorld, true, true, ImVec2(32_px,24_px))) {
    isTransWorld = !isTransWorld;
  }
  ImGui::SetItemTooltip("Show %s Space", isTransWorld ? "Local" : "World");

  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12_px);

  // Overlay toggles (grid / collision / icons) are all forced off in clean-preview mode.
  ImGui::BeginDisabled(cleanPreview);

  if(ConnectedToggleButton(ICON_MDI_GRID, showGrid && !cleanPreview, true, true, ImVec2(32_px, 24_px))) {
    showGrid = !showGrid;
  }
  ImGui::SetItemTooltip("%s Grid", showGrid ? "Hide" : "Show");

  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 4_px);
  if(ConnectedToggleButton(ICON_MDI_LANDSLIDE_OUTLINE, showCollMesh && !cleanPreview, true, true, ImVec2(32_px, 24_px))) {
    showCollMesh = !showCollMesh;
  }
  ImGui::SetItemTooltip("%s Collision Mesh", showCollMesh ? "Hide" : "Show");

  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 4_px);
  if(ConnectedToggleButton(ICON_MDI_CYLINDER, showCollObj && !cleanPreview, true, true, ImVec2(32_px,24_px))) {
    showCollObj = !showCollObj;
  }
  ImGui::SetItemTooltip("%s Collision Bodies", showCollObj ? "Hide" : "Show");

  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 4_px);
  if(ConnectedToggleButton(ICON_MDI_IMAGE_OUTLINE, showIcons && !cleanPreview, true, true, ImVec2(32_px, 24_px))) {
    showIcons = !showIcons;
  }
  ImGui::SetItemTooltip(cleanPreview ? "Overlays hidden (camera resolution)" : (showIcons ? "Hide Icons" : "Show Icons"));

  ImGui::EndDisabled();

  // Camera select + resolution toggle: mirror the editor view onto a scene camera.
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12_px);
  {
    std::vector<CamRef> cams;
    collectCameras(scene->getRootObject(), cams);

    std::string preview = "Free Camera";
    for (auto &c : cams) if (c.uuid == boundCameraUUID) preview = c.name;
    if (boundCameraUUID && preview == "Free Camera") preview = "Camera";

    ImGui::SetNextItemWidth(150_px);
    if (ImGui::BeginCombo("##camsel", preview.c_str())) {
      if (ImGui::Selectable("Free Camera", boundCameraUUID == 0)) boundCameraUUID = 0;
      for (auto &c : cams) {
        ImGui::PushID((int)(c.uuid & 0xFFFFFFFF));
        if (ImGui::Selectable(c.name.c_str(), c.uuid == boundCameraUUID)) boundCameraUUID = c.uuid;
        ImGui::PopID();
      }
      ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Mirror the editor view onto a scene camera");

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6_px);
    ImGui::BeginDisabled(boundCameraUUID == 0);
    if (ConnectedToggleButton(ICON_MDI_ASPECT_RATIO, useCameraRes && boundCameraUUID, true, true, ImVec2(32_px, 24_px))) {
      useCameraRes = !useCameraRes;
    }
    ImGui::SetItemTooltip("Resolution: %s", useCameraRes ? "Camera" : "Free (editor)");
    ImGui::EndDisabled();
  }

  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12_px);
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3_px);
  ImGui::Text("Cam Speed: %.2fx", moveSpeedModifier);

  ImGui::SetCursorPosY(currPos.y + BAR_HEIGHT);

  auto dragDelta = mousePos - mousePosStart;
  if (isMouseDown && inputActive) {
    ImGui::ClearActiveID();
    if (!camLocked) {
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
    }
  } else {
    camera.stopRotateDelta();
    camera.stopMoveDelta();
    mousePosStart = mousePos = {0,0};
  }
  if (!newMouseDown)isMouseDown = false;

  currPos = ImGui::GetCursorScreenPos();
  currPos.x = floorf(currPos.x + offX);
  currPos.y = floorf(currPos.y + offY);
  ImGui::SetCursorScreenPos(currPos);

  vpOffsetY = currPos.y;

  auto tex = fb.getTexture();
  ImGui::Image(ImTextureID(tex), viewSize);

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

  // Per-viewport gizmo id so multiple open viewports keep independent gizmo state.
  ImGuizmo::PushID((int)winId);
  ImGuizmo::SetDrawlist(draw_list);
  ImGuizmo::SetRect(currPos.x, currPos.y, currSize.x, currSize.y);

  // Snap settings (per gizmo mode, Ctrl to enable) plus the Manipulate call, shared by both
  // selection paths below. Returns true while the gizmo is being dragged.
  auto manipulateGizmo = [&](glm::mat4 &mat) -> bool {
    glm::vec3 snap(10.0f);
    if (gizmoOp == 1) snap = glm::vec3(90.0f / 4.0f);
    else if (gizmoOp == 2) snap = glm::vec3(0.125f);
    bool isSnap = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
    return ImGuizmo::Manipulate(
      glm::value_ptr(uniGlobal.cameraMat), glm::value_ptr(uniGlobal.projMat),
      GIZMO_OPS[gizmoOp], isTransWorld ? ImGuizmo::MODE::WORLD : ImGuizmo::MODE::LOCAL,
      glm::value_ptr(mat), nullptr, isSnap ? glm::value_ptr(snap) : nullptr);
  };

  // Nested prefab object selected: dedicated gizmo that writes a transform override on
  // the instance, keyed by the path (composing/decomposing against the parent's world).
  if (hasSelection && !ctx.selSubPath.empty())
  {
    auto target = resolveNestedTarget(*scene);
    if (target.valid)
    {
      glm::vec3 skewN{0,0,0}; glm::vec4 perspN{0,0,0,1};
      glm::vec3 gscale = target.world.scale;
      for (int i = 0; i < 3; i++) if (glm::abs(gscale[i]) < 0.0001f) gscale[i] = 0.0001f;
      glm::mat4 gizmoMat = glm::recompose(gscale, target.world.rot, target.world.pos, skewN, perspN);

      if (manipulateGizmo(gizmoMat))
      {
        gizmoTransformActive = true;

        glm::vec3 nwScale, nwPos; glm::quat nwRot;
        glm::decompose(gizmoMat, nwScale, nwRot, nwPos, skewN, perspN);

        // world -> parent-relative local
        glm::quat pInv = glm::inverse(target.parentWorld.rot);
        glm::vec3 newLpos = (pInv * (nwPos - target.parentWorld.pos)) / target.parentWorld.scale;
        glm::quat newLrot = pInv * nwRot;
        glm::vec3 newLscale = nwScale / target.parentWorld.scale;

        auto auth = Editor::SelectionUtils::pickAuthNode(target.rootInstance, target.nodes);

        if (auth.directDefEdit) {
          // Direct edit of this prefab's definition (empty scope -> the node's own slot).
          target.node->pos.resolve(target.node->propOverrides) = newLpos;
          target.node->rot.resolve(target.node->propOverrides) = newLrot;
          target.node->scale.resolve(target.node->propOverrides) = newLscale;
        } else {
          NestedPathScope authorScope(auth.authNode->propOverrides, auth.relPath);
          ensurePropertyOverride(target.node, target.node->pos);
          ensurePropertyOverride(target.node, target.node->rot);
          ensurePropertyOverride(target.node, target.node->scale);
          target.node->pos.resolve(target.node->propOverrides) = newLpos;
          target.node->rot.resolve(target.node->propOverrides) = newLrot;
          target.node->scale.resolve(target.node->propOverrides) = newLscale;
        }

        if (ctx.isPrefabEditing(target.rootInstance->uuid)) {
          ctx.project->getAssets().markPrefabDirty(target.rootInstance->uuidPrefab.value);
        }
      }
    }
  }

  if (hasSelection && ctx.selSubPath.empty()) {
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
        glm::vec3 scale = obj->scale.resolve(obj->propOverrides, &isOverride);
        for (int i = 0; i < 3; i++) if (glm::abs(scale[i]) < 0.0001f) scale[i] = 0.0001f;
        gizmoMat = glm::recompose(
          scale,
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

      // Grid snap for the absolute-snap shortcut below (the gizmo's own snap is in the helper).
      glm::vec3 snap(10.0f);
      if (gizmoOp == 1) snap = glm::vec3(90.0f / 4.0f);
      else if (gizmoOp == 2) snap = glm::vec3(0.125f);
      bool isOnlySelf = ImGui::IsKeyDown(ImGuiKey_LeftShift);

      // snap object to absolute grid
      if(ImGui::IsKeyDown(ImGuiKey_LeftShift) && ImGui::IsKeyPressed(ctx.prefs.keymap.snapObject))
      {
        glm::vec3 pos = obj->pos.resolve(obj->propOverrides);
        pos.x = std::round(pos.x / snap.x) * snap.x;
        pos.y = std::round(pos.y / snap.y) * snap.y;
        pos.z = std::round(pos.z / snap.z) * snap.z;
        obj->pos.resolve(obj->propOverrides) = pos;
      }

      if(manipulateGizmo(gizmoMat)) {
        gizmoTransformActive = true;

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
              if (selObj->isPrefabInstance() && !ctx.isPrefabEditing(selObj->uuid)) {
                ensurePropertyOverride(selObj, selObj->pos);
                ensurePropertyOverride(selObj, selObj->rot);
                ensurePropertyOverride(selObj, selObj->scale);
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
              if (selObj->isPrefabInstance() && !ctx.isPrefabEditing(selObj->uuid)) {
                ensurePropertyOverride(selObj, selObj->pos);
                ensurePropertyOverride(selObj, selObj->rot);
                ensurePropertyOverride(selObj, selObj->scale);
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

  // The over-gizmo check at the top lags a frame (ImGuizmo computes hover during Manipulate),
  // so grabbing a handle can still cause a box-select. Cancel it the moment the gizmo grabs.
  if (ImGuizmo::IsUsing()) {
    selectionPending = false;
    selectionDragging = false;
  }
  ImGuizmo::PopID();

  if (camLocked) {
    overRotGizmo = false;
  } else {
    gizPos = {currPos.x + viewSize.x - 50_px, currPos.y + 54_px};
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) viewGizmoOwned = false;
    bool eligible = isMouseHover || viewGizmoOwned;

    glm::vec3 posOffset = camera.pos - camera.pivot;
    float camDist = glm::length(posOffset);
    if (eligible) {
      if (ImViewGuizmo::Rotate(posOffset, camera.rot, gizPos, camDist)) {
        camera.pos = camera.pivot + posOffset;
      }
      if (ImViewGuizmo::IsUsing()) viewGizmoOwned = true;
    } else {
      glm::vec3 dummyPos = posOffset;
      glm::quat dummyRot = camera.rot;
      ImViewGuizmo::Rotate(dummyPos, dummyRot, gizPos, camDist);
    }
    overRotGizmo = eligible && ImViewGuizmo::IsOver();
  }
}
