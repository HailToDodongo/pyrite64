/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <array>
#include <memory>
#include <SDL3/SDL_gpu.h>

#include "json.hpp"
#include "IconsMaterialDesignIcons.h"
#include "../../build/sceneContext.h"
#include "../../utils/aabb.h"

namespace Editor
{
  class Viewport3D;
}

struct SDL_GPUCommandBuffer;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPURenderPass;

namespace Project { class Object; }

namespace Project::Component
{
  struct Entry
  {
    int id{};
    uint64_t uuid{};
    std::string name{};
    std::shared_ptr<void> data{};
  };

  typedef void(*FuncCompDraw)(Object&, Entry &entry);
  typedef void(*FuncCompDraw3D)(Object&, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass);
  typedef void(*FuncCompCopyPass)(Object&, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPUCopyPass* pass);
  typedef std::shared_ptr<void>(*FuncCompInit)(Object&);
  typedef nlohmann::json(*FuncCompSerial)(const Entry &entry);
  typedef std::shared_ptr<void>(*FuncCompDeserial)(nlohmann::json &doc);
  typedef void(*FuncCompBuild)(Object&, Entry &entry, Build::SceneCtx &ctx);
  typedef Utils::AABB(*FuncCompGetAABB)(Object&, Entry &entry);

  struct CompInfo
  {
    int id{};
    int prio{};
    const char* icon{};
    const char* name{};
    const char* docSlug{}; // docs page path relative to PYRITE_DOCS_URL
    FuncCompInit funcInit{};
    FuncCompDraw funcUpdate{};
    FuncCompDraw funcDraw{};
    FuncCompDraw3D funcDraw3D{};
    FuncCompDraw3D funcDrawPost3D{};
    FuncCompCopyPass funcDrawCopyPass{};
    FuncCompSerial funcSerialize{};
    FuncCompDeserial funcDeserialize{};
    FuncCompBuild funcBuild{};
    FuncCompGetAABB funcGetAABB{};
  };

  #define MAKE_COMP(name) \
    namespace name \
    { \
      std::shared_ptr<void> init(Object& obj); \
      void update(Object& obj, Entry &entry); \
      void draw(Object& obj, Entry &entry); \
      void draw3D(Object&, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass); \
      void drawCopyPass(Object&, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPUCopyPass* pass); \
      nlohmann::json serialize(const Entry &entry); \
      std::shared_ptr<void> deserialize(nlohmann::json &doc); \
      void build(Object&, Entry &entry, Build::SceneCtx &ctx); \
      Utils::AABB getAABB(Object &obj, Entry &entry); \
    }

  MAKE_COMP(Code)
  MAKE_COMP(Model)
  MAKE_COMP(Light)
  MAKE_COMP(Camera)
  MAKE_COMP(CollMesh)
  MAKE_COMP(CollBody)
  MAKE_COMP(RigidBody)
  MAKE_COMP(Audio2D)
  MAKE_COMP(Constraint)
  MAKE_COMP(Culling)
  MAKE_COMP(NodeGraph)
  MAKE_COMP(AnimModel)
  MAKE_COMP(CharBody)

  namespace Camera
  {
    // Resolved view parameters of a camera component, used by the editor viewport to mirror it.
    struct View { int resX{320}; int resY{240}; float aspect{4.0f/3.0f}; float fov{65.0f}; };
    View getView(Object &obj, Entry &entry);
  }

  namespace Code
  {
    /**
     * Assigns a Script to a Code component.
     * @param entry Code component entry to assign the Script to.
     * @param scriptUUID UUID of the Script.
     * @param openScriptComboBox true to auto-open the combo box.
     */
    void setScript(Entry &entry, uint64_t scriptUUID, bool openScriptComboBox);
  }

