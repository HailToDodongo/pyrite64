/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>

#include "../project/scene/object.h"
#include "glm/mat4x4.hpp"

namespace Editor::TransformUtils
{
  using ChildLocalOffsetMap = std::unordered_map<uint64_t, glm::vec3>;

  /**
   * Rebuilds an object's world matrix from its resolved transform properties.
   * @param obj Object whose resolved transform should be composed.
   * @return World matrix built from the resolved scale, rotation, and position.
   */
  glm::mat4 composeResolvedObjectMatrix(const Project::Object &obj);

  /**
   * Caches each descendant's position in the local space of its immediate parent.
   * @param obj Parent object whose direct children should be captured.
   * @return Map keyed by descendant UUID with cached local-space positions.
   */
  ChildLocalOffsetMap captureChildLocalOffsets(const Project::Object &obj);

  /**
   * Caches each descendant's position using an explicit parent world matrix.
   * @param obj Parent object whose direct children should be captured.
   * @param mat Parent world matrix used to convert child positions into local space.
   * @return Map keyed by descendant UUID with cached local-space positions.
   */
  ChildLocalOffsetMap captureChildLocalOffsets(const Project::Object &obj, const glm::mat4 &mat);

  /**
   * Rebuilds descendant world positions after the parent transform changed.
   * @param obj Parent object whose descendants should be updated.
   * @param relPosMap Cached descendant positions in the old immediate-parent local space.
   * @param mat New parent world matrix.
   * @param shouldSkipChild Optional callback used to leave some subtrees untouched.
   */
  void applyChildWorldPositions(
    Project::Object &obj,
    const ChildLocalOffsetMap &relPosMap,
    const glm::mat4 &mat,
    const std::function<bool(const Project::Object&)> &shouldSkipChild = {}
  );

  /**
   * Wraps a transform editor so child world positions follow parent edits.
   * @tparam T Edited property value type.
   * @param obj Object whose transform is being edited.
   * @param editFunc UI callback that edits the resolved property value.
   * @param shouldSkipChild Optional callback used to leave some children untouched.
   * @return Wrapped callback with child propagation behavior.
   */
  template<typename T>
  std::function<bool(T*)> preserveChildTransformsDuringEdit(
    Project::Object *obj,
    std::function<bool(T*)> editFunc,
    const std::function<bool(const Project::Object&)> &shouldSkipChild = {}
  ) {
    // Keep the wrapped editor behavior unchanged when there is no target object
    return [obj, editFunc = std::move(editFunc), shouldSkipChild](T *val) -> bool {
      // There is no object to edit --> Abort
      if (!obj) return false;

      // Objects without children do not need the extra capture-and-restore pass
      if (obj->children.empty()) {
        return editFunc(val);
      }

      // Cache descendant positions in the current parent local space before editing
      ChildLocalOffsetMap relPosMap = captureChildLocalOffsets(*obj);

      // Let the original editor change the parent transform value
      bool changed = editFunc(val);
      if (!changed) return false;

      // Recompose the updated parent matrix and rebuild child world positions from it
      applyChildWorldPositions(
        *obj,
        relPosMap,
        composeResolvedObjectMatrix(*obj),
        shouldSkipChild
      );
      return true;
    };
  }
}
