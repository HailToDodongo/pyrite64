/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "autoScale.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "tiny3d/tools/gltf_importer/src/lib/cgltf.h"

namespace
{
  constexpr float FALLBACK_BASE_SCALE = 64.0f;
  // margin below the int16 limit, animated vertices can move outside the rest-pose bounds
  constexpr float MAX_QUANTIZED_EXTENT = 30000.0f;
  constexpr float MIN_BASE_SCALE = 1.0f;
  constexpr float MAX_BASE_SCALE = 512.0f;
}

float Build::computeAutoBaseScale(const std::string &gltfPath)
{
  cgltf_options options{};
  cgltf_data* data = nullptr;
  // accessor min/max live in the JSON, no buffer loading needed
  if(cgltf_parse_file(&options, gltfPath.c_str(), &data) != cgltf_result_success) {
    return FALLBACK_BASE_SCALE;
  }

  float maxAbs = 0.0f;
  for(size_t i=0; i<data->nodes_count; ++i)
  {
    auto node = &data->nodes[i];
    if(!node->mesh || (node->name && std::string(node->name).starts_with("fast64_f3d_material_library"))) {
      continue;
    }

    glm::mat4 world{};
    cgltf_node_transform_world(node, glm::value_ptr(world));

    for(size_t j=0; j<node->mesh->primitives_count; ++j)
    {
      auto prim = &node->mesh->primitives[j];
      for(size_t k=0; k<prim->attributes_count; ++k)
      {
        auto attr = &prim->attributes[k];
        if(attr->type != cgltf_attribute_type_position)continue;
        auto acc = attr->data;
        if(!acc->has_min || !acc->has_max)continue;

        glm::vec3 min{acc->min[0], acc->min[1], acc->min[2]};
        glm::vec3 max{acc->max[0], acc->max[1], acc->max[2]};
        for(int corner=0; corner<8; ++corner) {
          glm::vec3 p{
            (corner & 1) ? max.x : min.x,
            (corner & 2) ? max.y : min.y,
            (corner & 4) ? max.z : min.z
          };
          glm::vec3 world_p = glm::vec3(world * glm::vec4(p, 1.0f));
          maxAbs = std::max({maxAbs, fabsf(world_p.x), fabsf(world_p.y), fabsf(world_p.z)});
        }
      }
    }
  }
  cgltf_free(data);

  if(maxAbs <= 0.0f) {
    return FALLBACK_BASE_SCALE;
  }

  float scale = exp2f(floorf(log2f(MAX_QUANTIZED_EXTENT / maxAbs)));
  return std::clamp(scale, MIN_BASE_SCALE, MAX_BASE_SCALE);
}
