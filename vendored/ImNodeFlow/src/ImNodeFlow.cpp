#include "ImNodeFlow.h"
#include <algorithm>
#include <cfloat>
#include <cstring>

namespace ImFlow {
    // -----------------------------------------------------------------------------------------------------------------
    // LINK

    void Link::update() {
        ImVec2 start = m_left->pinPoint();
        ImVec2 end = m_right->pinPoint();
        ImVec2 mouse = ImGui::GetMousePos();
        float thickness = m_left->getStyle()->extra.link_thickness;

        // Waypoints are stored in grid space; route + interact in screen space.
        std::vector<ImVec2> mids;
        mids.reserve(m_waypoints.size());
        for (const auto& w : m_waypoints) mids.push_back(m_inf->grid2screen(w));

        // A pin on a mirrored node faces the opposite way, so the wire must leave/arrive there.
        bool leftMir = m_left->getParent() && m_left->getParent()->isMirrored();
        bool rightMir = m_right->getParent() && m_right->getParent()->isMirrored();
        float outDir = leftMir ? -1.0f : 1.0f;
        float inDir = rightMir ? 1.0f : -1.0f;

        std::vector<ImVec2> pts;
        build_link_polyline(start, end, mids, pts, outDir, inDir);

        if (!ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            m_selected = false;

        const float handleR = 5.0f;          // drawn handle radius (screen px)
        const float grabR2  = (handleR + 3.0f) * (handleR + 3.0f);
        bool canInteract = !m_inf->getDisabled() && !m_inf->getDraggedLink()
                           && !m_inf->isNodeDragged() && !m_inf->getDragOut();

        // Hovered waypoint handle (if any).
        int hoveredWp = -1;
        for (int i = 0; i < (int)mids.size(); ++i) {
            float dx = mouse.x - mids[i].x, dy = mouse.y - mids[i].y;
            if (dx * dx + dy * dy <= grabR2) { hoveredWp = i; break; }
        }

        // Drive an active waypoint drag, or begin one / remove a handle.
        if (m_draggedWaypoint >= 0) {
            if (m_draggedWaypoint < (int)m_waypoints.size())
                m_waypoints[m_draggedWaypoint] = m_inf->screen2grid(mouse);
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                m_draggedWaypoint = -1;
                m_inf->setDraggedLink(nullptr);
            }
        } else if (hoveredWp >= 0 && canInteract && m_inf->getSingleUseClick()) {
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                m_waypoints.erase(m_waypoints.begin() + hoveredWp);
            } else {
                m_draggedWaypoint = hoveredWp;
                m_selected = true;
                m_inf->setDraggedLink(this);
            }
            m_inf->consumeSingleUseClick();
        }

        // Body hover (off the handles). A click on the body inserts a new waypoint there.
        bool onBody = (hoveredWp < 0) && (m_draggedWaypoint < 0) && polyline_collider(mouse, pts, 5.0f);
        if (onBody) {
            m_hovered = true;
            thickness = m_left->getStyle()->extra.link_hovered_thickness;
            if (canInteract && m_inf->getSingleUseClick()) {
                m_inf->consumeSingleUseClick();
                m_selected = true;
                int seg = nearestControlSegment(mouse, start, mids, end);
                m_waypoints.insert(m_waypoints.begin() + seg, m_inf->screen2grid(mouse));
                m_draggedWaypoint = seg;
                m_inf->setDraggedLink(this);
            }
        } else {
            m_hovered = false;
        }

        if (m_hovered || hoveredWp >= 0 || m_draggedWaypoint >= 0)
            m_inf->hoveredLink(this);

        // Draw: optional selection underlay, then the link itself.
        if (m_selected)
            draw_link_solid(pts, m_left->getStyle()->extra.outline_color,
                            thickness + m_left->getStyle()->extra.link_selected_outline_thickness);

        if (m_left->getStyle()->extra.animated) {
            // Control-flow link: dashes flow from source to destination.
            float phase = (float)ImGui::GetTime() * -16.0f; // pixels / second
            draw_link_flow(pts, m_left->getStyle()->color, thickness, phase);
        } else {
            // Fade source colour -> destination colour, so a converted link reads as a transition.
            draw_link_gradient(pts, m_left->getStyle()->color, m_right->getStyle()->color, thickness);
        }

