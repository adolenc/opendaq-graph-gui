#include "nodes.h"

using namespace ImGui;


#define IS_SET(state, flag)         ((state) & (flag))
#define SET_FLAG(state, flag)       ((state) |= (flag))
#define CLEAR_FLAG(state, flag)     ((state) &= ~(flag))
#define TOGGLE_FLAG(state, flag)    ((state) ^= (flag))
#define HAS_ALL_FLAGS(state, flags) (((state) & (flags)) == (flags))
#define HAS_ANY_FLAG(state, flags)  ((state) & (flags))
#define CLEAR_FLAGS(state, flags)   ((state) &= ~(flags))

static bool OtherImGuiWindowIsBlockingInteraction()
{
    return ImGui::GetIO().WantCaptureMouse && !ImGui::IsWindowHovered();
}

unsigned int ImGuiNodesIdentifier::id_counter_ = 0;

void ImGuiNodes::UpdateCanvasGeometry(ImDrawList* draw_list)
{
    const ImGuiIO& io = ImGui::GetIO();

    mouse_ = ImGui::GetMousePos();

    {
        ImVec2 min = ImGui::GetWindowContentRegionMin();
        ImVec2 max = ImGui::GetWindowContentRegionMax();

        nodes_imgui_window_pos_ = ImGui::GetWindowPos() + min;
        nodes_imgui_window_size_ = max - min;	
    }

    ImRect canvas(nodes_imgui_window_pos_, nodes_imgui_window_pos_ + nodes_imgui_window_size_);

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_0))
    {
        scroll_ = {};
        scale_ = 1.0f;
    }
        
    if (!ImGui::IsMouseDown(0) && canvas.Contains(mouse_))
    {
        static bool blocked_by_imgui_interaction = false;

        if (ImGui::IsMouseClicked(1))
            blocked_by_imgui_interaction = OtherImGuiWindowIsBlockingInteraction();
        
        if (ImGui::IsMouseDragging(1) && !blocked_by_imgui_interaction)
            scroll_ += io.MouseDelta;

        ImVec2 focus = (mouse_ - scroll_ - nodes_imgui_window_pos_) / scale_;

        if (io.MouseWheel < 0.0f)
            for (float zoom = io.MouseWheel; zoom < 0.0f; zoom += 1.0f)
                scale_ = ImMax(0.3f, scale_ / 1.15f);

        if (io.MouseWheel > 0.0f)
            for (float zoom = io.MouseWheel; zoom > 0.0f; zoom -= 1.0f)
                scale_ = ImMin(3.0f, scale_ * 1.15f);

        ImVec2 shift = scroll_ + (focus * scale_);
        scroll_ += mouse_ - shift - nodes_imgui_window_pos_;

        if (ImGui::IsMouseReleased(1) && !active_node_)
        {
            bool was_blocked = blocked_by_imgui_interaction;
            blocked_by_imgui_interaction = false;

            if (!was_blocked && io.MouseDragMaxDistanceSqr[1] < io.MouseDragThreshold * io.MouseDragThreshold)
                ImGui::OpenPopup("NodesContextMenu");
        }
    }

    const float grid = 64.0f * scale_;
    for (float x = fmodf(scroll_.x, grid); x < nodes_imgui_window_size_.x; x += grid)
    {		
        for (float y = fmodf(scroll_.y, grid); y < nodes_imgui_window_size_.y; y += grid)
        {
            int mark_x = (int)((x - scroll_.x) / grid);
            int mark_y = (int)((y - scroll_.y) / grid);
            ImColor color = (mark_y % 5) || (mark_x % 5) ? ImColor(0.3f, 0.3f, 0.3f, .3f) : ImColor(1.0f, 1.0f, 1.0f, .3f);
            draw_list->AddCircleFilled(ImVec2(x, y) + nodes_imgui_window_pos_, 2.0f * scale_, color);
        }
    }
}

ImGuiNodesNode* ImGuiNodes::UpdateNodesFromCanvas()
{
    if (nodes_.empty())
        return NULL;

    const ImGuiIO& io = ImGui::GetIO();

    ImVec2 offset = nodes_imgui_window_pos_ + scroll_;
    ImRect canvas(nodes_imgui_window_pos_, nodes_imgui_window_pos_ + nodes_imgui_window_size_);
    ImGuiNodesNode* hovered_node = NULL;

    for (int node_idx = nodes_.size() - 1; node_idx >= 0; --node_idx)
    {
        ImGuiNodesNode* node = nodes_[node_idx];
        IM_ASSERT(node);

        ImRect node_rect = node->area_node_;
        node_rect.Min *= scale_;
        node_rect.Max *= scale_;
        node_rect.Translate(offset);

        node_rect.ClipWith(canvas);

        if (canvas.Overlaps(node_rect))
        {
            SET_FLAG(node->state_, ImGuiNodesNodeStateFlag_Visible);
            CLEAR_FLAG(node->state_, ImGuiNodesNodeStateFlag_Hovered);
        }
        else
        {
            CLEAR_FLAGS(node->state_, ImGuiNodesNodeStateFlag_Visible | ImGuiNodesNodeStateFlag_Hovered | ImGuiNodesNodeStateFlag_MarkedForSelection);
            continue;
        }

        if (!hovered_node && node_rect.Contains(mouse_))
            hovered_node = node;

        if (state_ == ImGuiNodesState_Selecting)
        {
            if (active_dragging_selection_area_.Overlaps(node_rect))
            {
                SET_FLAG(node->state_, ImGuiNodesNodeStateFlag_MarkedForSelection);
                continue;
            }

            CLEAR_FLAG(node->state_, ImGuiNodesNodeStateFlag_MarkedForSelection);
        }

        for (int input_idx = 0; input_idx < node->inputs_.size(); ++input_idx)
        {
            ImGuiNodesInput& input = node->inputs_[input_idx];

            CLEAR_FLAGS(input.state_, ImGuiNodesConnectorStateFlag_Hovered | ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget | ImGuiNodesConnectorStateFlag_Dragging);

            if (state_ == ImGuiNodesState_DraggingInput)
            {
                if (&input == active_input_)
                    SET_FLAG(input.state_, ImGuiNodesConnectorStateFlag_Dragging);

                continue;
            }				

            if (state_ == ImGuiNodesState_DraggingOutput)
            {
                if (active_node_ == node)
                    continue;

                if (!input.source_node_)
                    SET_FLAG(input.state_, ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget);
            }

            if (!hovered_node || hovered_node != node)
                continue;

            if (state_ == ImGuiNodesState_Selecting)
                continue;

            ImRect input_rect = input.area_input_;
            input_rect.Min *= scale_;
            input_rect.Max *= scale_;
            input_rect.Translate(offset);

            if (input_rect.Contains(mouse_))
            {
                if (state_ != ImGuiNodesState_DraggingOutput)
                {
                    SET_FLAG(input.state_, ImGuiNodesConnectorStateFlag_Hovered);
                    continue;
                }
                
                if (IS_SET(input.state_, ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget))
                    SET_FLAG(input.state_, ImGuiNodesConnectorStateFlag_Hovered);
            }				
        }

        for (int output_idx = 0; output_idx < node->outputs_.size(); ++output_idx)
        {
            ImGuiNodesOutput& output = node->outputs_[output_idx];

            CLEAR_FLAGS(output.state_, ImGuiNodesConnectorStateFlag_Hovered | ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget | ImGuiNodesConnectorStateFlag_Dragging);

            if (state_ == ImGuiNodesState_DraggingOutput)
            {
                if (&output == active_output_)
                    SET_FLAG(output.state_, ImGuiNodesConnectorStateFlag_Dragging);

                continue;
            }

            if (state_ == ImGuiNodesState_DraggingInput)
            {
                if (active_node_ == node)
                    continue;

                if (!active_input_->source_node_)
                    SET_FLAG(output.state_, ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget);
            }

            if (!hovered_node || hovered_node != node)
                continue;
        
            if (state_ == ImGuiNodesState_Selecting)
                continue;
            
            ImRect output_rect = output.area_output_;
            output_rect.Min *= scale_;
            output_rect.Max *= scale_;
            output_rect.Translate(offset);

            if (output_rect.Contains(mouse_))
            {
                if (state_ != ImGuiNodesState_DraggingInput)
                {
                    SET_FLAG(output.state_, ImGuiNodesConnectorStateFlag_Hovered);
                    continue;
                }

                if (IS_SET(output.state_, ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget))
                    SET_FLAG(output.state_, ImGuiNodesConnectorStateFlag_Hovered);
            }
        }	
    }

    if (hovered_node)
        SET_FLAG(hovered_node->state_, ImGuiNodesNodeStateFlag_Hovered);

    return hovered_node;
}

