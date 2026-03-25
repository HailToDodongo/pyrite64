/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "object.h"
#include "../context.h"

#include "glm/ext/matrix_transform.hpp"

void Renderer::Object::draw(
  SDL_GPURenderPass* pass,
  SDL_GPUCommandBuffer* cmdBuff,
  const Project::Assets::Model3D *model,
  const std::vector<uint32_t> &parts
) {
  if (!mesh && !n64Mesh) return;

  if (transformDirty) {
    auto m = glm::identity<glm::mat4>();
    m = glm::scale(m, {scale,scale,scale});
    m = glm::translate(m, pos);
    uniform.modelMat = m;
    transformDirty = false;
  }

  if(mesh) {
    SDL_PushGPUVertexUniformData(cmdBuff, 1, &uniform, sizeof(uniform));
    mesh->draw(pass);
  }
  if(n64Mesh && model) {
    n64Mesh->draw(pass, cmdBuff, uniform, parts, *model, matInstance);
  }
}
