/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "n64Mesh.h"

void Renderer::N64Mesh::fromT3DM(const T3DMData &t3dmData)
{
  mesh.vertices.clear();
  mesh.indices.clear();
  parts.clear();


  parts.resize(t3dmData.models.size());
  auto part = parts.begin();

  uint16_t idx = 0;
  for (auto &model : t3dmData.models)
  {
    part->indicesOffset = mesh.indices.size();
    part->indicesCount = model.triangles.size() * 3;

    part->material.colPrim = {
      model.material.primColor[0],
      model.material.primColor[1],
      model.material.primColor[2],
      model.material.primColor[3]
    };
    part->material.colEnv = {
      model.material.envColor[0],
      model.material.envColor[1],
      model.material.envColor[2],
      model.material.envColor[3],
    };

    printf("Tex: %s\n", model.material.texA.texPath.c_str());
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
}

void Renderer::N64Mesh::draw(SDL_GPURenderPass* pass, UniformsObject &uniforms)
{
  for (auto &part : parts) {
    uniforms.mat = part.material;
    mesh.draw(pass, part.indicesOffset, part.indicesCount);
  }
  //mesh.draw(pass);
}
