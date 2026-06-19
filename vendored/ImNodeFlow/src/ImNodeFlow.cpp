#include "ImNodeFlow.h"
#include <algorithm>

namespace ImFlow {
    // -----------------------------------------------------------------------------------------------------------------
    // LINK

    void Link::update() {
        ImVec2 start = m_left->pinPoint();
        ImVec2 end = m_right->pinPoint();
        float thickness = m_left->getStyle()->extra.link_thickness;
        bool mouseClickState = m_inf->getSingleUseClick();

        if (!ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            m_selected = false;

        if (smart_bezier_collider(ImGui::GetMousePos(), start, end, 2.5)) {
            m_hovered = true;
            thickness = m_left->getStyle()->extra.link_hovered_thickness;
            if (mouseClickState) {
                m_inf->consumeSingleUseClick();
                m_selected = true;
            }
        } else { m_hovered = false; }

        if (m_selected)
            smart_bezier(start, end, m_left->getStyle()->extra.outline_color,
                         thickness + m_left->getStyle()->extra.link_selected_outline_thickness);

        if (m_left->getStyle()->extra.animated) {
            // Control-flow link: dashes flow from source to destination.
            const float speed = -16.0f; // pixels / second
            float phase = (float)ImGui::GetTime() * speed;
            smart_bezier_flow(start, end, m_left->getStyle()->color, thickness, phase);
        } else {
            // Fade source colour -> destination colour, so a converted link reads as a transition.
            smart_bezier_gradient(start, end, m_left->getStyle()->color, m_right->getStyle()->color, thickness);
        }

        if (m_selected && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            m_right->deleteLink(this);
    }

    Link::~Link() {
        m_left->deleteLink(this);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // BASE NODE

    bool BaseNode::isHovered() {
        ImVec2 paddingTL = {m_style->padding.x, m_style->padding.y};
        ImVec2 paddingBR = {m_style->padding.z, m_style->padding.w};
        return ImGui::IsMouseHoveringRect(m_inf->grid2screen(m_pos - paddingTL),
                                          m_inf->grid2screen(m_pos + m_size + paddingBR)) && !m_inf->getDisabled();
    }

    void BaseNode::update() {
        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        ImGui::PushID(this);
        bool mouseClickState = m_inf->getSingleUseClick();
        ImVec2 offset = m_inf->grid2screen({0.f, 0.f});
        ImVec2 paddingTL = {m_style->padding.x, m_style->padding.y};
        ImVec2 paddingBR = {m_style->padding.z, m_style->padding.w};

        draw_list->ChannelsSetCurrent(1); // Foreground
        ImGui::SetCursorScreenPos(offset + m_pos);

        ImGui::BeginGroup();

        // Header
        ImGui::BeginGroup();
        ImGui::TextColored(m_style->header_title_color, "%s", m_title.c_str());
        ImGui::Spacing();
        ImGui::EndGroup();
        float headerH = ImGui::GetItemRectSize().y;
        float titleW = ImGui::GetItemRectSize().x;

        // Inputs
        if (!m_ins.empty()) {
            ImGui::BeginGroup();
            for (auto &p: m_ins) {
                p->setPos(ImGui::GetCursorPos());
                p->update();
            }
            for (auto &p: m_dynamicIns) {
                if (p.first == 1) {
                    p.second->setPos(ImGui::GetCursorPos());
                    p.second->update();
                    p.first = 0;
                }
            }
            ImGui::EndGroup();
            ImGui::SameLine();
        }

        // Content
        ImGui::BeginGroup();
        draw();
        ImGui::Dummy(ImVec2(0.f, 0.f));
        ImGui::EndGroup();
        ImGui::SameLine();

        // Outputs
        float maxW = 0.0f;
        for (auto &p: m_outs) {
            float w = p->calcWidth();
            if (w > maxW)
                maxW = w;
        }
        for (auto &p: m_dynamicOuts) {
            float w = p.second->calcWidth();
            if (w > maxW)
                maxW = w;
        }
        // Align outputs to the full node width (incl. drawBottom), from last frame's
        // m_size, so a wide bottom prop pushes them to the real right edge.
        float nodeW = titleW > m_size.x ? titleW : m_size.x;
        ImGui::BeginGroup();
        for (auto &p: m_outs) {
            // FIXME: This looks horrible
            if ((m_pos + ImVec2(nodeW, 0) + m_inf->getGrid().scroll()).x <
                ImGui::GetCursorPos().x + ImGui::GetWindowPos().x + maxW)
                p->setPos(ImGui::GetCursorPos() + ImGui::GetWindowPos() + ImVec2(maxW - p->calcWidth(), 0.f));
            else
                p->setPos(ImVec2((m_pos + ImVec2(nodeW - p->calcWidth(), 0) + m_inf->getGrid().scroll()).x,
                                 ImGui::GetCursorPos().y + ImGui::GetWindowPos().y));
            p->update();
        }
        for (auto &p: m_dynamicOuts) {
            // FIXME: This looks horrible
            if ((m_pos + ImVec2(nodeW, 0) + m_inf->getGrid().scroll()).x <
                ImGui::GetCursorPos().x + ImGui::GetWindowPos().x + maxW)
                p.second->setPos(
                        ImGui::GetCursorPos() + ImGui::GetWindowPos() + ImVec2(maxW - p.second->calcWidth(), 0.f));
            else
                p.second->setPos(
                        ImVec2((m_pos + ImVec2(nodeW - p.second->calcWidth(), 0) + m_inf->getGrid().scroll()).x,
                               ImGui::GetCursorPos().y + ImGui::GetWindowPos().y));
            p.second->update();
            p.first -= 1;
        }

        ImGui::EndGroup();

        // Full-width content under the pin rows, flush with the node's left edge.
        drawBottom();

        ImGui::EndGroup();
        m_size = ImGui::GetItemRectSize();
        ImVec2 headerSize = ImVec2(m_size.x + paddingBR.x, headerH);

        // Background
        draw_list->ChannelsSetCurrent(0);
        draw_list->AddRectFilled(offset + m_pos - paddingTL, offset + m_pos + m_size + paddingBR, m_style->bg,
                                 m_style->radius);
        draw_list->AddRectFilled(offset + m_pos - paddingTL, offset + m_pos + headerSize, m_style->header_bg,
                                 m_style->radius, ImDrawFlags_RoundCornersTop);

        ImU32 col = m_style->border_color;
        float thickness = m_style->border_thickness;
        ImVec2 ptl = paddingTL;
        ImVec2 pbr = paddingBR;
        if (m_selected) {
            col = m_style->border_selected_color;
            thickness = m_style->border_selected_thickness;
        }
        if (thickness < 0.f) {
            ptl.x -= thickness / 2;
            ptl.y -= thickness / 2;
            pbr.x -= thickness / 2;
            pbr.y -= thickness / 2;
            thickness *= -1.f;
        }
        draw_list->AddRect(offset + m_pos - ptl, offset + m_pos + m_size + pbr, col, m_style->radius, 0, thickness);


        if (ImGui::IsWindowHovered() && !ImGui::IsKeyDown(ImGuiKey_LeftCtrl) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_inf->on_selected_node())
            selected(false);

        if (isHovered()) {
            m_inf->hoveredNode(this);
            if (mouseClickState) {
                selected(true);
                m_inf->consumeSingleUseClick();
            }
        }

        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete) && !ImGui::IsAnyItemActive() && isSelected() && !m_inf->getDisabled())
            destroy();

        bool onHeader = ImGui::IsMouseHoveringRect(offset + m_pos - paddingTL, offset + m_pos + headerSize);
        if (onHeader && mouseClickState && !m_inf->getDisabled()) {
            m_inf->consumeSingleUseClick();
            m_dragged = true;
            m_inf->draggingNode(true);
        }
        if (m_dragged || (m_selected && m_inf->isNodeDragged())) {
            float step = m_inf->getStyle().grid_size / m_inf->getStyle().grid_subdivisions;
            m_posTarget += ImGui::GetIO().MouseDelta;
            // "Slam" The position
            m_pos.x = round(m_posTarget.x / step) * step;
            m_pos.y = round(m_posTarget.y / step) * step;

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                m_dragged = false;
                m_inf->draggingNode(false);
                m_posTarget = m_pos;
            }
        }
        ImGui::PopID();

        // Deleting dead pins
        m_dynamicIns.erase(std::remove_if(m_dynamicIns.begin(), m_dynamicIns.end(),
                                          [](const std::pair<int, std::shared_ptr<Pin>> &p) { return p.first == 0; }),
                           m_dynamicIns.end());
        m_dynamicOuts.erase(std::remove_if(m_dynamicOuts.begin(), m_dynamicOuts.end(),
                                           [](const std::pair<int, std::shared_ptr<Pin>> &p) { return p.first == 0; }),
                            m_dynamicOuts.end());
    }

