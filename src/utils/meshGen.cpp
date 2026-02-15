/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "meshGen.h"

#include <cmath>

#include "../renderer/mesh.h"

void Utils::Mesh::generateCube(Renderer::Mesh&mesh, float size) {
  mesh.vertices.clear();
  mesh.indices.clear();

  for (int i=0; i<6; i++) {
    glm::vec3 normal{};
    glm::vec3 tangent{};
    glm::vec3 bitangent{};

    switch(i) {
      case 0: normal = {0, 0, 1}; tangent = {1, 0, 0}; bitangent = {0, 1, 0}; break; // front
      case 1: normal = {0, 0, -1}; tangent = {-1, 0, 0}; bitangent = {0, 1, 0}; break; // back
      case 2: normal = {1, 0, 0}; tangent = {0, 0, -1}; bitangent = {0, 1, 0}; break; // right
      case 3: normal = {-1, 0, 0}; tangent = {0, 0, 1}; bitangent = {0, 1, 0}; break; // left
      case 4: normal = {0, 1, 0}; tangent = {1, 0, 0}; bitangent = {0, 0, -1}; break; // top
      case 5: normal = {0, -1, 0}; tangent = {1, 0, 0}; bitangent = {0, 0, 1}; break; // bottom
    }

    uint16_t startIdx = mesh.vertices.size();

    auto v0 = glm::ivec3((normal - tangent - bitangent) * size * 0.5f);
    auto v1 = glm::ivec3((normal + tangent - bitangent) * size * 0.5f);
    auto v2 = glm::ivec3((normal + tangent + bitangent) * size * 0.5f);
    auto v3 = glm::ivec3((normal - tangent + bitangent) * size * 0.5f);

    uint16_t norm = 0;
    mesh.vertices.push_back({v0, norm, {0xFF, 0xFF,0xFF,0xFF}, {0,0}});
    mesh.vertices.push_back({v1, norm, {0xFF, 0xFF,0xFF,0xFF}, {32,0}});
    mesh.vertices.push_back({v2, norm, {0xFF, 0xFF,0xFF,0xFF}, {32,32}});
    mesh.vertices.push_back({v3, norm, {0xFF, 0xFF,0xFF,0xFF}, {0,32}});

    mesh.indices.push_back(startIdx + 2);
    mesh.indices.push_back(startIdx + 0);
    mesh.indices.push_back(startIdx + 1);

    mesh.indices.push_back(startIdx + 0);
    mesh.indices.push_back(startIdx + 2);
    mesh.indices.push_back(startIdx + 3);
  }
}

void Utils::Mesh::generateGrid(Renderer::Mesh&mesh, int size) {

  glm::u8vec4 col{0x80,0x80,0x80,0xFF};
  glm::u8vec4 colX{0xFF, 0, 0, 0xFF};
  glm::u8vec4 colY{0, 0xFF, 0, 0xFF};
  glm::u8vec4 colZ{0, 0, 0xFF, 0xFF};


  for (int z=-size; z<=size; z++) {
    mesh.vertLines.push_back({{ -size, 0.0f, (float)z }, 0, z == 0 ? colX : col});
    mesh.vertLines.push_back({{  size, 0.0f, (float)z }, 0, z == 0 ? colX : col});
    mesh.indices.push_back(mesh.vertLines.size() - 2);
    mesh.indices.push_back(mesh.vertLines.size() - 1);
  }
  for (int x=-size; x<=size; x++) {
    mesh.vertLines.push_back({{ (float)x, 0.0f, -size }, 0, x == 0 ? colZ : col});
    mesh.vertLines.push_back({{ (float)x, 0.0f,  size }, 0, x == 0 ? colZ : col});
    mesh.indices.push_back(mesh.vertLines.size() - 2);
    mesh.indices.push_back(mesh.vertLines.size() - 1);
  }

  mesh.vertLines.push_back({{ 0.0f, -size, 0.0f }, 0, colY});
  mesh.vertLines.push_back({{ 0.0f,  size, 0.0f }, 0, colY});
  mesh.indices.push_back(mesh.vertLines.size() - 2);
  mesh.indices.push_back(mesh.vertLines.size() - 1);
}

