/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "materialInstance.h"

#include "../../assets/model3d.h"
#include "../../scene/object.h"

void Project::Component::Shared::MaterialInstance::build(Utils::BinaryFile &file, Build::SceneCtx &ctx, Object &obj)
{
  uint16_t setMask = 0;
  if(setDepth.resolve(obj)) setMask |= 1 << 0;
  if(setPrim.resolve(obj))  setMask |= 1 << 1;
  if(setEnv.resolve(obj))   setMask |= 1 << 2;
  if(setLighting.resolve(obj)) setMask |= 1 << 3;
  if(setFresnel.resolve(obj)) setMask |= 1 << 4;

  for(int i=0; i<8; ++i) {
    if(texSlots[i].set.value) {
      setMask |= 1 << (8 + i);
    }
  }

  uint8_t valFlags = depth.resolve(obj); // 2bits
  if(lighting.value) valFlags |= 1 << 2;

  auto posStart = file.getPos();
  file.write<uint32_t>(0); // size
  file.write<uint16_t>(setMask);
  file.write<uint8_t>(fresnel.resolve(obj));
  file.write<uint8_t>(valFlags);
  file.writeRGBA(prim.resolve(obj));
  file.writeRGBA(env.resolve(obj));
  file.writeRGBA(fresnelColor.resolve(obj));

  for(int i=0; i<8; ++i)
  {
    if(texSlots[i].set.value) {
      texSlots[i].build(file, ctx);
    }
  }

  auto size = file.getPos() - posStart;
  file.atPos(posStart, [&]{
    file.write<uint32_t>(size);
  });
}

void Project::Component::Shared::MaterialInstance::validateWithModel(const Assets::Model3D &model)
{
  uint32_t slotIdx = 0;
  for(auto &[slot, tex] : model.materials)
  {
    if(tex.tex0.dynTexture.value)
    {
      texSlots[slotIdx].set.value = true;
      texSlots[slotIdx].dynPlaceholder.value = 0;
      ++slotIdx;
      if(slotIdx >= 8)break;
    }
    if(tex.tex1.dynTexture.value)
    {
      texSlots[slotIdx].set.value = true;
      texSlots[slotIdx].dynPlaceholder.value = 1;
      ++slotIdx;
      if(slotIdx >= 8)break;
    }
  }

  // disable the rest of the slots
  for(; slotIdx < 8; ++slotIdx) {
    texSlots[slotIdx].set.value = false;
  }
}