    // -----------------------------------------------------------------------------------------------------------------
    // HANDLER

    int ImNodeFlow::m_instances = 0;

    bool ImNodeFlow::on_selected_node() {
        return std::any_of(m_nodes.begin(), m_nodes.end(),
                           [](const auto &n) { return n.second->isSelected() && n.second->isHovered(); });
    }

    bool ImNodeFlow::on_free_space() {
        return std::all_of(m_nodes.begin(), m_nodes.end(),
                           [](const auto &n) { return !n.second->isHovered(); })
               && std::all_of(m_links.begin(), m_links.end(),
                              [](const auto &l) { return !l.lock()->isHovered(); });
    }

    ImVec2 ImNodeFlow::screen2grid( const ImVec2 & p )
    {
        if ( ImGui::GetCurrentContext() == m_context.getRawContext() )
            return p - m_context.scroll();
        return ( p - m_context.origin() ) / m_context.scale() - m_context.scroll();
    }

    ImVec2 ImNodeFlow::grid2screen( const ImVec2 & p )
    {
        if ( ImGui::GetCurrentContext() == m_context.getRawContext() )
            return p + m_context.scroll();
        return ( p + m_context.scroll() ) * m_context.scale() + m_context.origin();
    }

    void ImNodeFlow::addLink(std::shared_ptr<Link> &link) {
        m_links.push_back(link);
    }