void Utils::Mesh::addLineSphere(Renderer::Mesh &mesh, const glm::vec3 &pos, const glm::vec3 &halfExtend,
  const glm::u8vec4 &color)
{

  float radius = halfExtend.y;
  int steps = 16;
  float step = 2.0f * (float)M_PI / steps;
  glm::vec3 last = pos + glm::vec3{radius, 0, 0};
  for(int i=1; i<=steps; ++i) {
    float angle = i * step;
    glm::vec3 next = pos + glm::vec3{radius * cosf(angle), 0, radius * sinf(angle)};
    addLine(mesh, last, next, color);
    last = next;
  }
  last = pos + glm::vec3{0, radius, 0};
  for(int i=1; i<=steps; ++i) {
    float angle = i * step;
    glm::vec3 next = pos + glm::vec3{0, radius * cosf(angle), radius * sinf(angle)};
    addLine(mesh, last, next, color);
    last = next;
  }
  last = pos + glm::vec3{radius, 0, 0};
  for(int i=1; i<=steps; ++i) {
    float angle = i * step;
    glm::vec3 next = pos + glm::vec3{radius * cosf(angle), radius * sinf(angle), 0};
    addLine(mesh, last, next, color);
    last = next;
  }
}

void Utils::Mesh::addLineBox(
  Renderer::Mesh&mesh, const glm::vec3&pos, const glm::vec3&halfExtend,
  const glm::u8vec4 &color
) {
  uint16_t startIdx = mesh.vertLines.size();

  glm::vec3 v0 = pos + glm::vec3(-halfExtend.x, -halfExtend.y, -halfExtend.z);
  glm::vec3 v1 = pos + glm::vec3( halfExtend.x, -halfExtend.y, -halfExtend.z);
  glm::vec3 v2 = pos + glm::vec3( halfExtend.x,  halfExtend.y, -halfExtend.z);
  glm::vec3 v3 = pos + glm::vec3(-halfExtend.x,  halfExtend.y, -halfExtend.z);
  glm::vec3 v4 = pos + glm::vec3(-halfExtend.x, -halfExtend.y,  halfExtend.z);
  glm::vec3 v5 = pos + glm::vec3( halfExtend.x, -halfExtend.y,  halfExtend.z);
  glm::vec3 v6 = pos + glm::vec3( halfExtend.x,  halfExtend.y,  halfExtend.z);
  glm::vec3 v7 = pos + glm::vec3(-halfExtend.x,  halfExtend.y,  halfExtend.z);

  mesh.vertLines.push_back({v0, 0, color});
  mesh.vertLines.push_back({v1, 0, color});
  mesh.vertLines.push_back({v2, 0, color});
  mesh.vertLines.push_back({v3, 0, color});
  mesh.vertLines.push_back({v4, 0, color});
  mesh.vertLines.push_back({v5, 0, color});
  mesh.vertLines.push_back({v6, 0, color});
  mesh.vertLines.push_back({v7, 0, color});

  constexpr uint16_t INDICES[] = {
    0,1, 1,2, 2,3, 3,0,
    4,5, 5,6, 6,7, 7,4,
    0,4, 1,5, 2,6, 3,7
  };
  for (auto idx : INDICES) {
    mesh.indices.push_back(startIdx + idx);
  }
}

void Utils::Mesh::addLine(Renderer::Mesh &mesh, const glm::vec3 &start, const glm::vec3 &end, const glm::u8vec4 &color)
{
  uint16_t startIdx = mesh.vertLines.size();

  mesh.vertLines.push_back({start, 0, color});
  mesh.vertLines.push_back({end, 0, color});

  mesh.indices.push_back(startIdx + 0);
  mesh.indices.push_back(startIdx + 1);
}

void Utils::Mesh::addSprite(Renderer::Mesh &mesh, const glm::vec3 &pos, uint32_t objectId, uint8_t spriteIdx, const glm::u8vec4 &color)
{
  glm::u8vec4 col{color.r, color.g, color.b, spriteIdx};
  uint16_t idx = mesh.vertLines.size();

  mesh.vertLines.push_back({.pos = pos, .objectId = objectId, .color = col});
  mesh.vertLines.push_back({.pos = pos, .objectId = objectId, .color = col});
  mesh.vertLines.push_back({.pos = pos, .objectId = objectId, .color = col});
  mesh.vertLines.push_back({.pos = pos, .objectId = objectId, .color = col});

  mesh.indices.push_back(idx+0);
  mesh.indices.push_back(idx+1);
  mesh.indices.push_back(idx+2);
  mesh.indices.push_back(idx+2);
  mesh.indices.push_back(idx+3);
  mesh.indices.push_back(idx+0);
}