        // Waypoint handles, only while the link is in focus.
        if (m_hovered || m_selected || hoveredWp >= 0 || m_draggedWaypoint >= 0) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            for (int i = 0; i < (int)mids.size(); ++i) {
                bool active = (i == hoveredWp) || (i == m_draggedWaypoint);
                float r = active ? handleR + 1.0f : handleR;
                dl->AddCircleFilled(mids[i], r, m_left->getStyle()->color, 12);
                dl->AddCircle(mids[i], r, IM_COL32(255, 255, 255, 200), 12, 1.5f);
            }
        }

        if (m_selected && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            m_right->deleteLink(this);
    }

    int Link::nearestControlSegment(const ImVec2& p, const ImVec2& start, const std::vector<ImVec2>& mids, const ImVec2& end) {
        // Control points in order: start, waypoints..., end. Returns the segment index the
        // click is closest to, which is also the insertion index into the waypoint list.
        std::vector<ImVec2> cp;
        cp.reserve(mids.size() + 2);
        cp.push_back(start);
        for (const auto& m : mids) cp.push_back(m);
        cp.push_back(end);

        int best = 0;
        float bestD2 = FLT_MAX;
        for (int i = 0; i + 1 < (int)cp.size(); ++i) {
            ImVec2 a = cp[i], b = cp[i + 1];
            ImVec2 ab(b.x - a.x, b.y - a.y), ap(p.x - a.x, p.y - a.y);
            float len2 = ab.x * ab.x + ab.y * ab.y;
            float t = len2 > 0.f ? (ap.x * ab.x + ap.y * ab.y) / len2 : 0.f;
            t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
            float dx = p.x - (a.x + ab.x * t), dy = p.y - (a.y + ab.y * t);
            float d2 = dx * dx + dy * dy;
            if (d2 < bestD2) { bestD2 = d2; best = i; }
        }
        return best;
    }

    Link::~Link() {
        if (m_inf) {
            if (m_inf->getDraggedLink() == this) m_inf->setDraggedLink(nullptr);
            if (m_inf->getHoveredLink() == this) m_inf->hoveredLink(nullptr);
        }
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

        // Mirror the node when its logic input is fed from a node to our right, so the
        // control wire doesn't have to wrap back around. Node centres are compared (those
        // don't move when mirrored, so the decision is stable).
        m_mirrored = false;
        {
            float myCenterX = m_pos.x + m_size.x * 0.5f;
            for (auto &p: m_ins) {
                if (p->getStyle()->socket_shape != 3) continue; // logic (triangle) inputs only
                for (auto &wl: p->getLinks()) {
                    if (auto l = wl.lock()) {
                        BaseNode* src = l->left() ? l->left()->getParent() : nullptr;
                        if (src && src->getPos().x + src->getSize().x * 0.5f > myCenterX)
                            m_mirrored = true;
                        break;
                    }
                }
                break; // the first logic input decides
            }
        }

        // A node renders as [left column] [content] [right column]. Normally that's
        // inputs | content | outputs; when mirrored the two pin columns swap sides.
        float nodeW = titleW > m_size.x ? titleW : m_size.x; // full node width (incl. drawBottom)

        // Left column: pins flow flush against the node's left edge.
        auto drawLeftColumn = [&](std::vector<std::shared_ptr<Pin>>& statics,
                                  std::vector<std::pair<int, std::shared_ptr<Pin>>>& dyn, bool isInput) {
            ImGui::BeginGroup();
            for (auto &p: statics) { p->setPos(ImGui::GetCursorPos()); p->update(); }
            for (auto &p: dyn) {
                if (isInput && p.first != 1) continue;
                p.second->setPos(ImGui::GetCursorPos());
                p.second->update();
                if (isInput) p.first = 0; else p.first -= 1;
            }
            ImGui::EndGroup();
        };

        // Right column: pins are right-aligned to the node's right edge.
        auto drawRightColumn = [&](std::vector<std::shared_ptr<Pin>>& statics,
                                   std::vector<std::pair<int, std::shared_ptr<Pin>>>& dyn, bool isInput) {
            float maxW = 0.0f;
            for (auto &p: statics) maxW = std::max(maxW, p->calcWidth());
            for (auto &p: dyn) maxW = std::max(maxW, p.second->calcWidth());
            auto place = [&](Pin* pin) {
                // FIXME: This looks horrible (inherited from the original output layout)
                if ((m_pos + ImVec2(nodeW, 0) + m_inf->getGrid().scroll()).x <
                    ImGui::GetCursorPos().x + ImGui::GetWindowPos().x + maxW)
                    pin->setPos(ImGui::GetCursorPos() + ImGui::GetWindowPos() + ImVec2(maxW - pin->calcWidth(), 0.f));
                else
                    pin->setPos(ImVec2((m_pos + ImVec2(nodeW - pin->calcWidth(), 0) + m_inf->getGrid().scroll()).x,
                                       ImGui::GetCursorPos().y + ImGui::GetWindowPos().y));
            };
            ImGui::BeginGroup();
            for (auto &p: statics) { place(p.get()); p->update(); }
            for (auto &p: dyn) {
                if (isInput && p.first != 1) continue;
                place(p.second.get());
                p.second->update();
                if (isInput) p.first = 0; else p.first -= 1;
            }
            ImGui::EndGroup();
        };

        std::vector<std::shared_ptr<Pin>>& leftStatics = m_mirrored ? m_outs : m_ins;
        auto& leftDyn  = m_mirrored ? m_dynamicOuts : m_dynamicIns;
        std::vector<std::shared_ptr<Pin>>& rightStatics = m_mirrored ? m_ins : m_outs;
        auto& rightDyn = m_mirrored ? m_dynamicIns : m_dynamicOuts;

        if (!leftStatics.empty()) {
            drawLeftColumn(leftStatics, leftDyn, !m_mirrored);
            ImGui::SameLine();
        }

        ImGui::BeginGroup();
        draw();
        ImGui::Dummy(ImVec2(0.f, 0.f));
        ImGui::EndGroup();
        ImGui::SameLine();

        drawRightColumn(rightStatics, rightDyn, m_mirrored);

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

    std::shared_ptr<NodeGroup> ImNodeFlow::addGroup(const std::string& title, const ImVec2& pos, const ImVec2& size) {
        auto g = std::make_shared<NodeGroup>(title, pos, size);
        m_groups.push_back(g);
        return g;
    }

    namespace { constexpr float GRP_HEADER_H = 26.0f, GRP_HANDLE = 16.0f, GRP_MIN_W = 80.0f, GRP_MIN_H = 60.0f; }

    void ImNodeFlow::drawGroups() {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        for (auto& g : m_groups) {
            ImVec2 tl = grid2screen(g->m_pos);
            ImVec2 br = grid2screen(g->m_pos + g->m_size);
            bool sel = (g.get() == m_selectedGroup) || (g.get() == m_hoveredGroup);
            ImU32 outline = sel ? IM_COL32(0xFF, 0x99, 0x55, 0xFF) : IM_COL32(0xAA, 0xAA, 0xAA, 0xC0);

            dl->AddRect(tl, br, outline, 4.0f, 0, sel ? 2.0f : 1.5f);

            // Header strip (faint) so the title reads against the grid.
            ImVec2 hbr = grid2screen(g->m_pos + ImVec2(g->m_size.x, GRP_HEADER_H));
            dl->AddRectFilled(tl, hbr, IM_COL32(0xAA, 0xAA, 0xAA, 0x22), 4.0f, ImDrawFlags_RoundCornersTop);
            dl->AddLine(ImVec2(tl.x, hbr.y), ImVec2(hbr.x, hbr.y), outline, 1.0f);

            if (g.get() != m_editingGroup)
                dl->AddText(grid2screen(g->m_pos + ImVec2(8.0f, 5.0f)), IM_COL32(0xEE, 0xEE, 0xEE, 0xFF), g->m_title.c_str());

            // Resize grip (bottom-right corner).
            ImVec2 hTL = grid2screen(g->m_pos + g->m_size - ImVec2(GRP_HANDLE, GRP_HANDLE));
            dl->AddTriangleFilled(ImVec2(br.x, hTL.y), br, ImVec2(hTL.x, br.y), outline);
        }
    }

    void ImNodeFlow::updateGroups() {
        m_hoveredGroup = nullptr;

        // Drive an in-progress drag/resize.
        if (m_activeGroup) {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            if (m_activeGroup->m_resizing) {
                m_activeGroup->m_size.x = fmaxf(GRP_MIN_W, m_activeGroup->m_size.x + d.x);
                m_activeGroup->m_size.y = fmaxf(GRP_MIN_H, m_activeGroup->m_size.y + d.y);
            } else if (m_activeGroup->m_dragging) {
                m_activeGroup->m_pos = m_activeGroup->m_pos + d;
                for (auto* n : m_activeGroup->m_held) n->setPos(n->getPos() + d);
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                m_activeGroup->m_dragging = m_activeGroup->m_resizing = false;
                m_activeGroup->m_held.clear();
                m_activeGroup = nullptr;
            }
            return;
        }

        bool busy = m_draggingNode || m_hovering || m_hoveredNode || m_dragOut
                    || m_draggedLink || m_hoveredLink || m_boxSelecting || m_editingGroup;

        // Hover + begin interaction, top-most group first.
        for (auto it = m_groups.rbegin(); it != m_groups.rend(); ++it) {
            NodeGroup* g = it->get();
            ImVec2 tl = grid2screen(g->m_pos);
            ImVec2 hbr = grid2screen(g->m_pos + ImVec2(g->m_size.x, GRP_HEADER_H));
            ImVec2 handleTL = grid2screen(g->m_pos + g->m_size - ImVec2(GRP_HANDLE, GRP_HANDLE));
            ImVec2 br = grid2screen(g->m_pos + g->m_size);

            bool onHeader = ImGui::IsMouseHoveringRect(tl, hbr);
            bool onHandle = ImGui::IsMouseHoveringRect(handleTL, br);
            if (busy || (!onHeader && !onHandle)) continue;

            m_hoveredGroup = g;
            if (onHeader && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                m_editingGroup = g;
                m_selectedGroup = g;
                m_editGroupFocusFrames = 2;
                strncpy(m_editGroupBuf, g->m_title.c_str(), sizeof(m_editGroupBuf) - 1);
                m_editGroupBuf[sizeof(m_editGroupBuf) - 1] = '\0';
                consumeSingleUseClick();
            } else if (getSingleUseClick()) {
                m_selectedGroup = g;
                m_activeGroup = g;
                if (onHandle) {
                    g->m_resizing = true;
                } else {
                    g->m_dragging = true;
                    g->m_held.clear();
                    for (auto& n : m_nodes) {
                        ImVec2 c = n.second->getPos() + n.second->getSize() * 0.5f;
                        if (c.x >= g->m_pos.x && c.x <= g->m_pos.x + g->m_size.x &&
                            c.y >= g->m_pos.y && c.y <= g->m_pos.y + g->m_size.y)
                            g->m_held.push_back(n.second.get());
                    }
                }
                consumeSingleUseClick();
            }
            break;
        }

        // Deselect when a click misses every group.
        if (!m_hoveredGroup && !m_editingGroup && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            m_selectedGroup = nullptr;

        // Delete the selected group (soft: contained nodes stay where they are).
        if (m_selectedGroup && !m_editingGroup && ImGui::IsWindowFocused()
            && ImGui::IsKeyPressed(ImGuiKey_Delete) && !ImGui::IsAnyItemActive()) {
            m_selectedGroup->destroy();
            m_selectedGroup = nullptr;
        }

        // Inline rename field, drawn on top in canvas space.
        if (m_editingGroup) {
            ImGui::SetCursorScreenPos(grid2screen(m_editingGroup->m_pos + ImVec2(6.0f, 3.0f)));
            ImGui::SetNextItemWidth(fmaxf(60.0f, m_editingGroup->m_size.x - 12.0f));
            if (m_editGroupFocusFrames > 0) { --m_editGroupFocusFrames; ImGui::SetKeyboardFocusHere(); }
            bool enter = ImGui::InputText("##groupRename", m_editGroupBuf, sizeof(m_editGroupBuf),
                                          ImGuiInputTextFlags_EnterReturnsTrue);
            if (enter || ImGui::IsItemDeactivated()) {
                m_editingGroup->m_title = m_editGroupBuf;
                m_editingGroup = nullptr;
            }
        }

        m_groups.erase(std::remove_if(m_groups.begin(), m_groups.end(),
                       [](const std::shared_ptr<NodeGroup>& g){ return g->isDestroyed(); }), m_groups.end());
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
        draw_list->ChannelsSetCurrent(0);
        drawGroups(); // behind the node backgrounds
        for (auto &node: m_nodes) {
		node.second->m_inf = this;
		node.second->update();
	}
        // Remove "toDelete" nodes
	destroyDestroyedNodes();
        draw_list->ChannelsMerge();

        // Group boxes: hover/drag/resize/rename, now that node hover state is known.
        updateGroups();

        // Box (rubber-band) selection: drag on empty canvas to select nodes in the rect.
        // (m_hoveredLink / m_draggedLink hold last frame's link state, so a click that lands
        // on a link edits the link instead of starting a box-select.)
        if (!m_disabled && !m_dragOut && !m_draggingNode && !m_hovering && !m_boxSelecting
            && !m_hoveredLink && !m_draggedLink && !m_hoveredGroup && !m_activeGroup
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

        // Update and draw links. m_hoveredLink is recomputed here each frame; box-select
        // (above) reads the value left from the previous frame.
        m_hoveredLink = nullptr;
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
