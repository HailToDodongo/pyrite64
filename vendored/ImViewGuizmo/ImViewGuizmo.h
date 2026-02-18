#pragma once

/*===============================================
ImViewGuizmo Single-Header Library by Marcel Kazemi

To use, do this in one (and only one) of your C++ files:
#define IMVIEWGUIZMO_IMPLEMENTATION
 #include "ImViewGuizmo.h"

In all other files, just include the header as usual:
#include "ImViewGuizmo.h"

Copyright (c) 2025 Marcel Kazemi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
======================================================*/

#define GLM_ENABLE_EXPERIMENTAL
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/norm.hpp>


namespace ImViewGuizmo {

    using namespace glm;

    // INTERFACE
    
    struct Style {
        float scale = 1.f;
        
        // Axis visuals
        float lineLength = 0.5f;
        float lineWidth = 4.0f;
        float circleRadius = 15.0f;
        float fadeFactor = 0.25f;

        // Highlight
        ImU32 highlightColor   = IM_COL32(255, 255, 0, 255);
        float highlightWidth   = 2.0f;
        
        // Axis
        ImU32 axisColors[3] = {
            IM_COL32(230, 51, 51, 255),   // X
            IM_COL32(51, 230, 51, 255),   // Y
            IM_COL32(51, 128, 255, 255)   // Z
        };

        // Labels
        float labelSize = 1.0f;
        const char* axisLabels[3] = {"X", "Y", "Z"};
        ImU32 labelColor =  IM_COL32(255, 255, 255, 255);

        //Big Circle
        float bigCircleRadius  = 80.0f;
        ImU32 bigCircleColor = IM_COL32(255, 255, 255, 50);

        // Animation
        bool animateSnap = true;
        float snapAnimationDuration = 0.3f; // in seconds

        // Zoom/Pan Button Visuals
        float toolButtonRadius = 25.f;
        float toolButtonInnerPadding = 4.f;
        ImU32 toolButtonColor = IM_COL32(144, 144, 144, 50);
        ImU32 toolButtonHoveredColor = IM_COL32(215, 215, 215, 50);
        ImU32 toolButtonIconColor = IM_COL32(215, 215, 215, 225);
    };

    inline Style& GetStyle() {
        static Style style;
        return style;
    }

    // Gizmo Axis Struct
    struct GizmoAxis {
        int id;         // 0-5 for (+X,-X,+Y,-Y,+Z,-Z), 6=center
        int axisIndex;  // 0=X, 1=Y, 2=Z
        float depth;    // Screen-space depth
        vec3 direction; // 3D vector
    };

    enum ActiveTool {
        TOOL_NONE,
        TOOL_GIZMO,
        TOOL_ZOOM,
        TOOL_PAN
    };

    static constexpr float baseSize = 256.f;
    static constexpr vec3 origin = {0.0f, 0.0f, 0.0f};
    static constexpr vec3 worldRight = vec3(1.0f,  0.0f,  0.0f); // +X
    static constexpr vec3 worldUp = vec3(0.0f, 1.0f,  0.0f); // -Y
    static constexpr vec3 worldForward = vec3(0.0f,  0.0f,  1.0f); // +Z
    static constexpr vec3 axisVectors[3] = {{1,0,0}, {0,1,0}, {0,0,1}};

    struct Context {
        int hoveredAxisID = -1;
        bool isZoomButtonHovered = false;
        bool isPanButtonHovered = false;
        ActiveTool activeTool = TOOL_NONE;
        
        // Animation state
        bool isAnimating = false;
        float animationStartTime = 0.f;
        vec3 startPos;
        vec3 targetPos;
        vec3 startUp;
        vec3 targetUp;

        bool IsHoveringGizmo() const { return hoveredAxisID != -1; }
        void Reset() { 
            hoveredAxisID = -1; 
            isZoomButtonHovered = false;
            isPanButtonHovered = false;
            activeTool = TOOL_NONE;
        }
    };

    // Global context
    inline Context& GetContext() {
        static Context ctx;
        return ctx;
    }

    inline bool IsUsing() {
        return GetContext().activeTool != TOOL_NONE;
    }

    inline bool IsOver() {
        const Context& ctx = GetContext();
        return ctx.hoveredAxisID != -1 || ctx.isZoomButtonHovered || ctx.isPanButtonHovered;
    }

    /// @brief Renders and handles the view gizmo logic.
    /// @param cameraPos The position of the camera (will be modified).
    /// @param cameraRot The rotation of the camera (will be modified).
    /// @param position The top-left screen position where the gizmo should be rendered.
    /// @param snapDistance The distance the camera will snap to when an axis is clicked.
    /// @param rotationSpeed The rotation speed when dragging the gizmo.
    /// @return True if the camera was modified, false otherwise.
    bool Rotate(vec3& cameraPos, quat& cameraRot, ImVec2 position, float snapDistance =  5.f, float rotationSpeed = 0.005f);