void ImGuiNodes::AddNode(const ImGuiNodesIdentifier& name, ImColor color, 
                         const std::vector<ImGuiNodesIdentifier>& inputs,
                         const std::vector<ImGuiNodesIdentifier>& outputs,
                         ImGuiNodesUid parent_uid)
{
    ImVec2 pos(0.0f, 0.0f);
    if (!parent_uid.empty())
    {
        for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
        {
            ImGuiNodesNode* node = nodes_[node_idx];
            if (node->uid_ == parent_uid)
            {
                pos = node->area_node_.GetCenter() + ImVec2(200.0f, 0.0f);
                break;
            }
        }
    }
    pos += ImVec2(0.0f, (float)(nodes_.size() * 20));
    AddNode(name, color, pos, inputs, outputs, parent_uid);
}

void ImGuiNodes::AddNode(const ImGuiNodesIdentifier& name, ImColor color, ImVec2 pos, 
                         const std::vector<ImGuiNodesIdentifier>& inputs,
                         const std::vector<ImGuiNodesIdentifier>& outputs,
                        ImGuiNodesUid parent_uid)
{
    ImGuiNodesNode* node = new ImGuiNodesNode(name, color);
    node->parent_node_ = NULL;
    if (!parent_uid.empty())
    {
        for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
        {
            ImGuiNodesNode* parent_node = nodes_[node_idx];
            if (parent_node->uid_ == parent_uid)
            {
                node->parent_node_ = parent_node;
                break;
            }
        }
    }

    for (const auto& input : inputs)
        node->inputs_.push_back(ImGuiNodesInput(input));
    
    for (const auto& output : outputs)
        node->outputs_.push_back(ImGuiNodesOutput(output));
    
    ImVec2 inputs_size;
    ImVec2 outputs_size;
    for (int input_idx = 0; input_idx < node->inputs_.size(); ++input_idx)
    {
        const ImGuiNodesInput& input = node->inputs_[input_idx];
        inputs_size.x = ImMax(inputs_size.x, input.area_input_.GetWidth());
        inputs_size.y += input.area_input_.GetHeight();
    }
    for (int output_idx = 0; output_idx < node->outputs_.size(); ++output_idx)
    {
        const ImGuiNodesOutput& output = node->outputs_[output_idx];
        outputs_size.x = ImMax(outputs_size.x, output.area_output_.GetWidth());
        outputs_size.y += output.area_output_.GetHeight();
    }

    node->BuildNodeGeometry(inputs_size, outputs_size);
    node->TranslateNode(pos - node->area_node_.GetCenter());
    SET_FLAG(node->state_, ImGuiNodesNodeStateFlag_Visible | ImGuiNodesNodeStateFlag_Hovered);

    nodes_.push_back(node);
}

bool ImGuiNodes::SortSelectedNodesOrder()
{
    bool any_node_selected = false;

    ImVector<ImGuiNodesNode*> nodes_unselected;
    nodes_unselected.reserve(nodes_.size());

    ImVector<ImGuiNodesNode*> nodes_selected;
    nodes_selected.reserve(nodes_.size());

    std::vector<ImGuiNodesUid> selected_ids;
    for (ImGuiNodesNode** iterator = nodes_.begin(); iterator != nodes_.end(); ++iterator)
    {
        ImGuiNodesNode* node = ((ImGuiNodesNode*)*iterator);
        if (HAS_ANY_FLAG(node->state_, ImGuiNodesNodeStateFlag_MarkedForSelection | ImGuiNodesNodeStateFlag_Selected))
        {
            any_node_selected = true;
            CLEAR_FLAG(node->state_, ImGuiNodesNodeStateFlag_MarkedForSelection);
            SET_FLAG(node->state_, ImGuiNodesNodeStateFlag_Selected);
            nodes_selected.push_back(node);
            selected_ids.push_back(node->uid_);
            for (int input_idx = 0; input_idx < node->inputs_.size(); ++input_idx)
                if (IS_SET(node->inputs_[input_idx].state_, ImGuiNodesConnectorStateFlag_Selected))
                    selected_ids.push_back(node->inputs_[input_idx].uid_);
            for (int output_idx = 0; output_idx < node->outputs_.size(); ++output_idx)
                if (IS_SET(node->outputs_[output_idx].state_, ImGuiNodesConnectorStateFlag_Selected))
                    selected_ids.push_back(node->outputs_[output_idx].uid_);
        }
        else
            nodes_unselected.push_back(node);
    }

    if (interaction_handler_)
        interaction_handler_->OnSelectionChanged(selected_ids);

    int node_idx = 0;

    for (int unselected_idx = 0; unselected_idx < nodes_unselected.size(); ++unselected_idx)
        nodes_[node_idx++] = nodes_unselected[unselected_idx];

    for (int selected_idx = 0; selected_idx < nodes_selected.size(); ++selected_idx)
        nodes_[node_idx++] = nodes_selected[selected_idx];

    return any_node_selected;
}

