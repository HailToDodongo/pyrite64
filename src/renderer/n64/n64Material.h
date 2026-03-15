/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "../n64Mesh.h"

namespace Renderer::N64Material
{
  void convert(N64Mesh::MeshPart &part, const Project::Assets::Material &t3dMat);
}