    /// @brief Renders a zoom button and handles its logic.
    /// @param cameraPos The position of the camera (will be modified).
    /// @param cameraRot The rotation of the camera (used for direction).
    /// @param position The top-left screen position of the button.
    /// @param zoomSpeed The speed/sensitivity of the zoom.
    /// @return True if the camera was modified, false otherwise.
    bool Zoom(vec3& cameraPos, const quat& cameraRot, ImVec2 position, float zoomSpeed = 0.005f);
    
    /// @brief Renders a pan button and handles its logic.
    /// @param cameraPos The position of the camera (will be modified).
    /// @param position The top-left screen position of the button.
    /// @param panSpeed The speed/sensitivity of the pan.
    /// @return True if the camera was modified, false otherwise.
    bool Pan(vec3& cameraPos,  const quat& cameraRot, ImVec2 position, float panSpeed = 0.001f);

} // namespace ImViewGuizmo


#ifdef IMVIEWGUIZMO_IMPLEMENTATION
namespace ImViewGuizmo {

    // Internal function to reset hover states once per frame
    static void BeginFrame() {
        static int lastFrame = -1;
        int currentFrame = ImGui::GetFrameCount();
        if (lastFrame != currentFrame) {
            lastFrame = currentFrame;
            Context& ctx = GetContext();
            // Reset hover states, but keep active tool state
            ctx.hoveredAxisID = -1;
            ctx.isZoomButtonHovered = false;
            ctx.isPanButtonHovered = false;
        }
    }