    void ImNodeFlow::update(bool disabled) {
        // Updating looping stuff
        m_hovering = nullptr;
        m_hoveredNode = nullptr;
        m_draggingNode = m_draggingNodeNext;
        m_singleUseClick = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
	m_disabled = disabled;

        // Create child canvas
        m_context.begin();
        ImGui::GetIO().IniFilename = nullptr;
	
	//if disabled draw a transparent input-blocking window over the entire thing.  This blocks most input, though doesn't seem to block node dragging/selection
	if(m_disabled){
		ImGui::SetNextWindowPos({0, 0});
		ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
		ImGui::Begin("##ImNodeFlowInputBlocker", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);
		ImGui::End();
	}

        ImDrawList *draw_list = ImGui::GetWindowDrawList();

        // Display grid
        ImVec2 gridSize = ImGui::GetWindowSize();
        float subGridStep = m_style.grid_size / m_style.grid_subdivisions;
        for (float x = fmodf(m_context.scroll().x, m_style.grid_size); x < gridSize.x; x += m_style.grid_size)
            draw_list->AddLine(ImVec2(x, 0.0f), ImVec2(x, gridSize.y), m_style.colors.grid);
        for (float y = fmodf(m_context.scroll().y, m_style.grid_size); y < gridSize.y; y += m_style.grid_size)
            draw_list->AddLine(ImVec2(0.0f, y), ImVec2(gridSize.x, y), m_style.colors.grid);
        if (m_context.scale() > 0.7f) {
            for (float x = fmodf(m_context.scroll().x, subGridStep); x < gridSize.x; x += subGridStep)
                draw_list->AddLine(ImVec2(x, 0.0f), ImVec2(x, gridSize.y), m_style.colors.subGrid);
            for (float y = fmodf(m_context.scroll().y, subGridStep); y < gridSize.y; y += subGridStep)
                draw_list->AddLine(ImVec2(0.0f, y), ImVec2(gridSize.x, y), m_style.colors.subGrid);
        }

        // Update and draw nodes
        // TODO: I don't like this
        draw_list->ChannelsSplit(2);
        for (auto &node: m_nodes) {
		node.second->m_inf = this;
		node.second->update();
	}
        // Remove "toDelete" nodes
	destroyDestroyedNodes();
        draw_list->ChannelsMerge();

        // Box (rubber-band) selection: drag on empty canvas to select nodes in the rect.
        if (!m_disabled && !m_dragOut && !m_draggingNode && !m_hovering && !m_boxSelecting
            && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && on_free_space()) {
            m_boxSelecting = true;
            m_boxSelectStart = ImGui::GetMousePos();
        }
        if (m_boxSelecting) {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
                ImVec2 cur = ImGui::GetMousePos();
                ImVec2 rmin(fminf(m_boxSelectStart.x, cur.x), fminf(m_boxSelectStart.y, cur.y));
                ImVec2 rmax(fmaxf(m_boxSelectStart.x, cur.x), fmaxf(m_boxSelectStart.y, cur.y));
                draw_list->AddRectFilled(rmin, rmax, ImGui::GetColorU32(ImGuiCol_DragDropTarget, 0.15f));
                draw_list->AddRect(rmin, rmax, ImGui::GetColorU32(ImGuiCol_DragDropTarget, 0.85f), 0.f, 0, 1.5f);
                for (auto &n: m_nodes) {
                    ImVec2 p = grid2screen(n.second->getPos());
                    ImVec2 q(p.x + n.second->getSize().x, p.y + n.second->getSize().y);
                    bool overlap = !(q.x < rmin.x || p.x > rmax.x || q.y < rmin.y || p.y > rmax.y);
                    n.second->selected(overlap);
                }
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) m_boxSelecting = false;
        }

