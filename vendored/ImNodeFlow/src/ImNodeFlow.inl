#pragma once

#include "ImNodeFlow.h"
#include "imgui_bezier_math.h"

namespace ImFlow
{
    inline void smart_bezier(const ImVec2& p1, const ImVec2& p2, ImU32 color, float thickness)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float distance = sqrtf((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));
        float delta = distance * 0.45f;
        float minRight = 80.0f; // Minimum rightward offset for leftward or downward connections
        float vert = 0.f;
        ImVec2 p11, p22;
        float horizontalDist = p1.x - p2.x;
        float verticalDist = fabsf(p2.y - p1.y);
        if (horizontalDist > verticalDist && verticalDist < 60.0f) {
            // Nodes are side by side: use angular path (right, up/down, right, down)
            float offset = 30.0f; // how far to go right from p1
            float up = (p2.y <= p1.y) ? -50.0f : 50.0f; // go up or down depending on target
            ImVec2 pA = p1 + ImVec2(offset, 0);
            ImVec2 pB = ImVec2(pA.x, p1.y + up);
            ImVec2 pC = ImVec2(p2.x - offset, p1.y + up);
            ImVec2 pD = ImVec2(p2.x - offset, p2.y);
          
            auto pBMid = pB + ImVec2{-offset, 0};
            auto pCMid = pC + ImVec2{offset, 0};

            dl->AddLine(pBMid, pCMid, color, thickness);
            pBMid.y += 0.5f;
            dl->AddBezierCubic(p1, pA, pB, pBMid, color, thickness);
            pCMid.x += 1;
            pCMid.y += 0.5f;
            dl->AddBezierCubic(pCMid, pC, pD, p2, color, thickness);
          
        } else if (p2.x >= p1.x) {
            // Standard rightward connection
            p11 = p1 + ImVec2(delta, vert);
            p22 = p2 - ImVec2(delta, vert);
            dl->AddBezierCubic(p1, p11, p22, p2, color, thickness);
        } else {
            // Leftward or downward connection: go right first, then arc
            float arcHeight = 0.35f * distance + 30.0f;
            float rightward = fmaxf(minRight, delta * 0.4f);
            if (verticalDist < 40.0f) {
                vert = (p2.y >= p1.y) ? arcHeight : -arcHeight;
            } else {
                vert = (p2.y > p1.y) ? arcHeight : -arcHeight;
            }
            p11 = p1 + ImVec2(rightward, vert);
            p22 = p2 - ImVec2(rightward, vert);
            dl->AddBezierCubic(p1, p11, p22, p2, color, thickness);
        }
    }

    // Sample the automatic smart route into a polyline (used when a link has no waypoints).
    inline void build_smart_polyline(const ImVec2& p1, const ImVec2& p2, std::vector<ImVec2>& pts)
    {
        pts.clear();

        auto cubic = [](const ImVec2& a, const ImVec2& b, const ImVec2& c, const ImVec2& d, float t) {
            float u = 1.f - t, w0 = u*u*u, w1 = 3*u*u*t, w2 = 3*u*t*t, w3 = t*t*t;
            return ImVec2(w0*a.x + w1*b.x + w2*c.x + w3*d.x, w0*a.y + w1*b.y + w2*c.y + w3*d.y);
        };
        const int SEG = 20;
        auto addCubic = [&](const ImVec2& a, const ImVec2& b, const ImVec2& c, const ImVec2& d) {
            for(int i = pts.empty() ? 0 : 1; i <= SEG; ++i) pts.push_back(cubic(a, b, c, d, (float)i / SEG));
        };

        float distance = sqrtf((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));
        float delta = distance * 0.45f;
        float minRight = 80.0f;
        float vert = 0.f;
        float horizontalDist = p1.x - p2.x;
        float verticalDist = fabsf(p2.y - p1.y);
        if (horizontalDist > verticalDist && verticalDist < 60.0f) {
            float offset = 30.0f;
            float up = (p2.y <= p1.y) ? -50.0f : 50.0f;
            ImVec2 pA = p1 + ImVec2(offset, 0);
            ImVec2 pB = ImVec2(pA.x, p1.y + up);
            ImVec2 pC = ImVec2(p2.x - offset, p1.y + up);
            ImVec2 pD = ImVec2(p2.x - offset, p2.y);
            ImVec2 pBMid = pB + ImVec2(-offset, 0); pBMid.y += 0.5f;
            ImVec2 pCMid = pC + ImVec2(offset, 0);  pCMid.x += 1; pCMid.y += 0.5f;
            addCubic(p1, pA, pB, pBMid);
            pts.push_back(pCMid);
            addCubic(pCMid, pC, pD, p2);
        } else if (p2.x >= p1.x) {
            addCubic(p1, p1 + ImVec2(delta, vert), p2 - ImVec2(delta, vert), p2);
        } else {
            float arcHeight = 0.35f * distance + 30.0f;
            float rightward = fmaxf(minRight, delta * 0.4f);
            vert = (verticalDist < 40.0f) ? ((p2.y >= p1.y) ? arcHeight : -arcHeight)
                                          : ((p2.y >  p1.y) ? arcHeight : -arcHeight);
            addCubic(p1, p1 + ImVec2(rightward, vert), p2 - ImVec2(rightward, vert), p2);
        }
    }

    // Facing-aware cubic, used when an endpoint is mirrored so the wire leaves/arrives on the
    // side the pin actually points to (otherwise the smart route bulges the wrong way).
    inline void build_facing_polyline(const ImVec2& p1, const ImVec2& p2, float outDir, float inDir, std::vector<ImVec2>& pts)
    {
        pts.clear();
        float distance = sqrtf((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));
        float d = fmaxf(45.0f, distance * 0.5f);
        ImVec2 c1 = p1 + ImVec2(outDir * d, 0.f);
        ImVec2 c2 = p2 + ImVec2(inDir * d, 0.f);

        auto cubic = [](const ImVec2& a, const ImVec2& b, const ImVec2& c, const ImVec2& dd, float t) {
            float u = 1.f - t, w0 = u*u*u, w1 = 3*u*u*t, w2 = 3*u*t*t, w3 = t*t*t;
            return ImVec2(w0*a.x + w1*b.x + w2*c.x + w3*dd.x, w0*a.y + w1*b.y + w2*c.y + w3*dd.y);
        };
        const int SEG = 24;
        for (int i = 0; i <= SEG; ++i) pts.push_back(cubic(p1, c1, c2, p2, (float)i / SEG));
    }

    inline void build_link_polyline(const ImVec2& p1, const ImVec2& p2, const std::vector<ImVec2>& mids, std::vector<ImVec2>& pts, float outDir, float inDir)
    {
        if (mids.empty()) {
            // Default facings (output right, input left) keep the original smart route.
            if (outDir > 0.f && inDir < 0.f) build_smart_polyline(p1, p2, pts);
            else                             build_facing_polyline(p1, p2, outDir, inDir, pts);
            return;
        }

        // Smooth Catmull-Rom spline through start, the waypoints, and end (the curve passes
        // through every control point, so dragging a waypoint pulls the wire to it).
        pts.clear();
        std::vector<ImVec2> cp;
        cp.reserve(mids.size() + 2);
        cp.push_back(p1);
        for (const auto& m : mids) cp.push_back(m);
        cp.push_back(p2);

        auto cr = [](const ImVec2& P0, const ImVec2& P1, const ImVec2& P2, const ImVec2& P3, float t) {
            float t2 = t * t, t3 = t2 * t;
            return ImVec2(
                0.5f * ((2*P1.x) + (-P0.x + P2.x)*t + (2*P0.x - 5*P1.x + 4*P2.x - P3.x)*t2 + (-P0.x + 3*P1.x - 3*P2.x + P3.x)*t3),
                0.5f * ((2*P1.y) + (-P0.y + P2.y)*t + (2*P0.y - 5*P1.y + 4*P2.y - P3.y)*t2 + (-P0.y + 3*P1.y - 3*P2.y + P3.y)*t3));
        };

        const int SEG = 16;
        int n = (int)cp.size();
        pts.push_back(cp[0]);
        for (int i = 0; i + 1 < n; ++i) {
            const ImVec2& P0 = cp[i > 0 ? i - 1 : 0];
            const ImVec2& P1 = cp[i];
            const ImVec2& P2 = cp[i + 1];
            const ImVec2& P3 = cp[i + 2 < n ? i + 2 : n - 1];
            for (int s = 1; s <= SEG; ++s) pts.push_back(cr(P0, P1, P2, P3, (float)s / SEG));
        }
    }

    inline void draw_link_solid(const std::vector<ImVec2>& pts, ImU32 color, float thickness)
    {
        if (pts.size() < 2) return;
        ImGui::GetWindowDrawList()->AddPolyline(pts.data(), (int)pts.size(), color, 0, thickness);
    }

    inline void draw_link_gradient(const std::vector<ImVec2>& pts, ImU32 c1, ImU32 c2, float thickness)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        auto lerpCol = [](ImU32 a, ImU32 b, float t) {
            ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a), cb = ImGui::ColorConvertU32ToFloat4(b);
            return ImGui::ColorConvertFloat4ToU32(ImVec4(
                ca.x + (cb.x - ca.x) * t, ca.y + (cb.y - ca.y) * t,
                ca.z + (cb.z - ca.z) * t, ca.w + (cb.w - ca.w) * t));
        };

        int n = (int)pts.size();
        for(int i = 0; i + 1 < n; ++i) {
            float t = (n <= 1) ? 0.f : (float)i / (float)(n - 1);
            dl->AddLine(pts[i], pts[i + 1], lerpCol(c1, c2, t), thickness);
        }
    }

    inline void draw_link_flow(const std::vector<ImVec2>& pts, ImU32 color, float thickness, float phase)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        int n = (int)pts.size();
        if(n < 2) return;

        // Cumulative arc length at each polyline vertex.
        std::vector<float> acc(n, 0.f);
        for(int i = 1; i < n; ++i) {
            float dx = pts[i].x - pts[i - 1].x, dy = pts[i].y - pts[i - 1].y;
            acc[i] = acc[i - 1] + sqrtf(dx * dx + dy * dy);
        }
        float total = acc[n - 1];
        if(total <= 0.f) return;

        // Dash pattern (pixels of arc length). "phase" slides the pattern so the dashes
        // appear to travel from p1 towards p2.
        const float dashLen = 10.0f;
        const float gapLen  = 8.0f;
        const float period  = dashLen + gapLen;

        // Position on the polyline at arc length s, in [0, total].
        auto pointAt = [&](float s) -> ImVec2 {
            if(s <= 0.f)     return pts[0];
            if(s >= total)   return pts[n - 1];
            int i = 1;
            while(i < n && acc[i] < s) ++i;
            float segLen = acc[i] - acc[i - 1];
            float t = segLen > 0.f ? (s - acc[i - 1]) / segLen : 0.f;
            return ImVec2(pts[i - 1].x + (pts[i].x - pts[i - 1].x) * t,
                          pts[i - 1].y + (pts[i].y - pts[i - 1].y) * t);
        };

        // Exact arc-length dash boundaries, so edges glide with the phase.
        float startS = -fmodf(phase, period);
        if(startS > 0.f) startS -= period;

        for(float ds = startS; ds < total; ds += period) {
            float a = fmaxf(ds, 0.f);
            float b = fminf(ds + dashLen, total);
            if(b <= a) continue;

            // Stroke that follows the curve between arc lengths a and b (include any
            // polyline vertices in between so curved dashes stay on the curve).
            ImVec2 prev = pointAt(a);
            for(int i = 1; i < n; ++i) {
                if(acc[i] <= a) continue;
                if(acc[i] >= b) break;
                dl->AddLine(prev, pts[i], color, thickness);
                prev = pts[i];
            }
            dl->AddLine(prev, pointAt(b), color, thickness);
        }
    }

    inline bool polyline_collider(const ImVec2& p, const std::vector<ImVec2>& pts, float radius)
    {
        // Nearest distance from "p" to any segment of the (already routed) polyline. This
        // matches whatever was actually drawn, so hovering is correct for every route shape.
        float r2 = radius * radius;
        for(int i = 0; i + 1 < (int)pts.size(); ++i) {
            ImVec2 a = pts[i], b = pts[i + 1];
            ImVec2 ab(b.x - a.x, b.y - a.y), ap(p.x - a.x, p.y - a.y);
            float len2 = ab.x * ab.x + ab.y * ab.y;
            float t = len2 > 0.f ? (ap.x * ab.x + ap.y * ab.y) / len2 : 0.f;
            t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
            float dx = p.x - (a.x + ab.x * t), dy = p.y - (a.y + ab.y * t);
            if(dx * dx + dy * dy <= r2) return true;
        }
        return false;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // HANDLER

    template<typename T, typename... Params>
    std::shared_ptr<T> ImNodeFlow::addNode(const ImVec2& pos, Params&&... args)
    {
        static_assert(std::is_base_of<BaseNode, T>::value, "Pushed type is not a subclass of BaseNode!");

        std::shared_ptr<T> n = std::make_shared<T>(std::forward<Params>(args)...);
        n->setPos(pos);
        n->setHandler(this);
        if (!n->getStyle())
            n->setStyle(NodeStyle::cyan());

        auto uid = reinterpret_cast<uintptr_t>(n.get());
        n->setUID(uid);
        m_nodes[uid] = n;
	
	if(onNodeCreateHook)
		onNodeCreateHook(n);

        return n;
    }

    template<typename T, typename... Params>
    std::shared_ptr<T> ImNodeFlow::placeNodeAt(const ImVec2& pos, Params&&... args)
    {
        return addNode<T>(screen2grid(pos), std::forward<Params>(args)...);
    }

    template<typename T, typename... Params>
    std::shared_ptr<T> ImNodeFlow::placeNode(Params&&... args)
    {
        return placeNodeAt<T>(ImGui::GetMousePos(), std::forward<Params>(args)...);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // BASE NODE

    template<typename T>
    std::shared_ptr<InPin<T>> BaseNode::addIN(const std::string& name, std::function<bool(Pin*, Pin*)> filter, std::shared_ptr<PinStyle> style)
    {
        return addIN_uid<T>(name, name, std::move(filter), std::move(style));
    }

    template<typename T, typename U>
    std::shared_ptr<InPin<T>> BaseNode::addIN_uid(const U& uid, const std::string& name, std::function<bool(Pin*, Pin*)> filter, std::shared_ptr<PinStyle> style)
    {
        PinUID h = std::hash<U>{}(uid);
        auto p = std::make_shared<InPin<T>>(h, name, std::move(filter), std::move(style), this, &m_inf);
        m_ins.push_back(p);
        return p;
    }

    template<typename U>
    void BaseNode::dropIN(const U& uid)
    {
        PinUID h = std::hash<U>{}(uid);
        for (auto it = m_ins.begin(); it != m_ins.end(); it++)
        {
            if (it->get()->getUid() == h)
            {
                m_ins.erase(it);
                return;
            }
        }
    }

    inline void BaseNode::dropIN(const char* uid)
    {
        dropIN<std::string>(uid);
    }

    template<typename T>
    const T& BaseNode::showIN(const std::string& name, std::function<bool(Pin*, Pin*)> filter, std::shared_ptr<PinStyle> style)
    {
        return showIN_uid<T>(name, name, std::move(filter), std::move(style));
    }

    template<typename T, typename U>
    const T& BaseNode::showIN_uid(const U& uid, const std::string& name, std::function<bool(Pin*, Pin*)> filter, std::shared_ptr<PinStyle> style)
    {
        PinUID h = std::hash<U>{}(uid);
        for (std::pair<int, std::shared_ptr<Pin>>& p : m_dynamicIns)
        {
            if (p.second->getUid() == h)
            {
                p.first = 1;
                return static_cast<InPin<T>*>(p.second.get())->val();
            }
        }

        m_dynamicIns.emplace_back(std::make_pair(1, std::make_shared<InPin<T>>(h, name, std::move(filter), std::move(style), this, &m_inf)));
        return static_cast<InPin<T>*>(m_dynamicIns.back().second.get())->val();
    }

    template<typename T>
    std::shared_ptr<OutPin<T>> BaseNode::addOUT(const std::string& name, std::shared_ptr<PinStyle> style)
    {
        return addOUT_uid<T>(name, name, std::move(style));
    }

    template<typename T, typename U>
    std::shared_ptr<OutPin<T>> BaseNode::addOUT_uid(const U& uid, const std::string& name, std::shared_ptr<PinStyle> style)
    {
        PinUID h = std::hash<U>{}(uid);
        auto p = std::make_shared<OutPin<T>>(h, name, std::move(style), this, &m_inf);
        m_outs.emplace_back(p);
        return p;
    }

    template<typename U>
    void BaseNode::dropOUT(const U& uid)
    {
        PinUID h = std::hash<U>{}(uid);
        for (auto it = m_outs.begin(); it != m_outs.end(); it++)
        {
            if (it->get()->getUid() == h)
            {
                m_outs.erase(it);
                return;
            }
        }
    }

    inline void BaseNode::dropOUT(const char* uid)
    {
        dropOUT<std::string>(uid);
    }

    template<typename T>
    void BaseNode::showOUT(const std::string& name, std::shared_ptr<PinStyle> style)
    {
        showOUT_uid<T>(name, name, std::move(style));
    }

    template<typename T, typename U>
    void BaseNode::showOUT_uid(const U& uid, const std::string& name, std::shared_ptr<PinStyle> style)
    {
        PinUID h = std::hash<U>{}(uid);
        for (std::pair<int, std::shared_ptr<Pin>>& p : m_dynamicOuts)
        {
            if (p.second->getUid() == h)
            {
                p.first = 2;
                return;
            }
        }

        m_dynamicOuts.emplace_back(std::make_pair(2, std::make_shared<OutPin<T>>(h, name, std::move(style), this, &m_inf)));
    }

    template<typename U>
    Pin* BaseNode::inPin(const U& uid)
    {
        PinUID h = std::hash<U>{}(uid);
        auto it = std::ranges::find_if(m_ins, [&h](std::shared_ptr<Pin>& p)
                            { return p->getUid() == h; });
        assert(it != m_ins.end() && "Pin UID not found!");
        return it->get();
    }

    inline Pin* BaseNode::inPin(const char* uid)
    {
        return inPin<std::string>(uid);
    }

    template<typename U>
    Pin* BaseNode::outPin(const U& uid)
    {
        PinUID h = std::hash<U>{}(uid);
        auto it = std::ranges::find_if(m_outs, [&h](std::shared_ptr<Pin>& p)
                            { return p->getUid() == h; });
        assert(it != m_outs.end() && "Pin UID not found!");
        return it->get();
    }

    inline Pin* BaseNode::outPin(const char* uid)
    {
        return outPin<std::string>(uid);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // PIN

    // Triangle socket (3-gon), drawn so it can point left or right. Used for the logic pins
    // of a mirrored node, where the default right-pointing n-gon would face the wrong way.
    inline void draw_socket_triangle(ImDrawList* dl, const ImVec2& c, float r, ImU32 col, float thickness, bool pointLeft)
    {
        float dir = pointLeft ? -1.0f : 1.0f;
        ImVec2 v0(c.x + dir * r,         c.y);
        ImVec2 v1(c.x - dir * 0.5f * r,  c.y + 0.8660254f * r);
        ImVec2 v2(c.x - dir * 0.5f * r,  c.y - 0.8660254f * r);
        if (thickness <= 0.0f) dl->AddTriangleFilled(v0, v1, v2, col);
        else                   dl->AddTriangle(v0, v1, v2, col, thickness);
    }

    inline void Pin::drawSocket()
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 tl = pinPoint() - ImVec2(m_style->socket_radius, m_style->socket_radius);
        ImVec2 br = pinPoint() + ImVec2(m_style->socket_radius, m_style->socket_radius);

        // Mirrored node + triangle (logic) pin: draw the arrow pointing the other way.
        bool flipTri = (m_style->socket_shape == 3) && m_parent && m_parent->isMirrored();

        if (isConnected())
        {
            if (flipTri) draw_socket_triangle(draw_list, pinPoint(), m_style->socket_connected_radius, m_style->color, -1.0f, true);
            else         draw_list->AddCircleFilled(pinPoint(), m_style->socket_connected_radius, m_style->color, m_style->socket_shape);
        }
        else
        {
            float r = (ImGui::IsItemHovered() || ImGui::IsMouseHoveringRect(tl, br))
                      ? m_style->socket_hovered_radius : m_style->socket_radius;
            if (flipTri) draw_socket_triangle(draw_list, pinPoint(), r, m_style->color, m_style->socket_thickness, true);
            else         draw_list->AddCircle(pinPoint(), r, m_style->color, m_style->socket_shape, m_style->socket_thickness);
        }

        if (ImGui::IsMouseHoveringRect(tl, br))
            (*m_inf)->hovering(this);
    }

    inline void Pin::drawDecoration()
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        if (ImGui::IsItemHovered())
            draw_list->AddRectFilled(m_pos - m_style->extra.padding, m_pos + m_size + m_style->extra.padding, m_style->extra.bg_hover_color, m_style->extra.bg_radius);
        else
            draw_list->AddRectFilled(m_pos - m_style->extra.padding, m_pos + m_size + m_style->extra.padding, m_style->extra.bg_color, m_style->extra.bg_radius);
        draw_list->AddRect(m_pos - m_style->extra.padding, m_pos + m_size + m_style->extra.padding, m_style->extra.border_color, m_style->extra.bg_radius, 0, m_style->extra.border_thickness);
    }

    inline void Pin::update()
    {
        // Custom rendering
        if (m_renderer)
        {
            ImGui::BeginGroup();
            m_renderer(this);
            ImGui::EndGroup();
            m_size = ImGui::GetItemRectSize();
            if (ImGui::IsItemHovered())
                (*m_inf)->hovering(this);
            return;
        }

        ImGui::SetCursorPos(m_pos);
        ImGui::Text("%s", m_name.c_str());
        m_size = ImGui::GetItemRectSize();

        drawDecoration();
        drawSocket();

        if (ImGui::IsItemHovered())
            (*m_inf)->hovering(this);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // IN PIN

    template<class T>
    void InPin<T>::createLink(Pin *other, bool force)
    {
        if (other == this || other->getType() == PinType_Input)
            return;

        if (m_parent == other->getParent() && !m_allowSelfConnection)
            return;

	for(auto& m_link : m_links){
		if (m_link && m_link->left() == other){
        	    return;
        	}
	}

        if (!force && !m_filter(other, this)) // Check Filter
            return;

        // Enforce single-link capacity by replacing the existing connection: a value
        // input takes one source, a logic output drives one target. Skipped on a
        // forced (loaded) link so saved graphs are preserved as-is.
        if (!force)
        {
            if (m_singleLink) deleteLinks();
            if (other->isSingleLink()) other->deleteLinks();
        }

        auto m_link = std::make_shared<Link>(other, this, (*m_inf));
	m_links.push_back(m_link);
        other->addLink(m_link);
        (*m_inf)->addLink(m_link);

	if((*m_inf)->onLinkCreateHook)
		(*m_inf)->onLinkCreateHook(m_link);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // OUT PIN

    template<class T>
    void OutPin<T>::createLink(ImFlow::Pin *other, bool force)
    {
        if (other == this || other->getType() == PinType_Output)
            return;

        other->createLink(this, force);
    }

    template<class T>
    void OutPin<T>::addLink(std::shared_ptr<Link>& link)
    {
	m_links.push_back(link);
    }

    template<class T>
    void OutPin<T>::deleteLinks()
    {
	    // Detach our list first so the reentrant deleteLink calls triggered by each
	    // Link's destruction (~Link -> m_left->deleteLink) see an empty list and
	    // become no-ops, instead of mutating the container we are iterating.
	    std::vector<std::weak_ptr<Link>> snapshot;
	    snapshot.swap(m_links);
	    for(auto& wl : snapshot){
		    if(auto l = wl.lock()) l->right()->deleteLink(l.get());
	    }
    }

    template<class T>
    void OutPin<T>::deleteLink(const Link* link)
    {
	    for(unsigned int i=0;i<m_links.size();i++){
		    auto& m_link = m_links[i];
		    if(m_link.expired()){
			    m_links.erase(m_links.begin() + i);
			    i--;
		    }else if(m_link.lock().get() == link){
			    m_link.lock()->right()->deleteLink(m_link.lock().get());
			    m_links.erase(m_links.begin() + i);
			    i--;
		    }
	    }
    }
}
