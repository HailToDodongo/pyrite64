/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "n64Mesh.h"
#include "../context.h"
#include "../project/assetManager.h"
#include <filesystem>

#include "n64/n64Material.h"

namespace fs = std::filesystem;
extern SDL_GPUSampler *texSamplerRepeat; // @TODO make sampler manager? is this even needed?

void Renderer::N64Mesh::fromT3DM(const T3DMData &t3dmData, Project::AssetManager &assetManager)
{
  loaded = false;
  mesh.vertices.clear();
  mesh.indices.clear();
  parts.clear();

  parts.resize(t3dmData.models.size());
  auto part = parts.begin();

  auto fallbackTex = assetManager.getFallbackTexture().getGPUTex();

  uint16_t idx = 0;
  for (auto &model : t3dmData.models)
  {
    part->indicesOffset = mesh.indices.size();
    part->indicesCount = model.triangles.size() * 3;

    N64Material::convert(*part, model.material);

    part->texBindings[0].texture = fallbackTex;
    part->texBindings[0].sampler = texSamplerRepeat;
    part->texBindings[1].texture = fallbackTex;
    part->texBindings[1].sampler = texSamplerRepeat;

    if (!model.material.texA.texPath.empty()) {
      auto texEntry = assetManager.getByPath(model.material.texA.texPath);
      if (texEntry)part->texBindings[0].texture = texEntry->texture->getGPUTex();
    }

    if (!model.material.texB.texPath.empty()) {
      auto texEntry = assetManager.getByPath(model.material.texB.texPath);
      if (texEntry)part->texBindings[1].texture = texEntry->texture->getGPUTex();
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

void Renderer::N64Mesh::recreate(Renderer::Scene &scene) {
  mesh.recreate(scene);
  loaded = true;
}

void Renderer::N64Mesh::draw(SDL_GPURenderPass* pass, SDL_GPUCommandBuffer *cmdBuff, UniformsObject &uniforms)
{
  for (auto &part : parts) {
    uniforms.mat = part.material;
    SDL_BindGPUFragmentSamplers(pass, 0, part.texBindings, 2);
    SDL_BindGPUVertexSamplers(pass, 0, part.texBindings, 2); // needed?

    SDL_PushGPUVertexUniformData(cmdBuff, 1, &uniforms, sizeof(uniforms));
    SDL_PushGPUFragmentUniformData(cmdBuff, 0, &uniforms, sizeof(uniforms));

    // @TODO: lighting
    uniforms.mat.ambientColor = {0.2f, 0.2f, 0.2f, 1.0f};
    uniforms.mat.lightColor[0] = {0,0,0,1};
    uniforms.mat.lightColor[1] = {0,0,0,1};

    mesh.draw(pass, part.indicesOffset, part.indicesCount);
  }
  //mesh.draw(pass);
}