    bool Rotate(vec3& cameraPos, quat& cameraRot, ImVec2 position, float snapDistance, float rotationSpeed)
    {
        BeginFrame();
        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        Context& ctx = GetContext();
        Style& style = GetStyle();
        bool wasModified = false;

        // Global release check
        if (!ImGui::IsMouseDown(0) && ctx.activeTool != TOOL_NONE)
            ctx.activeTool = TOOL_NONE;
    

        // Animation logic
        if (ctx.isAnimating) {
            float elapsedTime = static_cast<float>(ImGui::GetTime()) - ctx.animationStartTime;
            float t = std::min(1.0f, elapsedTime / style.snapAnimationDuration);
            t = 1.0f - (1.0f - t) * (1.0f - t);

            vec3 currentDir;
            if (length(ctx.startPos) > 0.0001f && length(ctx.targetPos) > 0.0001f) {
                vec3 startDir = normalize(ctx.startPos);
                vec3 targetDir = normalize(ctx.targetPos);
                currentDir = normalize(mix(startDir, targetDir, t));
            } else
                currentDir = normalize(mix(vec3(0,0,1), normalize(ctx.targetPos), t));
            float startDistance = length(ctx.startPos);
            float targetDistance = length(ctx.targetPos);
            float currentDistance = mix(startDistance, targetDistance, t);
            cameraPos = currentDir * currentDistance;
            
            vec3 currentUp = normalize(mix(ctx.startUp, ctx.targetUp, t));
            cameraRot = quatLookAt(normalize(cameraPos), currentUp);

            wasModified = true;

            if (t >= 1.0f) {
                cameraPos = ctx.targetPos;
                cameraRot = quatLookAt(normalize(ctx.targetPos), ctx.targetUp);
                ctx.isAnimating = false;
            }
        }
        
        const float gizmoDiameter = baseSize * style.scale;
        const float scaledCircleRadius = style.circleRadius * style.scale;
        const float scaledBigCircleRadius = style.bigCircleRadius * style.scale;
        const float scaledLineWidth = style.lineWidth * style.scale;
        const float scaledHighlightWidth = style.highlightWidth * style.scale;
        const float scaledHighlightRadius = (style.circleRadius + 2.0f) * style.scale;
        const float scaledFontSize = ImGui::GetFontSize() * style.scale * style.labelSize;

        mat4 worldMatrix = translate(mat4(1.0f), cameraPos) * mat4_cast(cameraRot);
        mat4 viewMatrix = inverse(worldMatrix);

        mat4 gizmoViewMatrix = mat4(mat3(viewMatrix));
        mat4 gizmoProjectionMatrix = ortho(-1, 1, -1, 1, 1, -1);
        mat4 gizmoMvp = gizmoProjectionMatrix * gizmoViewMatrix;

        std::vector<GizmoAxis> axes;
        for (int i = 0; i < 3; ++i) {
            axes.push_back({i * 2, i, (gizmoViewMatrix * vec4(axisVectors[i], 0)).z, axisVectors[i]});
            axes.push_back({i * 2 + 1, i, (gizmoViewMatrix * vec4(-axisVectors[i], 0)).z, -axisVectors[i]});
        }

        std::sort(axes.begin(), axes.end(), [](const GizmoAxis& a, const GizmoAxis& b) {
            return a.depth < b.depth;
        });

        auto worldToScreen = [&](const vec3& worldPos) -> ImVec2 {
            const vec4 clipPos = gizmoMvp * vec4(worldPos, 1.0f);
            if (clipPos.w == 0.0f) return {-FLT_MAX, -FLT_MAX};
            const vec3 ndc = vec3(clipPos) / clipPos.w;
            return {
                position.x + ndc.x * (gizmoDiameter / 2.f),
                position.y - ndc.y * (gizmoDiameter / 2.f)
            };
        };

        if (ctx.activeTool == TOOL_NONE && !ctx.isAnimating) {
            const float halfGizmoSize = gizmoDiameter / 2.f;
            ImVec2 mousePos = io.MousePos;
            float distToCenterSq = ImLengthSqr(ImVec2(mousePos.x - position.x, mousePos.y - position.y));

            if (distToCenterSq < (halfGizmoSize + scaledCircleRadius) * (halfGizmoSize + scaledCircleRadius))
            {
                const float minDistanceSq = scaledCircleRadius * scaledCircleRadius;
                for (const auto& axis : axes) {
                    if (axis.depth < -0.1f)
                        continue;
                    
                    ImVec2 handlePos = worldToScreen(axis.direction * style.lineLength);
                    if (ImLengthSqr(ImVec2(handlePos.x - mousePos.x, handlePos.y - mousePos.y)) < minDistanceSq)
                        ctx.hoveredAxisID = axis.id;
                }
                if (ctx.hoveredAxisID == -1) {
                    ImVec2 centerPos = worldToScreen(origin);
                    if (ImLengthSqr(ImVec2(centerPos.x - mousePos.x, centerPos.y - mousePos.y)) < scaledBigCircleRadius * scaledBigCircleRadius)
                        ctx.hoveredAxisID = 6;
                }
            }
        }

        if (ctx.hoveredAxisID == 6 || ctx.activeTool == TOOL_GIZMO)
            drawList->AddCircleFilled(worldToScreen(origin), scaledBigCircleRadius, style.bigCircleColor);

        for (const auto& [id, axis_index, depth, direction] : axes) {
            // Color
            float factor = mix(style.fadeFactor, 1.0f, (depth + 1.0f) * 0.5f);
            ImVec4 baseColor = ImGui::ColorConvertU32ToFloat4(style.axisColors[axis_index]);
            ImVec4 fadedColor = ImVec4(baseColor.x, baseColor.y, baseColor.z, baseColor.w * factor);
            ImU32 final_color = ImGui::ColorConvertFloat4ToU32(fadedColor);

            // Screen Positions
            ImVec2 originPos = worldToScreen(origin);
            ImVec2 handlePos = worldToScreen(direction * style.lineLength);

            // Line stop at circle edge
            ImVec2 lineDir = ImVec2(handlePos.x - originPos.x, handlePos.y - originPos.y);
            float lineLength = sqrtf(lineDir.x * lineDir.x + lineDir.y * lineDir.y) + 1e-6f; // Avoid division by zero
            lineDir.x /= lineLength;
            lineDir.y /= lineLength;
            ImVec2 lineEndPos = ImVec2(handlePos.x - lineDir.x * scaledCircleRadius, handlePos.y - lineDir.y * scaledCircleRadius);
            
            // Drawing
            if (direction[axis_index] > 0) {
              drawList->AddLine(originPos, lineEndPos, final_color, scaledLineWidth); // Use the new endpoint
            }
            drawList->AddCircleFilled(handlePos, scaledCircleRadius, final_color); // Circle remains at the original position

            // Highlight
            if (ctx.hoveredAxisID == id)
                drawList->AddCircle(handlePos, scaledHighlightRadius, style.highlightColor, 0, scaledHighlightWidth);
        }

        ImFont* font = ImGui::GetFont();
        for (const auto& axis : axes) {
            if (axis.depth < -0.1f)
                continue;
            ImVec2 textPos = worldToScreen(axis.direction * style.lineLength);
            const char* label = style.axisLabels[axis.axisIndex];
            ImVec2 textSize = font->CalcTextSizeA(scaledFontSize, FLT_MAX, 0.f, label);
            drawList->AddText(font, scaledFontSize,{textPos.x - textSize.x * 0.5f, textPos.y - textSize.y * 0.5f}, style.labelColor, label);
        }

        // Drag logic
        if (ImGui::IsMouseDown(0)) {
            if (ctx.activeTool == TOOL_NONE && ctx.hoveredAxisID == 6) {
                ctx.activeTool = TOOL_GIZMO;
                ctx.isAnimating = false;
            }
        }

        if(ctx.activeTool == TOOL_GIZMO) {
            float yawAngle = -io.MouseDelta.x * rotationSpeed;
            float pitchAngle = -io.MouseDelta.y * rotationSpeed;
            quat yawRotation = angleAxis(yawAngle, worldUp);
            vec3 rightAxis = cameraRot * worldRight;
            quat pitchRotation = angleAxis(pitchAngle, rightAxis);
            quat totalRotation = yawRotation * pitchRotation;
            cameraPos = totalRotation * cameraPos;
            cameraRot = totalRotation * cameraRot;
            wasModified = true;
        }

        // Snap
        if (ImGui::IsMouseReleased(0) && ctx.hoveredAxisID >= 0 && ctx.hoveredAxisID <= 5 && !ImGui::IsMouseDragging(0)) {
            int axisIndex = ctx.hoveredAxisID / 2;
            float sign = (ctx.hoveredAxisID % 2 == 0) ? -1.0f : 1.0f;
            vec3 targetDir = sign * axisVectors[axisIndex];
            vec3 targetPosition = targetDir * snapDistance;

            vec3 up = worldUp; 
            if (axisIndex == 1)
                up = worldForward;
            vec3 targetUp = up;
            
            quat targetRotation = quatLookAt(targetDir, targetUp);

            if (style.animateSnap && style.snapAnimationDuration > 0.0f) {
                bool pos_is_different = length2(cameraPos - targetPosition) > 0.0001f;
                bool rot_is_different = (1.0f - abs(dot(cameraRot, targetRotation))) > 0.0001f;

                if (pos_is_different || rot_is_different) {
                    ctx.isAnimating = true;
                    ctx.animationStartTime = static_cast<float>(ImGui::GetTime());
                    ctx.startPos = cameraPos;
                    ctx.targetPos = targetPosition;
                    ctx.startUp = cameraRot * vec3(0.0f, 1.0f, 0.0f);
                    ctx.targetUp = targetUp;
                }
            } else {
                cameraRot = targetRotation;
                cameraPos = targetPosition;
                wasModified = true;
            }
        }

        return wasModified;
    }

