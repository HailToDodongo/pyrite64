/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include <vector>
#include <unordered_set>
#include "../utils/binaryFile.h"
#include "tiny3d/tools/gltf_importer/src/math/mat4.h"
#include "tiny3d/tools/gltf_importer/src/cgltfHelper.h"
#include "tiny3d/tools/gltf_importer/src/lib/cgltf.h"
#include "../../n64/engine/include/collision/meshBvhBuilder.h"
#include <cstddef>
#include <cmath>

namespace
{
  namespace {
    Mat4 parseNodeMatrix(const cgltf_node *node, const Vec3 &posScale)
    {
      Mat4 matScale{};
      if(node->has_scale)matScale.setScale({node->scale[0], node->scale[1], node->scale[2]});

      Mat4 matRot{};
      if(node->has_rotation)matRot.setRot({
        node->rotation[0],
        node->rotation[1],
        node->rotation[2],
        node->rotation[3]
      });

      Mat4 matTrans{};
      if(node->has_translation) {
        matTrans.setPos({
          node->translation[0] * posScale[0],
          node->translation[1] * posScale[1],
          node->translation[2] * posScale[2],
        });
      };

      Mat4 res = matTrans * matRot * matScale;
      for(int i=0; i<4; ++i) {
        for(int j=0; j<4; ++j) {
          if(fabs(res.data[i][j]) < 0.0001f)res.data[i][j] = 0.0f;
        }
      }

      return res;
    }
  }

  void convert(
    const char* gltfPath, Utils::BinaryFile &file,
    const std::unordered_set<std::string> &meshes
  )
  {
    cgltf_options options{};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, gltfPath, &data);

    if(result == cgltf_result_file_not_found) {
      throw std::runtime_error("File not found!");
    }
    if(cgltf_validate(data) != cgltf_result_success) {
      throw std::runtime_error("Invalid glTF data!");
    }

    cgltf_load_buffers(&options, data, gltfPath);

    std::vector<Vec3> verticesFloat{};
    std::vector<Vec3> normals{};
    std::vector<uint16_t> indices{};

    for(size_t i=0; i<data->nodes_count; ++i)
    {
      auto node = &data->nodes[i];
      if(!node->mesh || (node->name && std::string(node->name).starts_with("fast64_f3d_material_library"))) {
        continue;
      }

      if(!meshes.empty())
      {
        if(node->name == nullptr || meshes.find(node->name) == meshes.end()) {
          continue;
        }
      }

      auto nodeMat = parseNodeMatrix(node, {1.0f, 1.0f, 1.0f});
      auto mesh = node->mesh;

      for(size_t j = 0; j < mesh->primitives_count; j++)
      {
        const size_t baseIndex = verticesFloat.size();

        auto prim = &mesh->primitives[j];

        // Read indices
        if(prim->indices != nullptr)
        {
          auto acc = prim->indices;
          auto basePtr = ((uint8_t*)acc->buffer_view->buffer->data) + acc->buffer_view->offset + acc->offset;
          auto elemSize = Gltf::getDataSize(acc->component_type);

          for(size_t k = 0; k < acc->count; k++) {
            const size_t index = baseIndex + Gltf::readAsU32(basePtr, acc->component_type);
            if(index >= 0xFFFFu) throw std::runtime_error("Collision mesh exceeds 65535 vertices!");
            indices.push_back(static_cast<uint16_t>(index));
            basePtr += elemSize;
          }
        }

        for(size_t k = 0; k < prim->attributes_count; k++)
        {
          auto attr = &prim->attributes[k];
          auto acc = attr->data;
          auto basePtr = ((uint8_t*)acc->buffer_view->buffer->data) + acc->buffer_view->offset + acc->offset;

          if(attr->type == cgltf_attribute_type_position) {
            assert(attr->data->type == cgltf_type_vec3);
            for(size_t l = 0; l < acc->count; l++) {
              auto vert = Gltf::readAsVec3(basePtr, attr->data->type, acc->component_type);
              vert = nodeMat * vert;

              // collision geometry is exported in meters
              verticesFloat.push_back({vert[0], vert[1], vert[2]});
              if(verticesFloat.size() > 0xFFFFu) throw std::runtime_error("Collision mesh exceeds 65535 vertices!");
              if(!std::isfinite(vert[0]) || !std::isfinite(vert[1]) || !std::isfinite(vert[2])) {
                throw std::runtime_error("Collision mesh contains non-finite vertices!");
              }
              basePtr += Gltf::getDataSize(acc->component_type) * 3;
            }
          }
        }

      } // primitives
    } // nodes

