#pragma once

#include <imgui.h>
#include <imgui_internal.h>

inline static void CopyIOEvents(ImGuiContext* src, ImGuiContext* dst, ImVec2 origin, float scale, bool allowMousePress)
{
    dst->PlatformImeData = src->PlatformImeData;
    dst->IO.DeltaTime = src->IO.DeltaTime;
    dst->InputEventsQueue = src->InputEventsTrail;
    for (int i = dst->InputEventsQueue.Size - 1; i >= 0; --i) {
        ImGuiInputEvent& e = dst->InputEventsQueue[i];
        if (e.Type == ImGuiInputEventType_MousePos) {
            e.MousePos.PosX = (e.MousePos.PosX - origin.x) / scale;
            e.MousePos.PosY = (e.MousePos.PosY - origin.y) / scale;
        } else if (!allowMousePress && e.Type == ImGuiInputEventType_MouseButton && e.MouseButton.Down) {
            // Don't let the canvas start an interaction when it isn't the hovered window.
            dst->InputEventsQueue.erase(&dst->InputEventsQueue[i]);
        }
    }
}

inline static void AppendDrawData(ImDrawList* src, ImVec2 origin, float scale)
{
    // TODO optimize if vtx_start == 0 || if idx_start == 0
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const int vtx_start = dl->VtxBuffer.size();
    const int idx_start = dl->IdxBuffer.size();
    dl->VtxBuffer.resize(dl->VtxBuffer.size() + src->VtxBuffer.size());
    dl->IdxBuffer.resize(dl->IdxBuffer.size() + src->IdxBuffer.size());
    dl->CmdBuffer.reserve(dl->CmdBuffer.size() + src->CmdBuffer.size());
    dl->_VtxWritePtr = dl->VtxBuffer.Data + vtx_start;
    dl->_IdxWritePtr = dl->IdxBuffer.Data + idx_start;
    const ImDrawVert* vtx_read = src->VtxBuffer.Data;
    const ImDrawIdx* idx_read = src->IdxBuffer.Data;
    for (int i = 0, c = src->VtxBuffer.size(); i < c; ++i) {
        dl->_VtxWritePtr[i].uv = vtx_read[i].uv;
        dl->_VtxWritePtr[i].col = vtx_read[i].col;
        dl->_VtxWritePtr[i].pos = vtx_read[i].pos * scale + origin;
    }
    for (int i = 0, c = src->IdxBuffer.size(); i < c; ++i) {
        dl->_IdxWritePtr[i] = idx_read[i] + vtx_start;
    }
    for (auto cmd : src->CmdBuffer) {
        cmd.IdxOffset += idx_start;
        IM_ASSERT(cmd.VtxOffset == 0);
        cmd.ClipRect.x = cmd.ClipRect.x * scale + origin.x;
        cmd.ClipRect.y = cmd.ClipRect.y * scale + origin.y;
        cmd.ClipRect.z = cmd.ClipRect.z * scale + origin.x;
        cmd.ClipRect.w = cmd.ClipRect.w * scale + origin.y;
        dl->CmdBuffer.push_back(cmd);
    }

    dl->_VtxCurrentIdx += src->VtxBuffer.size();
    dl->_VtxWritePtr = dl->VtxBuffer.Data + dl->VtxBuffer.size();
    dl->_IdxWritePtr = dl->IdxBuffer.Data + dl->IdxBuffer.size();
}

struct ContainedContextConfig
{
    bool extra_window_wrapper = false;
    ImVec2 size = {0.f, 0.f};
    ImU32 color = IM_COL32_WHITE;
    bool zoom_enabled = true;
    float zoom_min = 0.3f;
    float zoom_max = 2.f;
    float zoom_divisions = 10.f;
    float zoom_smoothness = 5.f;
    float default_zoom = 1.f;
    ImGuiKey reset_zoom_key = ImGuiKey_R;
    ImGuiMouseButton scroll_button = ImGuiMouseButton_Middle;
};

class ContainedContext
{
public:
    ~ContainedContext();
    ContainedContextConfig& config() { return m_config; }
    void begin();
    void end();
    [[nodiscard]] ImVec2 size() const { return m_size; }
    [[nodiscard]] float scale() const { return m_scale; }
    [[nodiscard]] const ImVec2& origin() const { return m_origin; }
    [[nodiscard]] bool hovered() const { return m_hovered; }
    [[nodiscard]] const ImVec2& scroll() const { return m_scroll; }
    void setScroll(const ImVec2& s) { m_scroll = s; }
    void setScale(float s) { m_scale = s; m_scaleTarget = s; }
    ImGuiContext* getRawContext() { return m_ctx; }
    void setFontDensity();
private:
    ContainedContextConfig m_config;

    ImVec2 m_origin;
    ImVec2 m_pos;
    ImVec2 m_size;
    ImGuiContext* m_ctx = nullptr;
    ImGuiContext* m_original_ctx = nullptr;

    bool m_anyWindowHovered = false;
    bool m_anyItemActive = false;
    bool m_hovered = false;

    float m_scale = m_config.default_zoom, m_scaleTarget = m_config.default_zoom;
    ImVec2 m_scroll = {0.f, 0.f};
};

