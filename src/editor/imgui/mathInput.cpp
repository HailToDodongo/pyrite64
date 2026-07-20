#include "mathInput.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>
#include <type_traits>
#include <unordered_map>

namespace
{
  // Keeps the typed expression alive while its ImGui field remains active
  struct MathInputState
  {
    std::string expression{};
    double baseValue{};
    bool active{false};
  };

  // ImGui IDs distinguish repeated labels across tables, components and vector axes
  std::unordered_map<ImGuiID, MathInputState> mathInputStates{};
  ImGui::MathInputActivity mathInputActivity{};

  /**
   * Parses arithmetic expressions through recursive descent and operator precedence.
   */
  class MathExpressionParser
  {
    private:
      const char *cursor{};
      std::optional<double> currentValue{};

      /**
       * Advances the parser cursor past consecutive whitespace characters.
       */
      void skipSpaces()
      {
        // Whitespace is allowed between every token
        while (*cursor && std::isspace(static_cast<unsigned char>(*cursor)))
          ++cursor;
      }

      /**
       * Consumes a token when it is the next non-space character.
       * @param token Character expected at the current parser position.
       * @return True when the token was found and consumed.
       */
      bool consume(char token)
      {
        // Advance only when the next non-space character matches the requested token
        skipSpaces();
        if (*cursor != token) return false;
        ++cursor;
        return true;
      }

      /**
       * Parses addition and subtraction expressions.
       * @param value Destination for the evaluated expression.
       * @return True when this precedence level was parsed successfully.
       */
      bool parseExpression(double &value)
      {
        // Addition and subtraction are evaluated after all higher-precedence operations
        if (!parseTerm(value)) return false;
        
        while (true) {
          if (consume('+')) {
            double rhs{};
            if (!parseTerm(rhs)) return false;
            value += rhs;
          } else if (consume('-')) {
            double rhs{};
            if (!parseTerm(rhs)) return false;
            value -= rhs;
          } else {
            return std::isfinite(value);
          }
        }
      }

      /**
       * Parses multiplication, division and remainder expressions.
       * @param value Destination for the evaluated term.
       * @return True when this precedence level was parsed successfully.
       */
      bool parseTerm(double &value)
      {
        // Multiplication, division and remainder share precedence and associate to the left
        if (!parseUnary(value)) return false;

        while (true) {
          if (consume('*')) {
            double rhs{};
            if (!parseUnary(rhs)) return false;
            value *= rhs;
          } else if (consume('/')) {
            double rhs{};
            // Division by zero invalidates the expression without changing the property
            if (!parseUnary(rhs) || rhs == 0.0) return false;
            value /= rhs;
          } else if (consume('%')) {
            double rhs{};
            if (!parseUnary(rhs) || rhs == 0.0) return false;
            value = std::fmod(value, rhs);
          } else {
            return std::isfinite(value);
          }
        }
      }

      /**
       * Parses recursive unary plus and minus operators.
       * @param value Destination for the evaluated unary expression.
       * @return True when this precedence level was parsed successfully.
       */
      bool parseUnary(double &value)
      {
        // Recursive unary parsing accepts chains such as --2 and preserves power precedence
        if (consume('+')) return parseUnary(value);
        if (consume('-')) {
          if (!parseUnary(value)) return false;
          value = -value;
          return true;
        }
        return parsePower(value);
      }

      /**
       * Parses right-associative exponentiation expressions.
       * @param value Destination for the evaluated power expression.
       * @return True when this precedence level was parsed successfully.
       */
      bool parsePower(double &value)
      {
        // Parsing the exponent through unary makes powers right-associative and allows 2^-2
        if (!parsePrimary(value)) return false;
        if (consume('^')) {
          double exponent{};
          if (!parseUnary(exponent)) return false;
          value = std::pow(value, exponent);
        }
        return std::isfinite(value);
      }

      /**
       * Parses a current-value token, numeric literal or parenthesised expression.
       * @param value Destination for the evaluated primary expression.
       * @return True when a complete primary value was parsed successfully.
       */
      bool parsePrimary(double &value)
      {
        // A primary value can be the supplied current value, a parenthesised expression or a number
        skipSpaces();
        if (consume('#')) {
          if (!currentValue.has_value()) return false;
          value = *currentValue;
          return std::isfinite(value);
        }
        if (consume('(')) {
          if (!parseExpression(value) || !consume(')')) return false;
          return true;
        }

        char *numberEnd{};
        // strtod also supports decimal and scientific notation
        value = std::strtod(cursor, &numberEnd);
        if (numberEnd == cursor) return false;
        cursor = numberEnd;
        return std::isfinite(value);
      }

    public:
      /**
       * Creates a parser positioned at the beginning of an expression.
       * @param expression Expression whose storage remains valid during parsing.
       * @param currentValue Optional value substituted for # tokens.
       */
      explicit MathExpressionParser(
        const std::string &expression,
        std::optional<double> currentValue = std::nullopt
      )
        : cursor{expression.c_str()}, currentValue{currentValue}
      {
      }

      /**
       * Evaluates the complete expression and rejects trailing invalid tokens.
       * @param value Destination for the evaluated result.
       * @return True when the complete expression is valid and finite.
       */
      bool parse(double &value)
      {
        // A valid result must consume the complete input except for trailing whitespace
        if (!parseExpression(value)) return false;
        skipSpaces();
        return *cursor == '\0' && std::isfinite(value);
      }
  };

