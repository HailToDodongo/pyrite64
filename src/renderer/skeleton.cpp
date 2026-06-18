/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "skeleton.h"
#include "../project/assets/model3d.h"

namespace
{
  // same as 't3d_mat4_from_srt'
  glm::mat4 mat4FromSRT(const glm::vec3 &scale,  const glm::quat &quat, const glm::vec3 translate)
  {
    float qxx = quat[0] * quat[0];
    float qyy = quat[1] * quat[1];
    float qzz = quat[2] * quat[2];
    float qxz = quat[0] * quat[2];
    float qxy = quat[0] * quat[1];
    float qyz = quat[1] * quat[2];
    float qwx = quat[3] * quat[0];
    float qwy = quat[3] * quat[1];
    float qwz = quat[3] * quat[2];

    glm::mat4 res{
        {1.0f - 2.0f * (qyy + qzz),        2.0f * (qxy + qwz),        2.0f * (qxz - qwy), 0.0f},
        {       2.0f * (qxy - qwz), 1.0f - 2.0f * (qxx + qzz),        2.0f * (qyz + qwx), 0.0f},
        {       2.0f * (qxz + qwy),        2.0f * (qyz - qwx), 1.0f - 2.0f * (qxx + qyy), 0.0f},
        {             translate[0],              translate[1],              translate[2], 1.0f}
    };
    return glm::scale(res, scale);
  }
}

void Renderer::Skeleton::readBone(const T3DM::Bone &bone, uint32_t parentIdx)
{
  uint32_t currIdx = bones.size();
  bones.push_back({
    .pos = glm::vec3(bone.pos.x(), bone.pos.y(), bone.pos.z()) * importScale,
    .rot = {bone.rot.x(), bone.rot.y(), bone.rot.z(), bone.rot.w()},
    .scale = glm::vec3(bone.scale.x(), bone.scale.y(), bone.scale.z()),
    .parentIdx = parentIdx
  });
  boneMats.push_back(glm::identity<glm::mat4>());

  for (const auto &childBone : bone.children) {
    readBone(*childBone, currIdx);
  }
}

Renderer::Skeleton::Skeleton(SDL_GPUDevice* device, const Project::Assets::Model3D &model, float importScale_)
  : storageBuff{device}, importScale{importScale_}
{
  for (const auto &rootBone : model.t3dm.skeletons) {
    readBone(rootBone, 0xFFFF'FFFF);
  }
  /*printf("Bones: %zu\n", bones.size());
  for (size_t i = 0; i < bones.size(); i++)
  {
    printf("Bone %zu: parent %u\n", i, bones[i].parentIdx);
    printf("  SRT: %f %f %f | %f %f %f %f | %f %f %f\n",
      bones[i].scale.x, bones[i].scale.y, bones[i].scale.z,
      bones[i].rot.x, bones[i].rot.y, bones[i].rot.z, bones[i].rot.w,
      bones[i].pos.x, bones[i].pos.y, bones[i].pos.z
    );
  }*/
}

Renderer::Skeleton::Skeleton(SDL_GPUDevice* device)
  : storageBuff{device}
{
  bones.push_back({
    .pos = {0.0f, 0.0f, 0.0f},
    .rot = {0,0,0,1},
    .scale = {1.0f, 1.0f, 1.0f},
  });
  boneMats.push_back(glm::identity<glm::mat4>());
}

void Renderer::Skeleton::update(SDL_GPUCopyPass &pass)
{
  for (size_t i = 0; i < bones.size(); i++)
  {
    const auto &bone = bones[i];
    boneMats[i] = mat4FromSRT(bone.scale, bone.rot, bone.pos);
    if(bone.parentIdx < boneMats.size()) {
       boneMats[i] = boneMats[bone.parentIdx] * boneMats[i];
    }
  }

  storageBuff.setData((char*)boneMats.data(), boneMats.size() * sizeof(glm::mat4));
  storageBuff.upload(pass);
}

void Renderer::Skeleton::use(SDL_GPURenderPass* pass)
{
  storageBuff.bind(pass);
}
