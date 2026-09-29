/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "mesh.h"
#include "../context.h"
#include "scene.h"

void Renderer::Mesh::recreate(Renderer::Scene &scene, bool clearData) {
  if (indices.empty()) {
    dataReady = false;
    return;
  }

  if (!vertBuff) {
    vertBuff = new VertBuffer(ctx.gpu);
  }

  /*printf("%p Mesh::recreate: Verts=%zu Indices=%zu Lines=%zu\n",
    this, vertices.size(), indices.size(), vertLines.size()
  );*/

  if (!vertLines.empty()) {
    vertBuff->setData(vertLines, indices);
  } else {
    aabb.reset();
    for (const auto& v : vertices) {
      aabb.addPoint(glm::vec3(v.pos) * vertexScale);
    }

    vertBuff->setData(vertices, indices);
  }

  scene.addOneTimeCopyPass([this, clearData](SDL_GPUCommandBuffer* cmdBuff, SDL_GPUCopyPass *copyPass){
    if(!vertBuff)return;
    vertBuff->upload(*copyPass);
    dataReady = true;

    if (clearData) {
       indices.clear();
       vertices.clear();
       vertLines.clear();
     }
  });
}

void Renderer::Mesh::draw(SDL_GPURenderPass* pass, uint32_t indexOffset, uint32_t indexCount) {
  if (!dataReady || !vertBuff)return;

  if (indexCount == 0) {
    indexCount = vertBuff->getIndexCount() - indexOffset;
  }

  SDL_GPUBufferBinding bufferBindings[2];
  vertBuff->addBindings(bufferBindings);

  SDL_BindGPUVertexBuffers(pass, 0, bufferBindings, 1);
  SDL_BindGPUIndexBuffer(pass, &bufferBindings[1], SDL_GPU_INDEXELEMENTSIZE_16BIT);

  //SDL_DrawGPUPrimitives(pass, vertices.size(), 1, 0, 0); // unindexed
  SDL_DrawGPUIndexedPrimitives(pass, indexCount, 1, indexOffset, 0, 0);
}

Renderer::Mesh::Mesh() {
}

Renderer::Mesh::~Mesh() {
  delete vertBuff;
}
