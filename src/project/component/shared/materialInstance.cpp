/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "materialInstance.h"
#include "../../scene/object.h"

void Project::Component::Shared::MaterialInstance::build(Utils::BinaryFile &file, Object &obj)
{
  uint16_t setMask = 0;
  if(setDepth.resolve(obj)) setMask |= 1 << 0;
  if(setPrim.resolve(obj))  setMask |= 1 << 1;
  if(setEnv.resolve(obj))   setMask |= 1 << 2;
  if(setLighting.resolve(obj)) setMask |= 1 << 3;
  if(setFresnel.resolve(obj)) setMask |= 1 << 4;

  uint8_t valFlags = depth.resolve(obj); // 2bits
  if(lighting.value) valFlags |= 1 << 2;

  file.write<uint16_t>(setMask);
  file.write<uint8_t>(fresnel.resolve(obj));
  file.write<uint8_t>(valFlags);
  file.writeRGBA(prim.resolve(obj));
  file.writeRGBA(env.resolve(obj));
  file.writeRGBA(fresnelColor.resolve(obj));
}
