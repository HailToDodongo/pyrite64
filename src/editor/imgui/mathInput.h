/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once

#include <cstdint>
#include <string>

namespace ImGui
{
  /**
   * Evaluates a mathematical expression containing arithmetic operators and parentheses.
   * @param expression Expression to evaluate.
   * @param result Destination for the calculated value.
   * @return True when the complete expression is valid.
   */
  bool EvaluateMathExpression(const std::string &expression, double &result);

  /**
   * Draws a floating-point input that evaluates mathematical expressions in real time.
   * @param label ImGui identifier and optional visible label.
   * @param value Floating-point value updated with valid expression results.
   * @return True when the calculated value changed during this frame.
   */
  bool MathInputFloat(const char *label, float *value);

  /**
   * Draws an integer input that evaluates mathematical expressions in real time.
   * @param label ImGui identifier and optional visible label.
   * @param value Integer value updated with valid expression results.
   * @return True when the calculated value changed during this frame.
   */
  bool MathInputInt(const char *label, int *value);

  /**
   * Draws an unsigned 32-bit input that evaluates mathematical expressions in real time.
   * @param label ImGui identifier and optional visible label.
   * @param value Unsigned value updated with clamped valid expression results.
   * @return True when the calculated value changed during this frame.
   */
  bool MathInputU32(const char *label, uint32_t *value);

  /**
   * Draws an unsigned 16-bit input that evaluates mathematical expressions in real time.
   * @param label ImGui identifier and optional visible label.
   * @param value Unsigned value updated with clamped valid expression results.
   * @return True when the calculated value changed during this frame.
   */
  bool MathInputU16(const char *label, uint16_t *value);

  /**
   * Draws an unsigned 8-bit input that evaluates mathematical expressions in real time.
   * @param label ImGui identifier and optional visible label.
   * @param value Unsigned value updated with clamped valid expression results.
   * @return True when the calculated value changed during this frame.
   */
  bool MathInputU8(const char *label, uint8_t *value);

  /**
   * Draws multiple floating-point expression inputs in a standard vector layout.
   * @param label ImGui identifier shared by the complete vector input.
   * @param values Contiguous floating-point component array.
   * @param components Number of components to draw.
   * @return True when at least one calculated component changed during this frame.
   */
  bool MathInputFloatN(const char *label, float *values, int components);

  /**
   * Draws multiple integer expression inputs in a standard vector layout.
   * @param label ImGui identifier shared by the complete vector input.
   * @param values Contiguous integer component array.
   * @param components Number of components to draw.
   * @return True when at least one calculated component changed during this frame.
   */
  bool MathInputIntN(const char *label, int *values, int components);
}