inline ContainedContext::~ContainedContext()
{
    if (m_ctx) ImGui::DestroyContext(m_ctx);
}

// Call after Begin()
inline void ContainedContext::setFontDensity()
{
#if IMGUI_VERSION_NUM >= 19198
    ImGui::SetFontRasterizerDensity(roundf(m_scale * 100.0f) / 100.0f); // Round density to two digits.
#endif
}

inline void ContainedContext::begin()
{
    ImGui::PushID(this);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_config.color);
    ImGui::BeginChild("view_port", m_config.size, 0, ImGuiWindowFlags_NoMove);
    setFontDensity();
    ImGui::PopStyleColor();
    m_pos = ImGui::GetWindowPos();
    bool outerHovered = ImGui::IsWindowHovered();

    m_size = ImGui::GetContentRegionAvail();
    m_origin = ImGui::GetCursorScreenPos();
    m_original_ctx = ImGui::GetCurrentContext();
    const ImGuiStyle& orig_style = ImGui::GetStyle();
    if (!m_ctx) m_ctx = ImGui::CreateContext(ImGui::GetIO().Fonts);
    ImGui::SetCurrentContext(m_ctx);
    ImGuiStyle& new_style = ImGui::GetStyle();
    new_style = orig_style;

    CopyIOEvents(m_original_ctx, m_ctx, m_origin, m_scale, outerHovered);

    ImGui::GetIO().DisplaySize = m_size / m_scale;
    ImGui::GetIO().ConfigInputTrickleEventQueue = false;

    // Copy the ImGuiBackendFlags_RendererHasTextures flag as they need to be matching.
    // This will also copy the ImGuiBackendFlags_RendererHasVtxOffset flag which will be more optimal in case large draw calls are being made.
    ImGui::GetIO().ConfigFlags = m_original_ctx->IO.ConfigFlags;
    ImGui::GetIO().BackendFlags = m_original_ctx->IO.BackendFlags;
#ifdef IMGUI_HAS_VIEWPORT
    ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_ViewportsEnable | ImGuiConfigFlags_DockingEnable);
#endif

    ImGui::NewFrame();

    if (!m_config.extra_window_wrapper)
        return;
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("viewport_container", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove
                                                | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    setFontDensity();
    ImGui::PopStyleVar();
}

inline void ContainedContext::end()
{
    m_anyWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
    if (m_config.extra_window_wrapper && ImGui::IsWindowHovered())
        m_anyWindowHovered = false;

    m_anyItemActive = ImGui::IsAnyItemActive();

    if (m_config.extra_window_wrapper)
        ImGui::End();

    ImGui::Render();

    ImDrawData* draw_data = ImGui::GetDrawData();

    // Only propagate the IME request when the inner canvas wants it, so outer-context
    // text inputs (e.g. a side panel) aren't clobbered to "off".
    if (m_ctx->PlatformImeData.WantVisible)
        m_original_ctx->PlatformImeData = m_ctx->PlatformImeData;
    ImGui::SetCurrentContext(m_original_ctx);
    m_original_ctx = nullptr;

    for (int i = 0; i < draw_data->CmdListsCount; ++i)
        AppendDrawData(draw_data->CmdLists[i], m_origin, m_scale);

    m_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && !m_anyWindowHovered;

    // Zooming
    if (m_config.zoom_enabled && m_hovered && ImGui::GetIO().MouseWheel != 0.f)
    {
        m_scaleTarget += ImGui::GetIO().MouseWheel / m_config.zoom_divisions;
        m_scaleTarget = m_scaleTarget < m_config.zoom_min ? m_config.zoom_min : m_scaleTarget;
        m_scaleTarget = m_scaleTarget > m_config.zoom_max ? m_config.zoom_max : m_scaleTarget;

        if (m_config.zoom_smoothness == 0.f)
        {
            m_scroll += (ImGui::GetMousePos() - m_pos) / m_scaleTarget - (ImGui::GetMousePos() - m_pos) / m_scale;
            m_scale = m_scaleTarget;
        }
    }
    if (abs(m_scaleTarget - m_scale) >= 0.015f / m_config.zoom_smoothness)
    {
        float cs = (m_scaleTarget - m_scale) / m_config.zoom_smoothness;
        m_scroll += (ImGui::GetMousePos() - m_pos) / (m_scale + cs) - (ImGui::GetMousePos() - m_pos) / m_scale;
        m_scale += (m_scaleTarget - m_scale) / m_config.zoom_smoothness;

        if (abs(m_scaleTarget - m_scale) < 0.015f / m_config.zoom_smoothness)
        {
            m_scroll += (ImGui::GetMousePos() - m_pos) / m_scaleTarget - (ImGui::GetMousePos() - m_pos) / m_scale;
            m_scale = m_scaleTarget;
        }
    }

    // Zoom reset
    if (ImGui::IsKeyPressed(m_config.reset_zoom_key, false))
        m_scaleTarget = m_config.default_zoom;

    // Scrolling
    if (m_hovered && !m_anyItemActive && ImGui::IsMouseDragging(m_config.scroll_button, 0.f))
    {
        m_scroll += ImGui::GetIO().MouseDelta / m_scale;
    }

    ImGui::EndChild();
    ImGui::PopID();
}
