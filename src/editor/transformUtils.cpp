/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "transformUtils.h"

#include "glm/gtx/matrix_decompose.hpp"

namespace Editor::TransformUtils
{
  namespace
  {
    /**
     * Recursively caches descendant positions using each node's immediate parent matrix.
     * @param obj Current subtree root being visited.
     * @param parentMat World matrix of the current subtree root's parent.
     * @param relPosMap Accumulator keyed by descendant UUID.
     */
    void captureChildLocalOffsetsRecursive(
      const Project::Object &obj,
      const glm::mat4 &parentMat,
      ChildLocalOffsetMap &relPosMap
    ) {
      // Convert descendants from the current parent's world space into local space once
      const glm::mat4 invParentMat = glm::inverse(parentMat);

      for (const auto &child : obj.children)
      {
        // Cache the child relative to its immediate parent before recursing deeper
        relPosMap[child->uuid] = invParentMat * glm::vec4(
          child->pos.resolve(*child),
          1.0f
        );

        // Recurse with the child's current world matrix so grandchildren use the correct space
        captureChildLocalOffsetsRecursive(*child, composeResolvedObjectMatrix(*child), relPosMap);
      }
    }
  }

  /**
   * Rebuilds an object's world matrix from its resolved transform properties.
   * @param obj Object whose resolved transform should be composed.
   * @return World matrix built from the resolved scale, rotation, and position.
   */
  glm::mat4 composeResolvedObjectMatrix(const Project::Object &obj)
  {
    // GLM's recompose helper also expects skew and perspective components
    glm::vec3 skew{0.0f};
    glm::vec4 persp{0.0f, 0.0f, 0.0f, 1.0f};

    // Rebuild the matrix from the resolved transform values currently visible in the editor
    return glm::recompose(
      obj.scale.resolve(obj),
      obj.rot.resolve(obj),
      obj.pos.resolve(obj),
      skew,
      persp
    );
  }

  /**
   * Caches each direct child's position in the local space of its parent.
   * @param obj Parent object whose direct children should be captured.
   * @return Map keyed by child UUID with cached local-space positions.
   */
  ChildLocalOffsetMap captureChildLocalOffsets(const Project::Object &obj)
  {
    // Reuse the explicit-matrix overload with the object's current resolved world matrix
    return captureChildLocalOffsets(obj, composeResolvedObjectMatrix(obj));
  }

  /**
   * Caches each direct child's position using an explicit parent world matrix.
   * @param obj Parent object whose direct children should be captured.
   * @param mat Parent world matrix used to convert child positions into local space.
   * @return Map keyed by child UUID with cached local-space positions.
   */
  ChildLocalOffsetMap captureChildLocalOffsets(const Project::Object &obj, const glm::mat4 &mat)
  {
    // Store the entire subtree in local space starting from the provided parent matrix
    ChildLocalOffsetMap relPosMap{};
    captureChildLocalOffsetsRecursive(obj, mat, relPosMap);
    return relPosMap;
  }

  /**
   * Rebuilds child world positions after the parent transform changed.
   * @param obj Parent object whose direct children should be updated.
   * @param relPosMap Cached child positions in the old parent local space.
   * @param mat New parent world matrix.
   * @param shouldSkipChild Optional callback used to leave some children untouched.
   */
  void applyChildWorldPositions(
    Project::Object &obj,
    const ChildLocalOffsetMap &relPosMap,
    const glm::mat4 &mat,
    const std::function<bool(const Project::Object&)> &shouldSkipChild
  ) {
    // Process every child of the edited parent
    for (auto &child : obj.children)
    {
      // Keep callers free to preserve specific subtrees exactly as they are
      if (shouldSkipChild && shouldSkipChild(*child)) continue;

      // Skip descendants that were not part of the cached transform state
      auto it = relPosMap.find(child->uuid);
      if (it == relPosMap.end()) continue;

      // Restore the child's world position using the updated parent transform
      child->pos.resolve(*child) = mat * glm::vec4(it->second, 1.0f);

      // Recurse so grandchildren and deeper levels inherit the updated child transform
      applyChildWorldPositions(
        *child,
        relPosMap,
        composeResolvedObjectMatrix(*child),
        shouldSkipChild
      );
    }
  }
}
