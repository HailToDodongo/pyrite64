/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <memory>
#include <tiny3d/tools/gltf_importer/src/structs.h>

#include "glm/mat4x4.hpp"
#include "glm/vec3.hpp"

namespace T3DM
{
  struct Keyframe;
  struct Anim;
}

namespace Renderer
{
  class Skeleton;

  class Animation
  {
    private:
      float animTime{0.0f};
      int kfPos{0};

      T3DM::Anim* lastAnim{nullptr};

      struct ChannelState
      {
        T3DM::Keyframe *kfCurr{};
        T3DM::Keyframe *kfNext{};
        uint32_t targetIdx{};
        uint32_t attrIdx{};
        T3DM::AnimChannelTarget targetType{};
      };
      std::vector<ChannelState> chState{};

      void reset(T3DM::Anim &anim);

    public:
      void update(
        T3DM::Anim &anim,
        std::shared_ptr<Skeleton> skeleton,
        float deltaTime
      );
  };
}
