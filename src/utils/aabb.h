/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "glm/vec3.hpp"
#include "glm/gtc/matrix_transform.hpp"

namespace Utils
{
  struct AABB
  {
    glm::vec3 min{};
    glm::vec3 max{};

    AABB() {
      reset();
    }

    void reset()
    {
      min = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
      max = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
    }

    glm::vec3 getCenter() const {
      return (min + max) * 0.5f;
    }

    glm::vec3 getHalfExtend() const {
      return (max - min) * 0.5f;
    }

    void addPoint(const glm::vec3 &p)
    {
      if(p.x < min.x)min.x = p.x;
      if(p.y < min.y)min.y = p.y;
      if(p.z < min.z)min.z = p.z;

      if(p.x > max.x)max.x = p.x;
      if(p.y > max.y)max.y = p.y;
      if(p.z > max.z)max.z = p.z;
    }

    void transform(const glm::mat4 &matrix) {
      glm::vec3 corners[8] = {
        {min.x, min.y, min.z}, {max.x, min.y, min.z},
        {min.x, max.y, min.z}, {max.x, max.y, min.z},
        {min.x, min.y, max.z}, {max.x, min.y, max.z},
        {min.x, max.y, max.z}, {max.x, max.y, max.z}
      };

      reset();
      for (const auto &corner : corners) {
        addPoint(glm::vec3(matrix * glm::vec4(corner, 1.0f)));
      }
    }
  };
}