void ImGuiNodes::Update()
{
    bool was_hovering_output = (state_ == ImGuiNodesState_HoveringOutput);
    ImGuiNodesUid previous_active_output_uid = active_output_ ? active_output_->uid_ : "";

    ProcessInteractions();

    if (state_ == ImGuiNodesState_HoveringOutput)
    {
        if (interaction_handler_ && active_output_)
        {
            if (previous_active_output_uid != active_output_->uid_)
                interaction_handler_->OnOutputHover("");
            interaction_handler_->OnOutputHover(active_output_->uid_);
        }
    }
    else if (was_hovering_output && interaction_handler_)
        interaction_handler_->OnOutputHover("");

    ProcessNodes();
    ProcessContextMenu();
}

void ImGuiNodes::ProcessInteractions()
{
    static bool blocked_by_imgui_interaction = false;

    const ImGuiIO& io = ImGui::GetIO();
    
    UpdateCanvasGeometry(ImGui::GetWindowDrawList());

    ImGuiNodesNode* hovered_node = UpdateNodesFromCanvas();

    bool consider_hover = state_ == ImGuiNodesState_Default;
    consider_hover |= state_ == ImGuiNodesState_HoveringNode;
    consider_hover |= state_ == ImGuiNodesState_HoveringInput;
    consider_hover |= state_ == ImGuiNodesState_HoveringOutput;

    if (hovered_node && consider_hover)
    {
        active_input_ = NULL;
        active_output_ = NULL;

        for (int input_idx = 0; input_idx < hovered_node->inputs_.size(); ++input_idx)
        {
            if (IS_SET(hovered_node->inputs_[input_idx].state_, ImGuiNodesConnectorStateFlag_Hovered))
            {
                active_input_ = &hovered_node->inputs_[input_idx];
                state_ = ImGuiNodesState_HoveringInput;
                break;
            }
        }						
        
        for (int output_idx = 0; output_idx < hovered_node->outputs_.size(); ++output_idx)
        {
            if (IS_SET(hovered_node->outputs_[output_idx].state_, ImGuiNodesConnectorStateFlag_Hovered))
            {
                active_output_ = &hovered_node->outputs_[output_idx];
                state_ = ImGuiNodesState_HoveringOutput;
                break;
            }
        }					

        if (!active_input_ && !active_output_)
            state_ = ImGuiNodesState_HoveringNode;	
    }

    if (state_ == ImGuiNodesState_DraggingInput)
    {
        active_output_ = NULL;
    
        if (hovered_node)
        {
            for (int output_idx = 0; output_idx < hovered_node->outputs_.size(); ++output_idx)
            {
                ImGuiNodesConnectorState state = hovered_node->outputs_[output_idx].state_;
                if (HAS_ALL_FLAGS(state, ImGuiNodesConnectorStateFlag_Hovered | ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget))
                    active_output_ = &hovered_node->outputs_[output_idx];
            }
        }
    }
    
    if (state_ == ImGuiNodesState_DraggingOutput)
    {
        active_input_ = NULL;
    
        if (hovered_node)
        {
            for (int input_idx = 0; input_idx < hovered_node->inputs_.size(); ++input_idx)
            {
                ImGuiNodesConnectorState state = hovered_node->inputs_[input_idx].state_;

                if (HAS_ALL_FLAGS(state, ImGuiNodesConnectorStateFlag_Hovered | ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget))
                    active_input_ = &hovered_node->inputs_[input_idx];
            }
        }
    }

    if (consider_hover)
    {
        active_node_ = hovered_node;

        if (!hovered_node)
            state_ = ImGuiNodesState_Default;
    }

    if (ImGui::IsMouseDoubleClicked(0))
    {
        if (OtherImGuiWindowIsBlockingInteraction())
            return;

        switch (state_)
        {
            case ImGuiNodesState_Default:
            {
                for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                    CLEAR_FLAGS(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected | ImGuiNodesNodeStateFlag_MarkedForSelection | ImGuiNodesNodeStateFlag_Hovered);

                return;
            };

            case ImGuiNodesState_HoveringInput:
            {
                if (active_input_->source_node_)
                {
                    active_input_->source_output_->connections_count_--;
                    active_input_->source_output_ = NULL;
                    active_input_->source_node_ = NULL;

                    state_ = ImGuiNodesState_DraggingInput;
                }

                return;
            }

            case ImGuiNodesState_HoveringNode:
            {
                IM_ASSERT(active_node_);
                if (IS_SET(active_node_->state_, ImGuiNodesNodeStateFlag_Collapsed))
                {
                    CLEAR_FLAG(active_node_->state_, ImGuiNodesNodeStateFlag_Collapsed);
                    active_node_->area_node_.Max.y += active_node_->body_height_;
                    active_node_->TranslateNode(ImVec2(0.0f, active_node_->body_height_ * -0.5f));
                }
                else
                {
                    SET_FLAG(active_node_->state_, ImGuiNodesNodeStateFlag_Collapsed);
                    active_node_->area_node_.Max.y -= active_node_->body_height_;

                    //const ImVec2 click = (mouse_ - scroll_ - pos_) / scale_;
                    //const ImVec2 position = click - active_node_->area_node_.GetCenter();

                    active_node_->TranslateNode(ImVec2(0.0f, active_node_->body_height_ * 0.5f));
                }

                state_ = ImGuiNodesState_Dragging;
                return;
            }
        }
    }

    if (ImGui::IsMouseClicked(0))
    {
        if (OtherImGuiWindowIsBlockingInteraction())
        {
            blocked_by_imgui_interaction = true;
            return;
        }
        
        blocked_by_imgui_interaction = false;

        switch (state_)
        {				
            case ImGuiNodesState_HoveringNode:
            {
                if (io.KeyCtrl)
                    TOGGLE_FLAG(active_node_->state_, ImGuiNodesNodeStateFlag_Selected);
                else
                {
                    if (!IS_SET(active_node_->state_, ImGuiNodesNodeStateFlag_Selected))
                    {
                        for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                            CLEAR_FLAG(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected);
                        ClearAllConnectorSelections();
                    }

                    SET_FLAG(active_node_->state_, ImGuiNodesNodeStateFlag_Selected);
                }

                SortSelectedNodesOrder();

                state_ = ImGuiNodesState_Dragging;
                return;
            }

            case ImGuiNodesState_HoveringInput:
            {
                if (!active_input_->source_node_)
                    state_ = ImGuiNodesState_DraggingInput;
                else
                    state_ = ImGuiNodesState_Dragging;

                return;
            }

            case ImGuiNodesState_HoveringOutput:
            {
                state_ = ImGuiNodesState_DraggingOutput;
                return;
            }
        }

        if (!io.KeyCtrl)
        {
            for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                CLEAR_FLAG(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected);
            
            ClearAllConnectorSelections();
        }

        return;
    }

    if (ImGui::IsMouseDragging(0))
    {
        if (blocked_by_imgui_interaction)
            return;
            
        switch (state_)
        {
            case ImGuiNodesState_Default:
            {
                ImRect canvas(nodes_imgui_window_pos_, nodes_imgui_window_pos_ + nodes_imgui_window_size_);
                if (!canvas.Contains(mouse_))
                    return;

                if (blocked_by_imgui_interaction)
                    return;

                if (!io.KeyCtrl)
                {
                    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                        CLEAR_FLAGS(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected | ImGuiNodesNodeStateFlag_MarkedForSelection);

                    ClearAllConnectorSelections();
                }

                state_ = ImGuiNodesState_Selecting;
                return;
            }

            case ImGuiNodesState_Selecting:
            {
                const ImVec2 pos = mouse_ - ImGui::GetMouseDragDelta(0);

                active_dragging_selection_area_.Min = ImMin(pos, mouse_);
                active_dragging_selection_area_.Max = ImMax(pos, mouse_);

                return;
            }

            case ImGuiNodesState_Dragging:
            {
                if (active_input_ && active_input_->source_output_ && active_input_->source_output_->connections_count_ > 0)
                {
                    active_node_ = active_input_->source_node_;
                    active_output_ = active_input_->source_output_;

                    active_input_->source_output_->connections_count_--;
                    active_input_->source_output_ = NULL;
                    active_input_->source_node_ = NULL;

                    state_ = ImGuiNodesState_DraggingOutput;
                    return;
                }

                if (!IS_SET(active_node_->state_, ImGuiNodesNodeStateFlag_Selected))
                    active_node_->TranslateNode(io.MouseDelta / scale_, false);
                else
                    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                        nodes_[node_idx]->TranslateNode(io.MouseDelta / scale_, true);

                return;
            }

            case ImGuiNodesState_DraggingInput:
            {
                ImVec2 offset = nodes_imgui_window_pos_ + scroll_;
                ImVec2 p1 = offset + (active_input_->pos_ * scale_);
                ImVec2 p4 = active_output_ ? (offset + (active_output_->pos_ * scale_)) : mouse_;

                active_dragging_connection_ = ImVec4(p1.x, p1.y, p4.x, p4.y);
                return;
            }

            case ImGuiNodesState_DraggingOutput:
            {
                ImVec2 offset = nodes_imgui_window_pos_ + scroll_;
                ImVec2 p1 = offset + (active_output_->pos_ * scale_);
                ImVec2 p4 = active_input_ ? (offset + (active_input_->pos_ * scale_)) : mouse_;

                active_dragging_connection_ = ImVec4(p4.x, p4.y, p1.x, p1.y);
                return;
            }
        }

        return;
    }

    if (ImGui::IsMouseReleased(0))
    {
        blocked_by_imgui_interaction = false;

        switch (state_)
        {
        case ImGuiNodesState_Selecting:
        {
            active_node_ = NULL;
            active_input_ = NULL;
            active_output_ = NULL;

            active_dragging_selection_area_ = {};

            SortSelectedNodesOrder();
            state_ = ImGuiNodesState_Default;
            return;
        }

        case ImGuiNodesState_Dragging:
        {
            // Check if this was a click on an input/output without much dragging
            if (io.MouseDragMaxDistanceSqr[0] < (io.MouseDragThreshold * io.MouseDragThreshold))
            {
                if (active_input_)
                {
                    if (io.KeyCtrl)
                    {
                        TOGGLE_FLAG(active_input_->state_, ImGuiNodesConnectorStateFlag_Selected);
                    }
                    else
                    {
                        ClearAllConnectorSelections();
                        for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                            CLEAR_FLAG(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected);
                        SET_FLAG(active_input_->state_, ImGuiNodesConnectorStateFlag_Selected);
                    }
                    state_ = ImGuiNodesState_HoveringInput;
                }
                else if (active_output_)
                {
                    if (io.KeyCtrl)
                    {
                        TOGGLE_FLAG(active_output_->state_, ImGuiNodesConnectorStateFlag_Selected);
                    }
                    else
                    {
                        ClearAllConnectorSelections();
                        for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                            CLEAR_FLAG(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected);
                        SET_FLAG(active_output_->state_, ImGuiNodesConnectorStateFlag_Selected);
                    }
                    state_ = ImGuiNodesState_HoveringOutput;
                }
                else
                {
                    state_ = ImGuiNodesState_HoveringNode;
                }
            }
            else
            {
                // This was an actual drag, transition to appropriate hover state
                if (active_input_)
                    state_ = ImGuiNodesState_HoveringInput;
                else if (active_output_)
                    state_ = ImGuiNodesState_HoveringOutput;
                else
                    state_ = ImGuiNodesState_HoveringNode;
            }
            return;
        }

        case ImGuiNodesState_DraggingInput:
        case ImGuiNodesState_DraggingOutput:
        {
            if (active_input_ && active_output_)
            {
                IM_ASSERT(hovered_node);
                IM_ASSERT(active_node_);
                active_input_->source_node_ = state_ == ImGuiNodesState_DraggingInput ? hovered_node : active_node_;

                if (active_input_->source_output_)
                    active_input_->source_output_->connections_count_--;

                active_input_->source_output_ = active_output_;
                active_output_->connections_count_++;
                if (interaction_handler_)
                    interaction_handler_->OnConnectionCreated(active_output_->uid_, active_input_->uid_);
            }
            else
            {
                if (io.MouseDragMaxDistanceSqr[0] < (io.MouseDragThreshold * io.MouseDragThreshold))
                {
                    if (state_ == ImGuiNodesState_DraggingInput && active_input_)
                    {
                        if (io.KeyCtrl)
                            TOGGLE_FLAG(active_input_->state_, ImGuiNodesConnectorStateFlag_Selected);
                        else
                        {
                            ClearAllConnectorSelections();
                            for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                                CLEAR_FLAG(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected);
                            SET_FLAG(active_input_->state_, ImGuiNodesConnectorStateFlag_Selected);
                        }
                    }
                    else if (state_ == ImGuiNodesState_DraggingOutput && active_output_)
                    {
                        if (io.KeyCtrl)
                            TOGGLE_FLAG(active_output_->state_, ImGuiNodesConnectorStateFlag_Selected);
                        else
                        {
                            ClearAllConnectorSelections();
                            for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                                CLEAR_FLAG(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected);
                            SET_FLAG(active_output_->state_, ImGuiNodesConnectorStateFlag_Selected);
                        }
                    }
                }
            }

            active_dragging_connection_ = ImVec4();
            state_ = ImGuiNodesState_Default;
            return;
        }
        }

        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
        {
            ImGuiNodesNode* node = nodes_[node_idx];
            
            for (int input_idx = 0; input_idx < node->inputs_.size(); ++input_idx)
            {
                ImGuiNodesInput& input = node->inputs_[input_idx];
                if (IS_SET(input.state_, ImGuiNodesConnectorStateFlag_Selected))
                {
                    if (input.source_output_)
                        input.source_output_->connections_count_--;
                    
                    input.source_node_ = NULL;
                    input.source_output_ = NULL;
                }
                CLEAR_FLAG(input.state_, ImGuiNodesConnectorStateFlag_Selected);
            }
            
            for (int output_idx = 0; output_idx < node->outputs_.size(); ++output_idx)
                CLEAR_FLAG(node->outputs_[output_idx].state_, ImGuiNodesConnectorStateFlag_Selected);
        }

        ImVector<ImGuiNodesNode*> nodes;
        nodes.reserve(nodes_.size());

        for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
        {
            ImGuiNodesNode* node = nodes_[node_idx];
            IM_ASSERT(node);

            if (IS_SET(node->state_, ImGuiNodesNodeStateFlag_Selected))
            {
                active_node_ = NULL;
                active_input_ = NULL;
                active_output_ = NULL;

                state_ = ImGuiNodesState_Default;

                for (int sweep_idx = 0; sweep_idx < nodes_.size(); ++sweep_idx)
                {
                    ImGuiNodesNode* sweep = nodes_[sweep_idx];
                    IM_ASSERT(sweep);
                
                    for (int input_idx = 0; input_idx < sweep->inputs_.size(); ++input_idx)
                    {
                        ImGuiNodesInput& input = sweep->inputs_[input_idx];

                        if (node == input.source_node_)
                        {
                            if (input.source_output_)
                                input.source_output_->connections_count_--;

                            input.source_node_ = NULL;
                            input.source_output_ = NULL;
                        }
                    }
                }

                for (int input_idx = 0; input_idx < node->inputs_.size(); ++input_idx)
                {
                    ImGuiNodesInput& input = node->inputs_[input_idx];
                    
                    if (input.source_output_)
                        input.source_output_->connections_count_--;
                    
                    input.name_.clear();
                    input.source_node_ = NULL;
                    input.source_output_ = NULL;
                }

                for (int output_idx = 0; output_idx < node->outputs_.size(); ++output_idx)
                {
                    ImGuiNodesOutput& output = node->outputs_[output_idx];
                    IM_ASSERT(output.connections_count_ == 0);
                }
    
                delete node;
            }
            else
            {
                nodes.push_back(node);
            }			
        }

        nodes_ = nodes;

        return;
    }
}