  /**
   * Formats a numeric value using its shortest round-trippable representation.
   * @tparam T Numeric value type.
   * @param value Value to format.
   * @return Text suitable for displaying and parsing again without precision loss.
   */
  template<typename T>
  std::string formatMathValue(T value)
  {
    if constexpr (std::is_floating_point_v<T>) {
      // Normalise negative zero before presenting the confirmed result
      if (value == static_cast<T>(0)) value = static_cast<T>(0);
    }

    // The default general format produces the shortest representation that round-trips
    char buffer[64]{};
    auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (error == std::errc{}) return std::string(buffer, end);
    return "0";
  }

  /**
   * Converts an evaluated double to a bounded destination numeric type.
   * @tparam T Destination numeric type.
   * @param value Evaluated expression result.
   * @return Truncated when integral and clamped value converted to T.
   */
  template<typename T>
  T convertMathValue(double value)
  {
    if constexpr (std::is_integral_v<T>) {
      // Integer fields follow normal truncation towards zero
      value = std::trunc(value);
    }

    // Clamp before casting so large or negative expressions cannot wrap integer fields
    long double bounded = std::clamp(
      static_cast<long double>(value),
      static_cast<long double>(std::numeric_limits<T>::lowest()),
      static_cast<long double>(std::numeric_limits<T>::max())
    );
    return static_cast<T>(bounded);
  }

  /**
   * Draws one expression-aware scalar input and updates its value in real time.
   * @tparam T Numeric value type.
   * @param label ImGui identifier and optional visible label.
   * @param value Value updated by valid expressions.
   * @return True when the calculated value changed during this frame.
   */
  template<typename T>
  bool mathInputScalar(const char *label, T *value)
  {
    // Record that the surrounding inspector control uses a mathematical input
    ++mathInputActivity.uses;

    // Keep one editable expression per concrete widget
    ImGuiID id = ImGui::GetID(label);
    auto [stateIt, inserted] = mathInputStates.try_emplace(id);
    MathInputState &state = stateIt->second;
    if (inserted || !state.active) {
      // Inactive fields mirror external changes made by gizmos, scripts or other inspectors
      state.expression = formatMathValue(*value);
      state.baseValue = static_cast<double>(*value);
    }

    // Auto-select preserves the normal behaviour of numeric ImGui inputs on activation
    bool wasActive = state.active;
    ImGui::InputText(label, &state.expression, ImGuiInputTextFlags_AutoSelectAll);
    bool active = ImGui::IsItemActive();

    bool changed = false;
    double result{};
    bool valid = MathExpressionParser{state.expression, state.baseValue}.parse(result);
    if ((active || wasActive) && valid) {
      // Apply every complete intermediate expression so the viewport updates in real time
      T calculated = convertMathValue<T>(result);
      if (*value != calculated) {
        *value = calculated;
        changed = true;
      }
    }

    bool enterPressed = (active || wasActive)
      && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter));
    bool confirmed = (active || wasActive)
      && (enterPressed || ImGui::IsItemDeactivated());
    if (confirmed) {
      // Replace the expression with its calculated value on Enter or focus loss
      state.expression = formatMathValue(*value);
      ++mathInputActivity.confirmations;
      if (active) ImGui::ClearActiveID();
      active = false;
    }

    state.active = active;
    return changed;
  }

  /**
   * Draws several scalar expression inputs using the standard ImGui vector layout.
   * @tparam T Numeric component type.
   * @param label ImGui identifier shared by the group.
   * @param values Contiguous component array.
   * @param components Number of components to draw.
   * @param input Typed scalar input function.
   * @return True when at least one component changed during this frame.
   */
  template<typename T>
  bool mathInputScalarN(const char *label, T *values, int components, bool (*input)(const char*, T*))
  {
    // Reproduce the standard ImGui multi-component layout with one expression per axis
    bool changed = false;
    ImGui::BeginGroup();
    ImGui::PushID(label);
    ImGui::PushMultiItemsWidths(components, ImGui::CalcItemWidth());
    for (int component = 0; component < components; ++component) {
      ImGui::PushID(component);
      if (component > 0) ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
      changed |= input("##value", &values[component]);
      ImGui::PopItemWidth();
      ImGui::PopID();
    }
    ImGui::PopID();
    ImGui::EndGroup();
    return changed;
  }
}

ImGui::MathInputActivity ImGui::GetMathInputActivity() {
  return mathInputActivity;
}

bool ImGui::MathInputFloat(const char *label, float *value) {
  // Public typed wrappers share the same parser and interaction state
  return mathInputScalar(label, value);
}

bool ImGui::EvaluateMathExpression(const std::string &expression, double &result) {
  // Expose the parser for custom text fields such as mixed multi-selection values
  return MathExpressionParser{expression}.parse(result);
}

bool ImGui::EvaluateMathExpression(
  const std::string &expression,
  double &result,
  double currentValue
) {
  // Supply the per-object base value used by # during multi-selection edits
  return MathExpressionParser{expression, currentValue}.parse(result);
}

bool ImGui::MathInputInt(const char *label, int *value) {
  return mathInputScalar(label, value);
}

bool ImGui::MathInputU32(const char *label, uint32_t *value) {
  return mathInputScalar(label, value);
}

bool ImGui::MathInputU16(const char *label, uint16_t *value) {
  return mathInputScalar(label, value);
}

bool ImGui::MathInputU8(const char *label, uint8_t *value) {
  return mathInputScalar(label, value);
}

bool ImGui::MathInputFloatN(const char *label, float *values, int components) {
  // Build vector inputs from the scalar float implementation
  return mathInputScalarN(label, values, components, MathInputFloat);
}

bool ImGui::MathInputIntN(const char *label, int *values, int components) {
  // Build vector inputs from the scalar integer implementation
  return mathInputScalarN(label, values, components, MathInputInt);
}
