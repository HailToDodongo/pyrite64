/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <array>
#include <memory>

#include "IconsMaterialDesignIcons.h"
#include "simdjson.h"
#include "../../build/sceneContext.h"

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
  typedef std::string(*FuncCompSerial)(Entry &entry);
  typedef std::shared_ptr<void>(*FuncCompDeserial)(simdjson::simdjson_result<simdjson::dom::object> &doc);
  typedef void(*FuncCompBuild)(Object&, Entry &entry, Build::SceneCtx &ctx);

  struct CompInfo
  {
    int id{};
    const char* icon{};
    const char* name{};
    FuncCompInit funcInit{};
    FuncCompDraw funcUpdate{};
    FuncCompDraw funcDraw{};
    FuncCompDraw3D funcDraw3D{};
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
      std::string serialize(Entry &entry); \
      std::shared_ptr<void> deserialize(simdjson::simdjson_result<simdjson::dom::object> &doc); \
      void build(Object&, Entry &entry, Build::SceneCtx &ctx); \
    }

  MAKE_COMP(Code)
  MAKE_COMP(Model)
  MAKE_COMP(Light)
  MAKE_COMP(Camera)

  constexpr std::array TABLE{
    CompInfo{
      .id = 0,
      .icon = ICON_MDI_SCRIPT " ",
      .name = "Code",
      .funcInit = Code::init,
      .funcDraw = Code::draw,
      .funcSerialize = Code::serialize,
      .funcDeserialize = Code::deserialize,
      .funcBuild = Code::build
    },
    CompInfo{
      .id = 1,
      .icon = ICON_MDI_CUBE_OUTLINE " ",
      .name = "Model",
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
      .name = "Light",
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
      .name = "Camera",
      .funcInit = Camera::init,
      .funcUpdate = Camera::update,
      .funcDraw = Camera::draw,
      .funcDraw3D = Camera::draw3D,
      .funcSerialize = Camera::serialize,
      .funcDeserialize = Camera::deserialize,
      .funcBuild = Camera::build
    },
  };
}
