#include "mathInput.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include "quickjs.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
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
   * Evaluates restricted arithmetic expressions with an isolated QuickJS context.
   */
  class MathExpressionEvaluator
  {
    private:
      JSRuntime *runtime{};
      JSContext *context{};

      /**
       * Checks that an expression contains only supported arithmetic tokens.
       * @param expression Expression to validate.
       * @return True when no JavaScript identifiers or statements are present.
       */
      bool hasOnlyArithmeticTokens(const std::string &expression) const
      {
        if (expression.empty() || expression.size() > 1024) return false;
        for (unsigned char token : expression) {
          if (std::isdigit(token) || std::isspace(token)) continue;
          switch (token) {
            case '.': case '+': case '-': case '*': case '/':
            case '%': case '^': case '(': case ')': case '#':
            case 'e': case 'E':
              break;
            default:
              return false;
          }
        }
        return true;
      }

    public:
      /**
       * Creates the isolated runtime used by all mathematical inspector inputs.
       */
      MathExpressionEvaluator()
      {
        runtime = JS_NewRuntime();
        if (!runtime) return;
        JS_SetMemoryLimit(runtime, 1024 * 1024);
        context = JS_NewContext(runtime);
      }

      /**
       * Releases the private QuickJS context and runtime.
       */
      ~MathExpressionEvaluator()
      {
        if (context) JS_FreeContext(context);
        if (runtime) JS_FreeRuntime(runtime);
      }

      MathExpressionEvaluator(const MathExpressionEvaluator&) = delete;
      MathExpressionEvaluator& operator=(const MathExpressionEvaluator&) = delete;

      /**
       * Evaluates an arithmetic expression and optionally supplies the value represented by #.
       * @param expression Arithmetic expression to evaluate.
       * @param value Destination for the finite numeric result.
       * @param currentValue Optional value substituted for # tokens.
       * @return True when QuickJS produced a finite number.
       */
      bool evaluate(
        const std::string &expression,
        double &value,
        std::optional<double> currentValue = std::nullopt
      )
      {
        if (!context || !hasOnlyArithmeticTokens(expression)) return false;

        std::string jsExpression{};
        jsExpression.reserve(expression.size() * 2 + 2);
        jsExpression.push_back('(');
        for (char token : expression) {
          if (token == '#') {
            if (!currentValue.has_value()) return false;
            jsExpression += "__currentValue";
          } else if (token == '^') {
            // JavaScript spells exponentiation as ** while the inspector exposes ^
            jsExpression += "**";
          } else {
            jsExpression.push_back(token);
          }
        }
        jsExpression.push_back(')');

        JSValue global = JS_GetGlobalObject(context);
        JS_SetPropertyStr(
          context,
          global,
          "__currentValue",
          JS_NewFloat64(context, currentValue.value_or(0.0))
        );
        JS_FreeValue(context, global);

        JSValue result = JS_Eval(
          context,
          jsExpression.c_str(),
          jsExpression.size(),
          "<math-input>",
          JS_EVAL_TYPE_GLOBAL
        );
        if (JS_IsException(result)) {
          // Reading the exception clears it before the next live evaluation
          JSValue exception = JS_GetException(context);
          JS_FreeValue(context, exception);
          JS_FreeValue(context, result);
          return false;
        }

        bool valid = JS_IsNumber(result)
          && JS_ToFloat64(context, &value, result) == 0
          && std::isfinite(value);
        JS_FreeValue(context, result);
        return valid;
      }
  };

  /**
   * Returns the evaluator shared by mathematical inspector inputs.
   * @return Process-local evaluator instance.
   */
  MathExpressionEvaluator& getMathExpressionEvaluator()
  {
    static MathExpressionEvaluator evaluator{};
    return evaluator;
  }

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
    bool valid = (active || wasActive)
      && getMathExpressionEvaluator().evaluate(state.expression, result, state.baseValue);
    if (valid) {
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
  // Public typed wrappers share the same evaluator and interaction state
  return mathInputScalar(label, value);
}

bool ImGui::EvaluateMathExpression(const std::string &expression, double &result) {
  // Expose the evaluator for custom text fields such as mixed multi-selection values
  return getMathExpressionEvaluator().evaluate(expression, result);
}

bool ImGui::EvaluateMathExpression(
  const std::string &expression,
  double &result,
  double currentValue
) {
  // Supply the per-object base value used by # during multi-selection edits
  return getMathExpressionEvaluator().evaluate(expression, result, currentValue);
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
