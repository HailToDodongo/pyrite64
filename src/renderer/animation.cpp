/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "animation.h"
#include <tiny3d/tools/gltf_importer/src/structs.h>

#include "skeleton.h"

void Renderer::Animation::reset(T3DM::Anim &anim)
{
  animTime = 0.0f;
  kfPos = 0;
  chState.clear();

  for(size_t i = 0; i < anim.channelMap.size(); i++) {
    auto &channel = anim.channelMap[i];
    chState.push_back({
      .kfCurr = nullptr,
      .kfNext = nullptr,
      .targetIdx = channel.targetIdx,
      .attrIdx = channel.attributeIdx,
      .targetType = channel.targetType,
    });
  }
}

void Renderer::Animation::update(
  T3DM::Anim &anim,
  std::shared_ptr<Skeleton> skeleton,
  float deltaTime
)
{
  if(lastAnim != &anim) {
    reset(anim);
    lastAnim = &anim;
  }

  animTime += deltaTime;
  if(animTime > anim.duration)
  {
    kfPos = 0;
    animTime -= anim.duration;
  }

  // printf("Anim: %s | Time: %.2f - %.2f\n", anim.name.c_str(), animTime, anim.duration);

  // fetch new keyframes when needed,
  // those make sure the current and next KFs are fetched in advanced
  while(kfPos < (int)anim.keyframes.size())
  {
    auto &kf = anim.keyframes[kfPos];
    if(kf.timeNeeded > animTime)break;

    //printf(" - target[%d]: %s | type: %d\n", channel.targetIdx, channel.targetName.c_str(), channel.targetType);
    auto &state = chState[kf.chanelIdx];
    state.kfCurr = state.kfNext;
    state.kfNext = &kf;
    kfPos++;
  }

  // now update all bones values that are attached in a channel,
  // the value is interpolated between the current and next KF.
  for(auto &ch : chState)
  {
    Skeleton::Bone* bone = skeleton ? skeleton->getBone(ch.targetIdx) : nullptr;
    if(bone && ch.kfNext && ch.kfCurr)
    {
      float timeDiff = ch.kfNext->time - ch.kfCurr->time;
      float interp = (animTime - ch.kfCurr->time) / timeDiff;

      switch(ch.targetType)
      {
        case T3DM::AnimChannelTarget::TRANSLATION:
          bone->pos[ch.attrIdx] = glm::mix(ch.kfCurr->valScalar, ch.kfNext->valScalar, interp);
        break;
        case T3DM::AnimChannelTarget::SCALE:
          bone->scale[ch.attrIdx] = glm::mix(ch.kfCurr->valScalar, ch.kfNext->valScalar, interp);
        break;
        case T3DM::AnimChannelTarget::ROTATION:
          bone->setRot(ch.kfCurr->valQuat.slerp(ch.kfNext->valQuat, interp));
        break;
        default: break;
      }
    }
  }
}
