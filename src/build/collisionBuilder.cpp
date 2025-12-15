/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "projectBuilder.h"
#include "../utils/binaryFile.h"
#include "../utils/fs.h"
#include "../project/assets/collision.h"
#include "tiny3d/tools/gltf_importer/src/cgltfHelper.h"
#include "tiny3d/tools/gltf_importer/src/parser.h"
#include "tiny3d/tools/gltf_importer/src/lib/cgltf.h"
#include "tiny3d/tools/gltf_importer/src/optimizer/optimizer.h"

namespace
{
  struct CollisionMesh
  {
    std::vector<glm::vec3> verticesFloat{};
    std::vector<glm::i16vec3> vertices{};
    std::vector<glm::i16vec3> normals{};
    std::vector<uint16_t> indices{};
  };

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

  void convert(const char* gltfPath, Utils::BinaryFile &file, float baseScale)
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
    std::vector<glm::i16vec3> vertices{};
    std::vector<glm::i16vec3> normals{};
    std::vector<uint16_t> indices{};

    for(int i=0; i<data->nodes_count; ++i)
    {
      auto node = &data->nodes[i];
      if(!node->mesh || (node->name && std::string(node->name).starts_with("fast64_f3d_material_library"))) {
        continue;
      }

      /*if(std::string(node->name).find("coll_") == std::string::npos) {
        continue;
      }*/

      auto nodeMat = parseNodeMatrix(node, {1.0f, 1.0f, 1.0f});
      auto mesh = node->mesh;

      for(int j = 0; j < mesh->primitives_count; j++)
      {
        int baseIndex = vertices.size();
        assert(baseIndex < 0x10000);

        auto prim = &mesh->primitives[j];

        // Read indices
        if(prim->indices != nullptr)
        {
          auto acc = prim->indices;
          auto basePtr = ((uint8_t*)acc->buffer_view->buffer->data) + acc->buffer_view->offset + acc->offset;
          auto elemSize = Gltf::getDataSize(acc->component_type);

          for(int k = 0; k < acc->count; k++) {
            indices.push_back(baseIndex + Gltf::readAsU32(basePtr, acc->component_type));
            basePtr += elemSize;
          }
        }

        for(int k = 0; k < prim->attributes_count; k++)
        {
          auto attr = &prim->attributes[k];
          auto acc = attr->data;
          auto basePtr = ((uint8_t*)acc->buffer_view->buffer->data) + acc->buffer_view->offset + acc->offset;

          if(attr->type == cgltf_attribute_type_position) {
            assert(attr->data->type == cgltf_type_vec3);
            for(int l = 0; l < acc->count; l++) {
              auto vert = Gltf::readAsVec3(basePtr, attr->data->type, acc->component_type);
              vert = nodeMat * vert;

              verticesFloat.push_back({
                vert[0] * baseScale,
                vert[1] * baseScale,
                vert[2] * baseScale
              });
              vertices.push_back({
                (int16_t)(vert[0] * baseScale),
                (int16_t)(vert[1] * baseScale),
                (int16_t)(vert[2] * baseScale)
              });
              basePtr += Gltf::getDataSize(acc->component_type) * 3;
            }
          }
        }

      } // primitives
    } // nodes

    // generate normals
    for(int v=0; v<indices.size(); v+=3) {
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
      normals.push_back({
        (int16_t)(normal[0] * 32767.0f),
        (int16_t)(normal[1] * 32767.0f),
        (int16_t)(normal[2] * 32767.0f)
      });
    }

    assert(indices.size() % 3 == 0);

    printf("Vert/Index count: %lu %lu\n", vertices.size(), indices.size());

    auto bvh = Project::Assets::Collision::createBVH(vertices, indices);

    file.write<uint32_t>(indices.size() / 3);
    file.write<uint32_t>(vertices.size());
    file.write<float>(1.0f);// / baseScale);
    file.write<uint32_t>(0); // vertex pointer
    file.write<uint32_t>(0); // normals pointer
    file.write<uint32_t>(0); // BVH pointer

    file.writeArray(indices.data(), indices.size());
    file.align(4);

    for(auto& n : normals) {
      file.write(n.x);
      file.write(n.y);
      file.write(n.z);
    }
    file.align(4);

    for(auto& v : verticesFloat) {
      file.writeArray(v.data, 3);
    }
    file.align(4);

    file.writeArray(bvh.data(), bvh.size());
    file.align(4);
  }
}

namespace Build
{
  std::vector<uint8_t> buildCollision(const std::string &gltfPath, float baseScale)
  {
    Utils::BinaryFile f{};
    convert(gltfPath.c_str(), f, baseScale);
    return f.getData();
  }
}