    bool Zoom(vec3& cameraPos, const quat& cameraRot, ImVec2 position, float zoomSpeed) {
        BeginFrame();
        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        Context& ctx = GetContext();
        Style& style = GetStyle();
        bool wasModified = false;

        const float radius = style.toolButtonRadius * style.scale;
        const ImVec2 center = { position.x + radius, position.y + radius };
        
        bool isHovered = false;
        if(ctx.activeTool == TOOL_NONE || ctx.activeTool == TOOL_ZOOM)
            if (ImLengthSqr(ImVec2(io.MousePos.x - center.x, io.MousePos.y - center.y)) < radius * radius)
                isHovered = true;
        
        ctx.isZoomButtonHovered = isHovered;

        // Start Drag
        if (isHovered && ImGui::IsMouseDown(0) && ctx.activeTool == TOOL_NONE) {
            ctx.activeTool = TOOL_ZOOM;
            ctx.isAnimating = false;
        }

        // Perform Zoom
        if (ctx.activeTool == TOOL_ZOOM) {
            if (io.MouseDelta.y != 0.0f) {
                // Use the camera's local forward vector for zooming
                vec3 cameraForward = cameraRot * worldForward;
                cameraPos += cameraForward * -io.MouseDelta.y * zoomSpeed;
                wasModified = true;
            }
        }
        
        // Draw
        ImU32 bgColor = style.toolButtonColor;
        if (ctx.activeTool == TOOL_ZOOM || isHovered)
            bgColor = style.toolButtonHoveredColor;
        drawList->AddCircleFilled(center, radius, bgColor);

        const float p = style.toolButtonInnerPadding * style.scale;
        const float th = 2.0f * style.scale;
        const ImU32 iconColor = style.toolButtonIconColor;

        constexpr float iconScale = 0.5f; 
        // Magnifying glass circle
        ImVec2 glassCenter = { center.x - (p / 2.0f) * iconScale, center.y - (p / 2.0f) * iconScale };
        float glassRadius = (radius - p - 1.f) * iconScale;
        drawList->AddCircle(glassCenter, glassRadius, iconColor, 0, th);

        // Handle
        ImVec2 handleStart = { center.x + (radius / 2.0f) * iconScale, center.y + (radius / 2.0f) * iconScale };
        ImVec2 handleEnd = { center.x + (radius - p) * iconScale, center.y + (radius - p) * iconScale };
        drawList->AddLine(handleStart, handleEnd, iconColor, th);

        // Plus sign (vertical)
        ImVec2 plusVertStart = { center.x - (p / 2.0f) * iconScale, center.y - (radius / 2.0f) * iconScale };
        ImVec2 plusVertEnd = { center.x - (p / 2.0f) * iconScale, center.y + (radius / 2.0f - p) * iconScale };
        drawList->AddLine(plusVertStart, plusVertEnd, iconColor, th);

        // Plus sign (horizontal)
        ImVec2 plusHorizStart = { center.x + (-radius / 2.0f + p / 2.0f) * iconScale, center.y - (p / 2.0f) * iconScale };
        ImVec2 plusHorizEnd = { center.x + (radius / 2.0f - p * 1.5f) * iconScale, center.y - (p / 2.0f) * iconScale };
        drawList->AddLine(plusHorizStart, plusHorizEnd, iconColor, th);
        
        return wasModified;
    }
    