void ImGuiNodes::ProcessNodes()
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    const ImVec2 offset = nodes_imgui_window_pos_ + scroll_;

    ImGui::SetWindowFontScale(scale_);

    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
    {
        const ImGuiNodesNode* node = nodes_[node_idx];
        IM_ASSERT(node);
        if (node->parent_node_)
        {
            ImVec2 head_offset(0.0f, node->area_name_.GetHeight() * 0.8f);
            RenderConnection(offset + (node->area_node_.GetTL() + head_offset) * scale_, offset + (node->parent_node_->area_node_.GetTR() + head_offset) * scale_, ImColor(0.0f, 1.0f, 0.0f, 0.05f), 10.0f);
        }
    }

    bool any_node_selected = false;
    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
    {
        const ImGuiNodesNode* node = nodes_[node_idx];
        IM_ASSERT(node);
        if (IS_SET(node->state_, ImGuiNodesNodeStateFlag_Selected))
        {
            any_node_selected = true;
            break;
        }
    }

    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
    {
        const ImGuiNodesNode* node = nodes_[node_idx];
        IM_ASSERT(node);

        for (int input_idx = 0; input_idx < node->inputs_.size(); ++input_idx)
        {		
            const ImGuiNodesInput& input = node->inputs_[input_idx];
            
            if (const ImGuiNodesNode* source_node = input.source_node_)
            {
                IM_ASSERT(source_node);

                ImVec2 p1 = offset;
                ImVec2 p4 = offset;

                if (IS_SET(node->state_, ImGuiNodesNodeStateFlag_Collapsed))
                {
                    ImVec2 collapsed_input = { 0, (node->area_node_.Max.y - node->area_node_.Min.y) * 0.5f };					

                    p1 += ((node->area_node_.Min + collapsed_input) * scale_);
                }
                else
                {
                    p1 += (input.pos_ * scale_);
                }

                if (IS_SET(source_node->state_, ImGuiNodesNodeStateFlag_Collapsed))
                {
                    ImVec2 collapsed_output = { 0, (source_node->area_node_.Max.y - source_node->area_node_.Min.y) * 0.5f };					
                    
                    p4 += ((source_node->area_node_.Max - collapsed_output) * scale_);
                }
                else
                {
                    p4 += (input.source_output_->pos_ * scale_);		
                }

                ImColor color = IS_SET(node->state_, ImGuiNodesNodeStateFlag_Collapsed)
                    ? ImColor(0.4f, 0.4f, 0.4f, 0.1f)
                    : (!any_node_selected || IS_SET(node->state_, ImGuiNodesNodeStateFlag_Selected) || IS_SET(source_node->state_, ImGuiNodesNodeStateFlag_Selected))
                    ? ImColor(1.0f, 1.0f, 1.0f, 1.0f)
                    : ImColor(0.4f, 0.4f, 0.4f, 0.5f);
                RenderConnection(p1, p4, color);
            }
        }
    }

    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
    {
        const ImGuiNodesNode* node = nodes_[node_idx];
        IM_ASSERT(node);
        node->Render(draw_list, offset, scale_, state_);
    }

    if (state_ == ImGuiNodesState_HoveringNode && 
        active_node_ && 
        IS_SET(active_node_->state_, ImGuiNodesNodeStateFlag_Warning) && 
        !active_node_->warning_message_.empty())
    {
        ImRect node_rect = active_node_->area_node_;
        node_rect.Min *= scale_;
        node_rect.Max *= scale_;
        node_rect.Translate(offset);
        ImRect header_area = ImRect(node_rect.GetTL(), node_rect.GetTR() + ImVec2(0.0f, active_node_->title_height_ * scale_));

        if (header_area.Contains(mouse_))
            ImGui::SetTooltip("%s", active_node_->warning_message_.c_str());
    }

    if (active_dragging_connection_.x != active_dragging_connection_.z && active_dragging_connection_.y != active_dragging_connection_.w)
        RenderConnection(ImVec2(active_dragging_connection_.x, active_dragging_connection_.y), ImVec2(active_dragging_connection_.z, active_dragging_connection_.w), ImColor(1.0f, 1.0f, 1.0f, 1.0f));

    ImGui::SetWindowFontScale(1.0f);

    if (state_ == ImGuiNodesState_Selecting)
    {
        draw_list->AddRectFilled(active_dragging_selection_area_.Min, active_dragging_selection_area_.Max, ImColor(1.0f, 1.0f, 0.0f, 0.1f));
        draw_list->AddRect(active_dragging_selection_area_.Min, active_dragging_selection_area_.Max, ImColor(1.0f, 1.0f, 0.0f, 0.5f));
    }

    ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
    ImGui::NewLine();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

    ImGui::NewLine();

    ImGui::Text("ImGui::IsAnyItemActive: %s", ImGui::IsAnyItemActive() ? "true" : "false");
    ImGui::Text("ImGui::IsAnyItemFocused: %s", ImGui::IsAnyItemFocused() ? "true" : "false");
    ImGui::Text("ImGui::IsAnyItemHovered: %s", ImGui::IsAnyItemHovered() ? "true" : "false");
    ImGui::Text("IsWindowHovered(AnyWindow): %s", ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ? "true" : "false");
    ImGui::Text("IsWindowHovered(): %s", ImGui::IsWindowHovered() ? "true" : "false");

    ImGui::NewLine();
    
    switch (state_)
    {
        case ImGuiNodesState_Default: ImGui::Text("ImGuiNodesState_Default"); break;
        case ImGuiNodesState_HoveringNode: ImGui::Text("ImGuiNodesState_HoveringNode"); break;
        case ImGuiNodesState_HoveringInput: ImGui::Text("ImGuiNodesState_HoveringInput"); break;
        case ImGuiNodesState_HoveringOutput: ImGui::Text("ImGuiNodesState_HoveringOutput"); break;
        case ImGuiNodesState_Dragging: ImGui::Text("ImGuiNodesState_Draging"); break;
        case ImGuiNodesState_DraggingInput: ImGui::Text("ImGuiNodesState_DragingInput"); break;
        case ImGuiNodesState_DraggingOutput: ImGui::Text("ImGuiNodesState_DragingOutput"); break;
        case ImGuiNodesState_Selecting: ImGui::Text("ImGuiNodesState_Selecting"); break;
        default: ImGui::Text("UNKNOWN"); break;
    }

    ImGui::NewLine();

    ImGui::Text("Window position: %.2f, %.2f", nodes_imgui_window_pos_.x, nodes_imgui_window_pos_.y);
    ImGui::Text("Window size: %.2f, %.2f", nodes_imgui_window_size_.x, nodes_imgui_window_size_.y);
    ImGui::Text("Mouse: %.2f, %.2f", mouse_.x, mouse_.y);
    ImGui::Text("Scroll: %.2f, %.2f", scroll_.x, scroll_.y);
    ImGui::Text("Scale: %.2f", scale_);

    ImGui::NewLine();
    
    if (active_node_)
        ImGui::Text("Active_node: %s", active_node_->name_.c_str());
    if (active_input_)
        ImGui::Text("Active_input: %s", active_input_->name_.c_str());
    if (active_output_)
        ImGui::Text("Active_output: %s", active_output_->name_.c_str());
}