    if(indices.size() % 3 != 0 || indices.size() / 3 > 0xFFFFu) {
      throw std::runtime_error("Collision mesh requires complete triangles and at most 65535 triangles!");
    }
    for(uint16_t index : indices) {
      if(index >= verticesFloat.size()) throw std::runtime_error("Collision mesh has an invalid vertex index!");
    }

    // generate normals
    for(size_t v=0; v<indices.size(); v+=3) {
      Vec3 edge1 = verticesFloat[indices[v+1]] - verticesFloat[indices[v]];
      Vec3 edge2 = verticesFloat[indices[v+2]] - verticesFloat[indices[v]];
      Vec3 edge3 = verticesFloat[indices[v+2]] - verticesFloat[indices[v]];

      if(edge1.length() < 0.01f || edge2.length() < 0.01f || edge3.length() < 0.01f) {
        printf("Degenerate triangle:\nA: %.4f %.4f %.4f\nB: %.4f %.4f %.4f\nC: %.4f %.4f %.4f\n",
          verticesFloat[indices[v]][0], verticesFloat[indices[v]][1], verticesFloat[indices[v]][2],
          verticesFloat[indices[v+1]][0], verticesFloat[indices[v+1]][1], verticesFloat[indices[v+1]][2],
          verticesFloat[indices[v+2]][0], verticesFloat[indices[v+2]][1], verticesFloat[indices[v+2]][2]
        );
        printf("Indices: %d %d %d\n", indices[v], indices[v+1], indices[v+2]);
        throw std::runtime_error("Degenerate triangle!");
      }

      Vec3 normal = edge1.cross(edge2);
      normal = normal * (1.0f / normal.length());
      normals.push_back(normal);
    }

    const auto triangleCount = static_cast<uint16_t>(indices.size() / 3);
    auto bvh = P64::Coll::buildMeshBvh(triangleCount, [&](uint16_t triangle) {
      P64::Coll::MeshBvhNode bounds{};
      for(int axis = 0; axis < 3; ++axis) {
        const float a = verticesFloat[indices[triangle * 3]][axis];
        const float b = verticesFloat[indices[triangle * 3 + 1]][axis];
        const float c = verticesFloat[indices[triangle * 3 + 2]][axis];
        bounds.min[axis] = std::min({a, b, c});
        bounds.max[axis] = std::max({a, b, c});
      }
      return bounds;
    });

    const uint32_t headerPos = file.getPos();
    file.write<uint32_t>(indices.size() / 3);
    file.write<uint32_t>(verticesFloat.size());
    file.write<float>(1.0f);
    file.write<uint32_t>(0); // patched to the BVH offset below

    file.writeArray(indices.data(), indices.size());
    file.align(4);

    for(auto& n : normals) {
      file.writeArray(n.data, 3);
    }
    file.align(4);

    for(auto& v : verticesFloat) {
      file.writeArray(v.data, 3);
    }
    file.align(4);

    const uint32_t bvhOffset = file.getPos() - headerPos;
    file.atPos(headerPos + offsetof(P64::Coll::RawCollisionHeader, bvhOffset), [&] {
      file.write(bvhOffset);
    });
    for(uint32_t i = 0; i < P64::Coll::meshBvhNodeCount(triangleCount); ++i) {
      file.writeArray(bvh[i].min, 3);
      file.writeArray(bvh[i].max, 3);
      file.write(bvh[i].escapeOrTriangle);
    }
  }
}

namespace Build
{
  Utils::BinaryFile buildCollision(
    const std::string &gltfPath,
    const std::unordered_set<std::string> &meshes
  )
  {
    Utils::BinaryFile f{};
    convert(gltfPath.c_str(), f, meshes);
    return f;
  }
}