    bool Pan(vec3& cameraPos, const quat& cameraRot, ImVec2 position, float panSpeed) {
        BeginFrame();
        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        Context& ctx = GetContext();
        Style& style = GetStyle();
        bool wasModified = false;

        const float radius = style.toolButtonRadius * style.scale;
        const ImVec2 center = { position.x + radius, position.y + radius };

        // Interaction Logic 
        bool isHovered = false;
        if (ctx.activeTool == TOOL_NONE || ctx.activeTool == TOOL_PAN) {
            if (ImLengthSqr(ImVec2(io.MousePos.x - center.x, io.MousePos.y - center.y)) < radius * radius)
                isHovered = true;
        }
        ctx.isPanButtonHovered = isHovered;
        
        // Start Drag
        if (isHovered && ImGui::IsMouseDown(0) && ctx.activeTool == TOOL_NONE) {
            ctx.activeTool = TOOL_PAN;
            ctx.isAnimating = false;
        }

        // Perform Pan
        if (ctx.activeTool == TOOL_PAN) {
            if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) {
                // Pan along the camera's local axes with natural "drag" controls
                cameraPos += cameraRot * worldRight * -io.MouseDelta.x * panSpeed; // Inverted horizontal
                cameraPos += cameraRot * worldUp * io.MouseDelta.y * panSpeed;    // Natural vertical
                wasModified = true;
            }
        }

        // Drawing 

        // Draw the background circle
        ImU32 bgColor = style.toolButtonColor;
        if (ctx.activeTool == TOOL_PAN ||isHovered)
            bgColor = style.toolButtonHoveredColor;
        drawList->AddCircleFilled(center, radius, bgColor);

        // Draw the icon on top of the background
        const ImU32 iconColor = style.toolButtonIconColor;
        const float th = 2.0f * style.scale; // Use scaled thickness for consistency

        // Four-way arrow symbol
        const float size = radius * 0.5f;
        const float arm = size * 0.25f; // 1/2 gap
        // Top Arrow (^) 
        const ImVec2 topTip = { center.x, center.y - size };
        drawList->AddLine({ topTip.x - arm, topTip.y + arm }, topTip, iconColor, th);
        drawList->AddLine({ topTip.x + arm, topTip.y + arm }, topTip, iconColor, th);
        // Bottom Arrow (v) 
        const ImVec2 botTip = { center.x, center.y + size };
        drawList->AddLine({ botTip.x - arm, botTip.y - arm }, botTip, iconColor, th);
        drawList->AddLine({ botTip.x + arm, botTip.y - arm }, botTip, iconColor, th);
        // Left Arrow (<) 
        const ImVec2 leftTip = { center.x - size, center.y };
        drawList->AddLine({ leftTip.x + arm, leftTip.y - arm }, leftTip, iconColor, th);
        drawList->AddLine({ leftTip.x + arm, leftTip.y + arm }, leftTip, iconColor, th);
        // Right Arrow (>) 
        const ImVec2 rightTip = { center.x + size, center.y };
        drawList->AddLine({ rightTip.x - arm, rightTip.y - arm }, rightTip, iconColor, th);
        drawList->AddLine({ rightTip.x - arm, rightTip.y + arm }, rightTip, iconColor, th);
        
        return wasModified;
    }


} // namespace ImViewGuizmo
#endif // IMVIEWGUIZMO_IMPLEMENTATION