void ImGuiNodes::ProcessContextMenu()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

    if (ImGui::BeginPopup("NodesContextMenu", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        ImVec2 position = (mouse_ - scroll_ - nodes_imgui_window_pos_) / scale_;
        if (interaction_handler_)
            interaction_handler_->RenderPopupMenu(this, position);
        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
}

void ImGuiNodesInput::TranslateInput(ImVec2 delta)
{
    pos_ += delta;
    area_input_.Translate(delta);
    area_name_.Translate(delta);
}

ImGuiNodesInput::ImGuiNodesInput(const ImGuiNodesIdentifier& name)
{
    state_ = ImGuiNodesConnectorStateFlag_Default;
    source_node_ = NULL;
    source_output_ = NULL;
    name_ = name.name_;
    uid_ = name.id_;

    area_name_.Min = ImVec2(0.0f, 0.0f);
    area_name_.Max = ImGui::CalcTextSize(name_.c_str());

    area_input_.Min = ImVec2(0.0f, 0.0f);
    area_input_.Max.x = ImGuiNodesConnectorDotPadding + ImGuiNodesConnectorDotDiameter + ImGuiNodesConnectorDotPadding;
    area_input_.Max.y = ImGuiNodesConnectorDistance;
    area_input_.Max *= area_name_.GetHeight();

    ImVec2 offset = ImVec2(0.0f, 0.0f) - area_input_.GetCenter();

    area_name_.Translate(ImVec2(area_input_.GetWidth(), (area_input_.GetHeight() - area_name_.GetHeight()) * 0.5f));

    area_input_.Max.x += area_name_.GetWidth();
    area_input_.Max.x += ImGuiNodesConnectorDotPadding * area_name_.GetHeight();

    area_input_.Translate(offset);
    area_name_.Translate(offset);
}

void ImGuiNodesInput::Render(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const
{
    if (state != ImGuiNodesState_Dragging && IS_SET(state_, ImGuiNodesConnectorStateFlag_Hovered) && !IS_SET(state_, ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget))
    {
        const ImColor color = source_node_ == NULL ? ImColor(0.0f, 0.0f, 1.0f, 0.5f) : ImColor(1.0f, 0.5f, 0.0f, 0.5f);
        draw_list->AddRectFilled((area_input_.Min * scale) + offset, (area_input_.Max * scale) + offset, color);
    }

    if (HAS_ANY_FLAG(state_, ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget | ImGuiNodesConnectorStateFlag_Dragging))
        draw_list->AddRectFilled((area_input_.Min * scale) + offset, (area_input_.Max * scale) + offset, ImColor(0.0f, 1.0f, 0.0f, 0.5f));

    if (IS_SET(state_, ImGuiNodesConnectorStateFlag_Selected))
        draw_list->AddRect((area_input_.Min * scale) + offset, (area_input_.Max * scale) + offset, ImColor(1.0f, 1.0f, 1.0f, 1.0f), 0.0f, 0, 2.0f * scale);

    bool consider_fill = false;
    consider_fill |= IS_SET(state_, ImGuiNodesConnectorStateFlag_Dragging);
    consider_fill |= HAS_ALL_FLAGS(state_, ImGuiNodesConnectorStateFlag_Hovered | ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget);
    consider_fill |= bool(source_node_);
    if (consider_fill)
        draw_list->AddCircleFilled((pos_ * scale) + offset, (ImGuiNodesConnectorDotDiameter * 0.5f) * area_name_.GetHeight() * scale, ImColor(1.0f, 1.0f, 1.0f, 1.0f));
    else
        draw_list->AddCircle((pos_ * scale) + offset, (ImGuiNodesConnectorDotDiameter * 0.5f) * area_name_.GetHeight() * scale, ImColor(1.0f, 1.0f, 1.0f, 1.0f));

    ImGui::SetCursorScreenPos((area_name_.Min * scale) + offset);
    ImGui::Text("%s", name_.c_str());
}

void ImGuiNodesOutput::TranslateOutput(ImVec2 delta)
{
    pos_ += delta;
    area_output_.Translate(delta);
    area_name_.Translate(delta);
}

ImGuiNodesOutput::ImGuiNodesOutput(const ImGuiNodesIdentifier& name)
{
    state_ = ImGuiNodesConnectorStateFlag_Default;
    connections_count_ = 0;
    name_ = name.name_;
    uid_ = name.id_;

    area_name_.Min = ImVec2(0.0f, 0.0f) - ImGui::CalcTextSize(name_.c_str());
    area_name_.Max = ImVec2(0.0f, 0.0f);

    area_output_.Min.x = ImGuiNodesConnectorDotPadding + ImGuiNodesConnectorDotDiameter + ImGuiNodesConnectorDotPadding;
    area_output_.Min.y = ImGuiNodesConnectorDistance;
    area_output_.Min *= -area_name_.GetHeight();
    area_output_.Max = ImVec2(0.0f, 0.0f);

    ImVec2 offset = ImVec2(0.0f, 0.0f) - area_output_.GetCenter();

    area_name_.Translate(ImVec2(area_output_.Min.x, (area_output_.GetHeight() - area_name_.GetHeight()) * -0.5f));

    area_output_.Min.x -= area_name_.GetWidth();
    area_output_.Min.x -= ImGuiNodesConnectorDotPadding * area_name_.GetHeight();

    area_output_.Translate(offset);
    area_name_.Translate(offset);
}

void ImGuiNodesOutput::Render(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const
{
    if (state != ImGuiNodesState_Dragging && IS_SET(state_, ImGuiNodesConnectorStateFlag_Hovered) && !IS_SET(state_, ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget))
        draw_list->AddRectFilled((area_output_.Min * scale) + offset, (area_output_.Max * scale) + offset, ImColor(0.0f, 0.0f, 1.0f, 0.5f));

    if (HAS_ANY_FLAG(state_, ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget | ImGuiNodesConnectorStateFlag_Dragging))
        draw_list->AddRectFilled((area_output_.Min * scale) + offset, (area_output_.Max * scale) + offset, ImColor(0.0f, 1.0f, 0.0f, 0.5f));

    if (IS_SET(state_, ImGuiNodesConnectorStateFlag_Selected))
        draw_list->AddRect((area_output_.Min * scale) + offset, (area_output_.Max * scale) + offset, ImColor(1.0f, 1.0f, 1.0f, 1.0f), 0.0f, 0, 2.0f * scale);

    bool consider_fill = false;
    consider_fill |= IS_SET(state_, ImGuiNodesConnectorStateFlag_Dragging);
    consider_fill |= HAS_ALL_FLAGS(state_, ImGuiNodesConnectorStateFlag_Hovered | ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget);
    consider_fill |= bool(connections_count_ > 0);
    if (consider_fill)
        draw_list->AddCircleFilled((pos_ * scale) + offset, (ImGuiNodesConnectorDotDiameter * 0.5f) * area_name_.GetHeight() * scale, ImColor(1.0f, 1.0f, 1.0f, 1.0f));
    else
        draw_list->AddCircle((pos_ * scale) + offset, (ImGuiNodesConnectorDotDiameter * 0.5f) * area_name_.GetHeight() * scale, ImColor(1.0f, 1.0f, 1.0f, 1.0f));

    ImGui::SetCursorScreenPos((area_name_.Min * scale) + offset);
    ImGui::Text("%s", name_.c_str());
}

void ImGuiNodesNode::TranslateNode(ImVec2 delta, bool selected_only)
{
    if (selected_only && !IS_SET(state_, ImGuiNodesNodeStateFlag_Selected))
        return;

    area_node_.Translate(delta);
    area_name_.Translate(delta);

    for (int input_idx = 0; input_idx < inputs_.size(); ++input_idx)
        inputs_[input_idx].TranslateInput(delta);

    for (int output_idx = 0; output_idx < outputs_.size(); ++output_idx)
        outputs_[output_idx].TranslateOutput(delta);
}

ImGuiNodesNode::ImGuiNodesNode(const ImGuiNodesIdentifier& name, ImColor color)
{
    name_ = name.name_;
    uid_ = name.id_;
    state_ = ImGuiNodesNodeStateFlag_Default;
    color_ = color;

    area_name_.Min = ImVec2(0.0f, 0.0f);
    area_name_.Max = ImGui::CalcTextSize(name_.c_str());
    title_height_ = ImGuiNodesTitleHight * area_name_.GetHeight();
}

void ImGuiNodesNode::BuildNodeGeometry(ImVec2 inputs_size, ImVec2 outputs_size)
{
    body_height_ = ImMax(inputs_size.y, outputs_size.y) + (ImGuiNodesVSeparation * area_name_.GetHeight());

    area_node_.Min = ImVec2(0.0f, 0.0f);
    area_node_.Max = ImVec2(0.0f, 0.0f);
    area_node_.Max.x += inputs_size.x + outputs_size.x;
    area_node_.Max.x += ImGuiNodesHSeparation * area_name_.GetHeight();
    area_node_.Max.x = ImMax(area_node_.Max.x, area_name_.GetWidth() + (ImGuiNodesHSeparation * area_name_.GetHeight()));
    area_node_.Max.y += title_height_ + body_height_;

    area_name_.Translate(ImVec2((area_node_.GetWidth() - area_name_.GetWidth()) * 0.5f, ((title_height_ - area_name_.GetHeight()) * 0.5f)));

    ImVec2 inputs = area_node_.GetTL();
    inputs.y += title_height_ + (ImGuiNodesVSeparation * area_name_.GetHeight() * 0.5f);
    for (int input_idx = 0; input_idx < inputs_.size(); ++input_idx)
    {
        inputs_[input_idx].TranslateInput(inputs - inputs_[input_idx].area_input_.GetTL());
        inputs.y += inputs_[input_idx].area_input_.GetHeight();
    }

    ImVec2 outputs = area_node_.GetTR();
    outputs.y += title_height_ + (ImGuiNodesVSeparation * area_name_.GetHeight() * 0.5f);
    for (int output_idx = 0; output_idx < outputs_.size(); ++output_idx)
    {
        outputs_[output_idx].TranslateOutput(outputs - outputs_[output_idx].area_output_.GetTR());
        outputs.y += outputs_[output_idx].area_output_.GetHeight();
    }
}

void ImGuiNodesNode::Render(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const
{
    if (!IS_SET(state_, ImGuiNodesNodeStateFlag_Visible))
        return;

    ImRect node_rect = area_node_;
    node_rect.Min *= scale;
    node_rect.Max *= scale;
    node_rect.Translate(offset);

    float rounding = title_height_ * scale * 0.3f;
    rounding = 0;

    ImColor head_color = color_, body_color = color_;
    head_color.Value.x *= 0.6;
    head_color.Value.y *= 0.6;
    head_color.Value.z *= 0.6;
    head_color.Value.w = 1.00f;

    if (IS_SET(state_, ImGuiNodesNodeStateFlag_Warning))
        head_color = ImColor(0.8f, 0.4f, 0.1f, 1.0f);

    body_color.Value.w = 0.75f;		

    const ImVec2 outline(4.0f * scale, 4.0f * scale);

    const ImDrawFlags rounding_corners_flags = ImDrawFlags_RoundCornersAll;

    if (IS_SET(state_, ImGuiNodesNodeStateFlag_Disabled))
    {
        body_color.Value.w = 0.25f;

        if (IS_SET(state_, ImGuiNodesNodeStateFlag_Collapsed))
            head_color.Value.w = 0.25f;
    }

    draw_list->AddRectFilled(node_rect.Min, node_rect.Max, body_color, rounding, rounding_corners_flags);

    const ImVec2 head = node_rect.GetTR() + ImVec2(0.0f, title_height_ * scale);

    if (!IS_SET(state_, ImGuiNodesNodeStateFlag_Collapsed))
        draw_list->AddLine(ImVec2(node_rect.Min.x, head.y), ImVec2(head.x - 1.0f, head.y), ImColor(0.0f, 0.0f, 0.0f, 0.5f), 2.0f);

    const ImDrawFlags head_corners_flags = IS_SET(state_, ImGuiNodesNodeStateFlag_Collapsed) ? rounding_corners_flags : ImDrawFlags_RoundCornersTop;
    draw_list->AddRectFilled(node_rect.Min, head, head_color, rounding, head_corners_flags);	

    if (IS_SET(state_, ImGuiNodesNodeStateFlag_Disabled))
    {
        IM_ASSERT(!node_rect.IsInverted());

        const float separation = 15.0f * scale;

        for (float line = separation; ; line += separation)
        {
            ImVec2 start = node_rect.Min + ImVec2(0.0f, line);
            ImVec2 stop = node_rect.Min + ImVec2(line, 0.0f);

            if (start.y > node_rect.Max.y)
                start = ImVec2(start.x + (start.y - node_rect.Max.y), node_rect.Max.y);

            if (stop.x > node_rect.Max.x)
                stop = ImVec2(node_rect.Max.x, stop.y + (stop.x - node_rect.Max.x));

            if (start.x > node_rect.Max.x)
                break;

            if (stop.y > node_rect.Max.y)
                break;

            draw_list->AddLine(start, stop, body_color, 3.0f * scale);
        }
    }

    if (!IS_SET(state_, ImGuiNodesNodeStateFlag_Collapsed))
    {
        for (int input_idx = 0; input_idx < inputs_.size(); ++input_idx)
            inputs_[input_idx].Render(draw_list, offset, scale, state);

        for (int output_idx = 0; output_idx < outputs_.size(); ++output_idx)
            outputs_[output_idx].Render(draw_list, offset, scale, state);
    }

    ImGui::SetCursorScreenPos((area_name_.Min * scale) + offset);
    ImGui::Text("%s", name_.c_str());

    if (HAS_ANY_FLAG(state_, ImGuiNodesNodeStateFlag_MarkedForSelection | ImGuiNodesNodeStateFlag_Selected))
    {
        // Create a subtle highlighted border color based on the node's color
        ImColor border_color = color_;
        border_color.Value.x *= 1.3f;  // Slightly brighten
        border_color.Value.y *= 1.3f;
        border_color.Value.z *= 1.3f;
        border_color.Value.w = 0.8f;   // Semi-transparent
        
        draw_list->AddRect(node_rect.Min - outline, node_rect.Max + outline, border_color, rounding, rounding_corners_flags, 2.0f * scale);
    }

    // Default border for all nodes
    draw_list->AddRect(node_rect.Min - outline * 0.5f, node_rect.Max + outline * 0.5f, ImColor(0.0f, 0.0f, 0.0f, 0.5f), rounding, rounding_corners_flags, 3.0f * scale);
}

void ImGuiNodes::RenderConnection(ImVec2 p1, ImVec2 p4, ImColor color, float thickness)
{		
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    float x_offset = 100.0f;
    float y_offset = (p4.y - p1.y) * 0.05f;

    ImVec2 p2 = p1;
    ImVec2 p3 = p4;

    p2 += (ImVec2(-x_offset, -y_offset) * scale_);
    p3 += (ImVec2(+x_offset, +y_offset) * scale_);

    if (p4.x > p1.x)
    {
        float dist = p4.x - p1.x;
        p2 += (ImVec2(-x_offset, 0.0f) * dist / 500.0f);
        p3 += (ImVec2(+x_offset, 0.0f) * dist / 500.0f);
    }

    // draw_list->AddLine(p1, p4, color, 1.5f * scale_);
    draw_list->AddBezierCubic(p1, p2, p3, p4, color, thickness * scale_);
    // draw_list->AddCircle(p2, 3.0f * scale_, color);
    // draw_list->AddCircle(p3, 3.0f * scale_, color);
}

ImGuiNodes::ImGuiNodes(ImGuiNodesInteractionHandler* interaction_handler)
{
    scale_ = 1.0f;
    state_ = ImGuiNodesState_Default;
    active_node_ = NULL;
    active_input_ = NULL;
    active_output_ = NULL;

    interaction_handler_ = interaction_handler;
}

ImGuiNodes::~ImGuiNodes()
{
    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
        delete nodes_[node_idx];
}

void ImGuiNodes::SetWarning(const ImGuiNodesUid& uid, const std::string& message)
{
    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
    {
        ImGuiNodesNode* node = nodes_[node_idx];
        if (node->uid_ == uid)
        {
            SET_FLAG(node->state_, ImGuiNodesNodeStateFlag_Warning);
            node->warning_message_ = message;
            break;
        }
    }
}

void ImGuiNodes::SetOk(const ImGuiNodesUid& uid)
{
    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
    {
        ImGuiNodesNode* node = nodes_[node_idx];
        if (node->uid_ == uid)
        {
            CLEAR_FLAG(node->state_, ImGuiNodesNodeStateFlag_Warning);
            node->warning_message_.clear();
            break;
        }
    }
}

void ImGuiNodes::ClearAllConnectorSelections()
{
    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
    {
        ImGuiNodesNode* node = nodes_[node_idx];
        
        for (int input_idx = 0; input_idx < node->inputs_.size(); ++input_idx)
            CLEAR_FLAG(node->inputs_[input_idx].state_, ImGuiNodesConnectorStateFlag_Selected);
            
        for (int output_idx = 0; output_idx < node->outputs_.size(); ++output_idx)
            CLEAR_FLAG(node->outputs_[output_idx].state_, ImGuiNodesConnectorStateFlag_Selected);
    }
}

#undef IS_SET
#undef SET_FLAG
#undef CLEAR_FLAG
#undef TOGGLE_FLAG
#undef HAS_ALL_FLAGS
#undef HAS_ANY_FLAG
#undef CLEAR_FLAGS