  constexpr std::array TABLE{
    CompInfo{
      .id = 0,
      .icon = ICON_MDI_SCRIPT " ",
      .name = "Code",
      .docSlug = "/manual/editor/components/code",
      .funcInit = Code::init,
      .funcDraw = Code::draw,
      .funcSerialize = Code::serialize,
      .funcDeserialize = Code::deserialize,
      .funcBuild = Code::build,
      .funcGetAABB = nullptr
    },
    CompInfo{
      .id = 1,
      .icon = ICON_MDI_CUBE_OUTLINE " ",
      .name = "Model (Static)",
      .docSlug = "/manual/editor/components/model",
      .funcInit = Model::init,
      .funcDraw = Model::draw,
      .funcDraw3D = Model::draw3D,
      .funcSerialize = Model::serialize,
      .funcDeserialize = Model::deserialize,
      .funcBuild = Model::build,
      .funcGetAABB = Model::getAABB
    },
    CompInfo{
      .id = 2,
      .icon = ICON_MDI_LIGHTBULB_ON_OUTLINE " ",
      .name = "Light",
      .docSlug = "/manual/editor/components/light",
      .funcInit = Light::init,
      .funcUpdate = Light::update,
      .funcDraw = Light::draw,
      .funcDraw3D = Light::draw3D,
      .funcSerialize = Light::serialize,
      .funcDeserialize = Light::deserialize,
      .funcBuild = Light::build,
      .funcGetAABB = nullptr
    },
    CompInfo{
      .id = 3,
      .icon = ICON_MDI_VIDEO_VINTAGE " ",
      .name = "Camera",
      .docSlug = "/manual/editor/components/camera",
      .funcInit = Camera::init,
      .funcUpdate = Camera::update,
      .funcDraw = Camera::draw,
      .funcDraw3D = Camera::draw3D,
      .funcSerialize = Camera::serialize,
      .funcDeserialize = Camera::deserialize,
      .funcBuild = Camera::build,
      .funcGetAABB = nullptr
    },
    CompInfo{
      .id = 4,
      .icon = ICON_MDI_LANDSLIDE_OUTLINE " ",
      .name = "Collision-Mesh",
      .docSlug = "/manual/editor/components/collMesh",
      .funcInit = CollMesh::init,
      .funcDraw = CollMesh::draw,
      .funcDrawPost3D = CollMesh::draw3D,
      .funcSerialize = CollMesh::serialize,
      .funcDeserialize = CollMesh::deserialize,
      .funcBuild = CollMesh::build,
      .funcGetAABB = CollMesh::getAABB
    },
    CompInfo{
      .id = 5,
      .icon = ICON_MDI_CYLINDER " ",
      .name = "Collider",
      .docSlug = "/manual/editor/components/collBody",
      .funcInit = CollBody::init,
      .funcDraw = CollBody::draw,
      .funcDrawPost3D = CollBody::draw3D,
      .funcSerialize = CollBody::serialize,
      .funcDeserialize = CollBody::deserialize,
      .funcBuild = CollBody::build,
      .funcGetAABB = nullptr
    },
    CompInfo{
      .id = 6,
      .icon = ICON_MDI_MUSIC " ",
      .name = "Audio (2D)",
      .docSlug = "/manual/editor/components/audio2d",
      .funcInit = Audio2D::init,
      .funcDraw = Audio2D::draw,
      .funcDrawPost3D = Audio2D::draw3D,
      .funcSerialize = Audio2D::serialize,
      .funcDeserialize = Audio2D::deserialize,
      .funcBuild = Audio2D::build,
      .funcGetAABB = nullptr
    },
    CompInfo{
      .id = 7,
      .prio = -2, // constraint must come before culling and any drawing
      .icon = ICON_MDI_LINK " ",
      .name = "Constraint",
      .docSlug = "/manual/editor/components/constraint",
      .funcInit = Constraint::init,
      .funcDraw = Constraint::draw,
      .funcDrawPost3D = Constraint::draw3D,
      .funcSerialize = Constraint::serialize,
      .funcDeserialize = Constraint::deserialize,
      .funcBuild = Constraint::build,
      .funcGetAABB = nullptr
    },
    CompInfo{
      .id = 8,
      .prio = -1, // culling must come before any models
      .icon = ICON_MDI_EYE_OFF_OUTLINE " ",
      .name = "Culling",
      .docSlug = "/manual/editor/components/culling",
      .funcInit = Culling::init,
      .funcDraw = Culling::draw,
      .funcDrawPost3D = Culling::draw3D,
      .funcSerialize = Culling::serialize,
      .funcDeserialize = Culling::deserialize,
      .funcBuild = Culling::build,
      .funcGetAABB = nullptr
    },
    CompInfo{
      .id = 9,
      .icon = ICON_MDI_GRAPH_OUTLINE " ",
      .name = "Node Graph",
      .docSlug = "/manual/editor/components/nodeGraph",
      .funcInit = NodeGraph::init,
      .funcDraw = NodeGraph::draw,
      .funcDraw3D = NodeGraph::draw3D,
      .funcSerialize = NodeGraph::serialize,
      .funcDeserialize = NodeGraph::deserialize,
      .funcBuild = NodeGraph::build,
      .funcGetAABB = nullptr
    },
    CompInfo{
      .id = 10,
      .icon = ICON_MDI_HUMAN " ",
      .name = "Model (Animated)",
      .docSlug = "/manual/editor/components/animModel",
      .funcInit = AnimModel::init,
      .funcDraw = AnimModel::draw,
      .funcDraw3D = AnimModel::draw3D,
      .funcDrawCopyPass = AnimModel::drawCopyPass,
      .funcSerialize = AnimModel::serialize,
      .funcDeserialize = AnimModel::deserialize,
      .funcBuild = AnimModel::build,
      .funcGetAABB = AnimModel::getAABB
    },
    CompInfo{
      .id = 11,
      .icon = ICON_MDI_CYLINDER " ",
      .name = "Rigid-Body",
      .docSlug = "/manual/editor/components/rigidBody",
      .funcInit = RigidBody::init,
      .funcDraw = RigidBody::draw,
      .funcDrawPost3D = RigidBody::draw3D,
      .funcSerialize = RigidBody::serialize,
      .funcDeserialize = RigidBody::deserialize,
      .funcBuild = RigidBody::build,
      .funcGetAABB = nullptr
    },
    CompInfo{
      .id = 12,
      .icon = ICON_MDI_RUN " ",
      .name = "Character-Body",
      .docSlug = "/manual/editor/components/charBody",
      .funcInit = CharBody::init,
      .funcDraw = CharBody::draw,
      .funcDrawPost3D = CharBody::draw3D,
      .funcSerialize = CharBody::serialize,
      .funcDeserialize = CharBody::deserialize,
      .funcBuild = CharBody::build,
      .funcGetAABB = nullptr
    },
  };

  extern std::array<CompInfo, TABLE.size()> TABLE_SORTED_BY_NAME;

  void init();
}
