/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <array>
#include <memory>

#include "json.hpp"
#include "IconsMaterialDesignIcons.h"
#include "../../build/sceneContext.h"
#include "../../editor/imgui/lang.h"

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
  typedef std::shared_ptr<void>(*FuncCompInit)(Object&);
  typedef nlohmann::json(*FuncCompSerial)(const Entry &entry);
  typedef std::shared_ptr<void>(*FuncCompDeserial)(nlohmann::json &doc);
  typedef void(*FuncCompBuild)(Object&, Entry &entry, Build::SceneCtx &ctx);

  struct CompInfo
  {
    int id{};
    int prio{};
    const char* icon{};
    const char* name{};
    FuncCompInit funcInit{};
    FuncCompDraw funcUpdate{};
    FuncCompDraw funcDraw{};
    FuncCompDraw3D funcDraw3D{};
    FuncCompDraw3D funcDrawPost3D{};
    FuncCompSerial funcSerialize{};
    FuncCompDeserial funcDeserialize{};
    FuncCompBuild funcBuild{};
  };

  #define MAKE_COMP(name) \
    namespace name \
    { \
      std::shared_ptr<void> init(Object& obj); \
      void update(Object& obj, Entry &entry); \
      void draw(Object& obj, Entry &entry); \
      void draw3D(Object&, Entry &entry, Editor::Viewport3D &vp, SDL_GPUCommandBuffer* cmdBuff, SDL_GPURenderPass* pass); \
      nlohmann::json serialize(const Entry &entry); \
      std::shared_ptr<void> deserialize(nlohmann::json &doc); \
      void build(Object&, Entry &entry, Build::SceneCtx &ctx); \
    }

  MAKE_COMP(Code)
  MAKE_COMP(Model)
  MAKE_COMP(Light)
  MAKE_COMP(Camera)
  MAKE_COMP(CollMesh)
  MAKE_COMP(CollBody)
  MAKE_COMP(Audio2D)
  MAKE_COMP(Constraint)
  MAKE_COMP(Culling)
  MAKE_COMP(NodeGraph)
  MAKE_COMP(AnimModel)

  constexpr std::array TABLE{
    CompInfo{
      .id = 0,
      .icon = ICON_MDI_SCRIPT " ",
      .name = MSG_OBJECT_COMPONENT_CODE,
      .funcInit = Code::init,
      .funcDraw = Code::draw,
      .funcSerialize = Code::serialize,
      .funcDeserialize = Code::deserialize,
      .funcBuild = Code::build
    },
    CompInfo{
      .id = 1,
      .icon = ICON_MDI_CUBE_OUTLINE " ",
      .name = MSG_OBJECT_COMPONENT_STATIC_MODEL,
      .funcInit = Model::init,
      .funcDraw = Model::draw,
      .funcDraw3D = Model::draw3D,
      .funcSerialize = Model::serialize,
      .funcDeserialize = Model::deserialize,
      .funcBuild = Model::build
    },
    CompInfo{
      .id = 2,
      .icon = ICON_MDI_LIGHTBULB_ON_OUTLINE " ",
      .name = MSG_OBJECT_COMPONENT_LIGHT,
      .funcInit = Light::init,
      .funcUpdate = Light::update,
      .funcDraw = Light::draw,
      .funcDraw3D = Light::draw3D,
      .funcSerialize = Light::serialize,
      .funcDeserialize = Light::deserialize,
      .funcBuild = Light::build
    },
    CompInfo{
      .id = 3,
      .icon = ICON_MDI_VIDEO_VINTAGE " ",
      .name = MSG_OBJECT_COMPONENT_CAMERA,
      .funcInit = Camera::init,
      .funcUpdate = Camera::update,
      .funcDraw = Camera::draw,
      .funcDraw3D = Camera::draw3D,
      .funcSerialize = Camera::serialize,
      .funcDeserialize = Camera::deserialize,
      .funcBuild = Camera::build
    },
    CompInfo{
      .id = 4,
      .icon = ICON_MDI_LANDSLIDE_OUTLINE " ",
      .name = MSG_OBJECT_COMPONENT_COLLISION_MESH,
      .funcInit = CollMesh::init,
      .funcDraw = CollMesh::draw,
      .funcDrawPost3D = CollMesh::draw3D,
      .funcSerialize = CollMesh::serialize,
      .funcDeserialize = CollMesh::deserialize,
      .funcBuild = CollMesh::build
    },
    CompInfo{
      .id = 5,
      .icon = ICON_MDI_CYLINDER " ",
      .name = MSG_OBJECT_COMPONENT_COLLISION_BODY,
      .funcInit = CollBody::init,
      .funcDraw = CollBody::draw,
      .funcDrawPost3D = CollBody::draw3D,
      .funcSerialize = CollBody::serialize,
      .funcDeserialize = CollBody::deserialize,
      .funcBuild = CollBody::build
    },
    CompInfo{
      .id = 6,
      .icon = ICON_MDI_MUSIC " ",
      .name = MSG_OBJECT_COMPONENT_AUDIO_2D,
      .funcInit = Audio2D::init,
      .funcDraw = Audio2D::draw,
      .funcDrawPost3D = Audio2D::draw3D,
      .funcSerialize = Audio2D::serialize,
      .funcDeserialize = Audio2D::deserialize,
      .funcBuild = Audio2D::build
    },
    CompInfo{
      .id = 7,
      .prio = -2, // constraint must come before culling and any drawing
      .icon = ICON_MDI_LINK " ",
      .name = MSG_OBJECT_COMPONENT_CONSTRAINT,
      .funcInit = Constraint::init,
      .funcDraw = Constraint::draw,
      .funcDrawPost3D = Constraint::draw3D,
      .funcSerialize = Constraint::serialize,
      .funcDeserialize = Constraint::deserialize,
      .funcBuild = Constraint::build
    },
    CompInfo{
      .id = 8,
      .prio = -1, // culling must come before any models
      .icon = ICON_MDI_EYE_OFF_OUTLINE " ",
      .name = MSG_OBJECT_COMPONENT_CULLING,
      .funcInit = Culling::init,
      .funcDraw = Culling::draw,
      .funcDrawPost3D = Culling::draw3D,
      .funcSerialize = Culling::serialize,
      .funcDeserialize = Culling::deserialize,
      .funcBuild = Culling::build
    },
    CompInfo{
      .id = 9,
      .icon = ICON_MDI_GRAPH_OUTLINE " ",
      .name = MSG_OBJECT_COMPONENT_NODE_GRAPH,
      .funcInit = NodeGraph::init,
      .funcDraw = NodeGraph::draw,
      .funcDraw3D = NodeGraph::draw3D,
      .funcSerialize = NodeGraph::serialize,
      .funcDeserialize = NodeGraph::deserialize,
      .funcBuild = NodeGraph::build
    },
    CompInfo{
      .id = 10,
      .icon = ICON_MDI_HUMAN " ",
      .name = MSG_OBJECT_COMPONENT_ANIMATED_MODEL,
      .funcInit = AnimModel::init,
      .funcDraw = AnimModel::draw,
      .funcDraw3D = AnimModel::draw3D,
      .funcSerialize = AnimModel::serialize,
      .funcDeserialize = AnimModel::deserialize,
      .funcBuild = AnimModel::build
    }
  };
}
