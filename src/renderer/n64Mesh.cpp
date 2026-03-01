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

void Renderer::N64Mesh::fromT3DM(const T3DM::T3DMData &t3dmData, Project::AssetManager &assetManager)
{
  loaded = false;
  mesh.vertices.clear();
  mesh.indices.clear();
  parts.clear();

  parts.resize(t3dmData.models.size());
  auto part = parts.begin();

  uint16_t idx = 0;
  for (auto &model : t3dmData.models)
  {
    part->refTex0 = assetManager.getFallbackTexture();
    part->refTex1 = part->refTex0;

    part->indicesOffset = mesh.indices.size();
    part->indicesCount = model.triangles.size() * 3;

    N64Material::convert(*part, model.material);

    part->texBindings[0].texture = part->refTex0.lock()->getGPUTex();
    part->texBindings[0].sampler = texSamplerRepeat;
    part->texBindings[1] = part->texBindings[0];

    if (!model.material.texA.texPath.empty()) {
      auto texEntry = assetManager.getByPath(model.material.texA.texPath);
      if (texEntry && texEntry->texture) {
        part->texBindings[0].texture = texEntry->texture->getGPUTex();
        part->refTex0 = texEntry->texture;
      }
    }

    if (!model.material.texB.texPath.empty()) {
      auto texEntry = assetManager.getByPath(model.material.texB.texPath);
      if (texEntry && texEntry->texture) {
        part->texBindings[1].texture = texEntry->texture->getGPUTex();
        part->refTex1 = texEntry->texture;
      }
    }

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
  const UniformsOverrides& overrides
)
{
  if (!scene)return;

  auto drawPart = [&](MeshPart &part)
  {
    if(part.refTex1.expired() || part.refTex0.expired()) {
      loaded = false;
      return;
    }

    uint32_t flags = uniforms.mat.flags;
    uint32_t blender = uniforms.mat.blender.x;

    if(part.material.flags & UniformN64Material::FLAG_SET_PRIM_COL) {
      lastPrim = part.material.colPrim;
    } else {
      if(overrides.setPrim)lastPrim = overrides.colPrim;
    }

    if(part.material.flags & UniformN64Material::FLAG_SET_ENV_COL) {
      lastEnv = part.material.colEnv;
    } else {
      if(overrides.setEnv)lastEnv = overrides.colEnv;
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