        for (auto &node: m_nodes) { node.second->updatePublicStatus(); }

        // Update and draw links
        for (auto &l: m_links) {
		if(!l.expired()){
			l.lock()->m_inf = this;
			l.lock()->update();
		}
	}

        // Links drop-off
        if (m_dragOut && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            if (!m_hovering) {
                if (on_free_space() && m_droppedLinkPopUp) {
                    if (m_droppedLinkPupUpComboKey == ImGuiKey_None || ImGui::IsKeyDown(m_droppedLinkPupUpComboKey)) {
                        m_droppedLinkLeft = m_dragOut;
                        m_openDroppedLinkPopUp = true;
                    }
                }
            } else
                m_dragOut->createLink(m_hovering);
        }

        // Links drag-out
        if (!m_draggingNode && m_hovering && !m_dragOut && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_disabled)
            m_dragOut = m_hovering;
        if (m_dragOut) {
            if (m_dragOut->getType() == PinType_Output)
                smart_bezier(m_dragOut->pinPoint(), ImGui::GetMousePos(), m_dragOut->getStyle()->color,
                             m_dragOut->getStyle()->extra.link_dragged_thickness);
            else
                smart_bezier(ImGui::GetMousePos(), m_dragOut->pinPoint(), m_dragOut->getStyle()->color,
                             m_dragOut->getStyle()->extra.link_dragged_thickness);

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                m_dragOut = nullptr;
        }

        // Only flag here; opened after m_context.end() so they don't scale with zoom.
        if (m_rightClickPopUp && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsWindowHovered()) {
            m_hoveredNodeAux = m_hoveredNode;
            m_openRightClickPopUp = true;
        }

        // Removing dead Links
        m_links.erase(std::remove_if(m_links.begin(), m_links.end(),
                                     [](const std::weak_ptr<Link> &l) { return l.expired(); }), m_links.end());

        // Clearing recursion blacklist
        m_pinRecursionBlacklist.clear();

        m_context.end();

        // Pop-ups, drawn in the outer (unscaled) context so they keep a fixed UI size.
        if (m_openRightClickPopUp) { ImGui::OpenPopup("RightClickPopUp"); m_openRightClickPopUp = false; }
        if (ImGui::BeginPopup("RightClickPopUp")) {
            m_rightClickPopUp(m_hoveredNodeAux);
            ImGui::EndPopup();
        }
        if (m_openDroppedLinkPopUp) { ImGui::OpenPopup("DroppedLinkPopUp"); m_openDroppedLinkPopUp = false; }
        if (ImGui::BeginPopup("DroppedLinkPopUp")) {
            m_droppedLinkPopUp(m_droppedLinkLeft);
            ImGui::EndPopup();
        }
    }

    void ImNodeFlow::destroyDestroyedNodes(){
        for (auto iter = m_nodes.begin(); iter != m_nodes.end();) {
            if (iter->second->toDestroy())
                iter = m_nodes.erase(iter);
            else
                ++iter;
        }
    }
}
