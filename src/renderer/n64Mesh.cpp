/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "n64Mesh.h"
#include "../context.h"
#include "../project/assetManager.h"
#include <filesystem>

#include "scene.h"
#include "n64/n64Material.h"

namespace fs = std::filesystem;
extern SDL_GPUSampler *texSamplerRepeat; // @TODO make sampler manager? is this even needed?

namespace
{
  constinit glm::vec4 lastPrim{};
  constinit glm::vec4 lastEnv{};
}

void Renderer::N64Mesh::fromT3DM(const Project::Assets::Model3D &model3d, Project::AssetManager &assetManager)
{
  loaded = false;
  mesh.vertices.clear();
  mesh.indices.clear();
  parts.clear();

  auto &t3dmData = model3d.t3dm;
  parts.resize(t3dmData.models.size());
  auto part = parts.begin();

  uint16_t idx = 0;
  for (auto &model : t3dmData.models)
  {
    part->indicesOffset = mesh.indices.size();
    part->indicesCount = model.triangles.size() * 3;

    part->materialName = model.materialName;
    part->texBindings[0].texture = assetManager.getFallbackTexture()->getGPUTex();
    part->texBindings[0].sampler = texSamplerRepeat;
    part->texBindings[1] = part->texBindings[0];

    //model.material.colorCombiner
    for (auto &tri : model.triangles) {

      for (auto &vert : tri.vert) {

        uint8_t r = (vert.rgba >> 24) & 0xFF;
        uint8_t g = (vert.rgba >> 16) & 0xFF;
        uint8_t b = (vert.rgba >> 8) & 0xFF;
        uint8_t a = (vert.rgba >> 0) & 0xFF;

        mesh.vertices.push_back({
          {vert.pos[0], vert.pos[1], vert.pos[2]},
          vert.norm,
          {r,g,b,a},
          glm::ivec2(vert.s, vert.t)
        });
        /*printf("v: %d,%d,%d norm: %d uv: %d,%d col: %08X\n",
          vert.pos[0], vert.pos[1], vert.pos[2],
          vert.norm,
          vert.s, vert.t,
          vert.rgba
        );*/
      }

      mesh.indices.push_back(idx++);
      mesh.indices.push_back(idx++);
      mesh.indices.push_back(idx++);
    }

    ++part;
  }
}

void Renderer::N64Mesh::recreate(Renderer::Scene &sc) {
  scene = &sc;
  mesh.recreate(sc);
  loaded = true;
}

void Renderer::N64Mesh::draw(
  SDL_GPURenderPass* pass, SDL_GPUCommandBuffer *cmdBuff, UniformsObject &uniforms,
  const std::vector<uint32_t> &partsIndices,
  const Project::Assets::Model3D &model,
  const Project::Component::Shared::MaterialInstance *matInstance
) {
  if (!scene)return;

  auto drawPart = [&](MeshPart &part)
  {
    uint32_t flags = uniforms.mat.flags;
    uint32_t blender = uniforms.mat.blender.x;

    uint32_t slotIdx = 0;
    auto matEntry = model.materials.find(part.materialName);
    if(matEntry != model.materials.end()) {
      auto mat = matEntry->second;

      auto resolveTex = [&](Project::Assets::MaterialTex &tex, int texBinding)
      {
        if (tex.set.value) {
          if(tex.dynType.value == tex.DYN_TYPE_FULL && slotIdx < 8) {
            tex = matInstance->texSlots[slotIdx];
            ++slotIdx;
          }
          else if(tex.dynType.value == tex.DYN_TYPE_TILE && slotIdx < 8) {
            tex.offset = matInstance->texSlots[slotIdx].offset;
            ++slotIdx;
          }
          auto texEntry = ctx.project->getAssets().getEntryByUUID(tex.texUUID.value);
          if (texEntry && texEntry->texture) {
            part.texBindings[texBinding].texture = texEntry->texture->getGPUTex();
          }
        }
      };
      resolveTex(mat.tex0, 0);
      resolveTex(mat.tex1, 1);

      N64Material::convert(part, mat);
    }

    if(part.material.flags & UniformN64Material::FLAG_SET_PRIM_COL) {
      lastPrim = part.material.colPrim;
    } else {
      if(matInstance->setPrim.value)lastPrim = matInstance->prim.value;
    }

    if(part.material.flags & UniformN64Material::FLAG_SET_ENV_COL) {
      lastEnv = part.material.colEnv;
    } else {
      if(matInstance->setEnv.value)lastEnv = matInstance->env.value;
    }

    uniforms.mat = part.material;
    uniforms.mat.colPrim = lastPrim;
    uniforms.mat.colEnv = lastEnv;
    uniforms.mat.blender.x = blender;

    uniforms.mat.flags |= flags;

    // @TODO: move out
    float clip = uniforms.mat.lightDir[0].w;
    const auto &lights = scene->getLights();
    int lightIdx = 0;
    for (auto &light : lights) {
      if (light.type == 0) {
        uniforms.mat.ambientColor = light.color;
      } else {
        if (lightIdx < 2)
        {
          uniforms.mat.lightDir[lightIdx] = glm::vec4(light.dir, 0.0f);
          uniforms.mat.lightColor[lightIdx] = light.color;
          ++lightIdx;
        }
      }
    }
    uniforms.mat.lightDir[0].w = clip;

    SDL_BindGPUFragmentSamplers(pass, 0, part.texBindings, 2);
    SDL_BindGPUVertexSamplers(pass, 0, part.texBindings, 2); // needed?

    SDL_PushGPUVertexUniformData(cmdBuff, 1, &uniforms, sizeof(uniforms));
    SDL_PushGPUFragmentUniformData(cmdBuff, 0, &uniforms, sizeof(uniforms));

    mesh.draw(pass, part.indicesOffset, part.indicesCount);
  };

  if(partsIndices.empty())
  {
    for (auto &part : parts) {
      drawPart(part);
    }
  } else {
    for (auto idx : partsIndices) {
      if (idx < parts.size()) {
        drawPart(parts[idx]);
      }
    }
  }
  //mesh.draw(pass);
}
