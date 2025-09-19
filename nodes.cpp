#include "nodes.h"

using namespace ImGui;

#define IS_SET(state, flag)     ((state) & (flag))
#define SET_FLAG(state, flag)   ((state) |= (flag))
#define CLEAR_FLAG(state, flag) ((state) &= ~(flag))
#define TOGGLE_FLAG(state, flag) ((state) ^= (flag))
#define HAS_ALL_FLAGS(state, flags) (((state) & (flags)) == (flags))
#define HAS_ANY_FLAG(state, flags) ((state) & (flags))
#define CLEAR_FLAGS(state, flags) ((state) &= ~(flags))


void ImGuiNodes::UpdateCanvasGeometry(ImDrawList* draw_list)
{
    const ImGuiIO& io = ImGui::GetIO();

    mouse_ = ImGui::GetMousePos();

    {
        ImVec2 min = ImGui::GetWindowContentRegionMin();
        ImVec2 max = ImGui::GetWindowContentRegionMax();

        pos_ = ImGui::GetWindowPos() + min;
        size_ = max - min;	
    }

    ImRect canvas(pos_, pos_ + size_);

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_0))
    {
        scroll_ = {};
        scale_ = 1.0f;
    }
        
    if (!ImGui::IsMouseDown(0) && canvas.Contains(mouse_))
    {
        if (ImGui::IsMouseDragging(1))
            scroll_ += io.MouseDelta;

        if (!io.KeyShift && !io.KeyCtrl)
        {
            ImVec2 focus = (mouse_ - scroll_ - pos_) / scale_;

            if (io.MouseWheel < 0.0f)
                for (float zoom = io.MouseWheel; zoom < 0.0f; zoom += 1.0f)
                    scale_ = ImMax(0.3f, scale_ / 1.05f);

            if (io.MouseWheel > 0.0f)
                for (float zoom = io.MouseWheel; zoom > 0.0f; zoom -= 1.0f)
                    scale_ = ImMin(3.0f, scale_ * 1.05f);

            ImVec2 shift = scroll_ + (focus * scale_);
            scroll_ += mouse_ - shift - pos_;
        }

        if (ImGui::IsMouseReleased(1) && !element_node_)
        {
            if (io.MouseDragMaxDistanceSqr[1] < (io.MouseDragThreshold * io.MouseDragThreshold))
            {
                bool selected = false;
                for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                {
                    if (IS_SET(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected))
                    {					
                        selected = true;
                        break;
                    }
                }
                if (!selected)
                    ImGui::OpenPopup("NodesContextMenu");
            }
        }
    }

    const float grid = 64.0f * scale_;
    int mark_x = (int)(scroll_.x / grid);
    int mark_y = (int)(scroll_.y / grid);
    for (float x = fmodf(scroll_.x, grid); x < size_.x; x += grid, --mark_x)
    {		
        for (float y = fmodf(scroll_.y, grid); y < size_.y; y += grid, --mark_y)
        {
            ImColor color = (mark_y % 5) || (mark_x % 5) ? ImColor(0.5f, 0.5f, 0.5f, 0.2f) : ImColor(1.0f, 1.0f, 1.0f, 0.2f);
            draw_list->AddCircleFilled(ImVec2(x, y) + pos_, 1.5f * scale_, color);
        }
    }
}

ImGuiNodesNode* ImGuiNodes::UpdateNodesFromCanvas()
{
    if (nodes_.empty())
        return NULL;

    const ImGuiIO& io = ImGui::GetIO();

    ImVec2 offset = pos_ + scroll_;
    ImRect canvas(pos_, pos_ + size_);
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
            node->state_ &= ~(ImGuiNodesNodeStateFlag_Visible | ImGuiNodesNodeStateFlag_Hovered | ImGuiNodesNodeStateFlag_MarkedForSelection);
            continue;
        }

        if (!hovered_node && node_rect.Contains(mouse_))
            hovered_node = node;

        if (state_ == ImGuiNodesState_Selecting)
        {
            if (io.KeyCtrl && area_.Overlaps(node_rect))
            {
                SET_FLAG(node->state_, ImGuiNodesNodeStateFlag_MarkedForSelection);
                continue;
            }

            if (!io.KeyCtrl && area_.Overlaps(node_rect))
            {
                SET_FLAG(node->state_, ImGuiNodesNodeStateFlag_MarkedForSelection);
                continue;
            }

            CLEAR_FLAG(node->state_, ImGuiNodesNodeStateFlag_MarkedForSelection);
        }

        for (int input_idx = 0; input_idx < node->inputs_.size(); ++input_idx)
        {
            ImGuiNodesInput& input = node->inputs_[input_idx];

            CLEAR_FLAGS(input.state_, ImGuiNodesConnectorStateFlag_Hovered | ImGuiNodesConnectorStateFlag_Consider | ImGuiNodesConnectorStateFlag_Draging);

            if (state_ == ImGuiNodesState_DragingInput)
            {
                if (&input == element_input_)
                    SET_FLAG(input.state_, ImGuiNodesConnectorStateFlag_Draging);

                continue;
            }				

            if (state_ == ImGuiNodesState_DragingOutput)
            {
                if (element_node_ == node)
                    continue;

                if (ConnectionMatrix(node, element_node_, &input, element_output_))
                    SET_FLAG(input.state_, ImGuiNodesConnectorStateFlag_Consider);
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
                if (state_ != ImGuiNodesState_DragingOutput)
                {
                    SET_FLAG(input.state_, ImGuiNodesConnectorStateFlag_Hovered);
                    continue;
                }
                
                if (IS_SET(input.state_, ImGuiNodesConnectorStateFlag_Consider))
                    SET_FLAG(input.state_, ImGuiNodesConnectorStateFlag_Hovered);
            }				
        }

        for (int output_idx = 0; output_idx < node->outputs_.size(); ++output_idx)
        {
            ImGuiNodesOutput& output = node->outputs_[output_idx];

            CLEAR_FLAGS(output.state_, ImGuiNodesConnectorStateFlag_Hovered | ImGuiNodesConnectorStateFlag_Consider | ImGuiNodesConnectorStateFlag_Draging);

            if (state_ == ImGuiNodesState_DragingOutput)
            {
                if (&output == element_output_)
                    SET_FLAG(output.state_, ImGuiNodesConnectorStateFlag_Draging);

                continue;
            }

            if (state_ == ImGuiNodesState_DragingInput)
            {
                if (element_node_ == node)
                    continue;

                if (ConnectionMatrix(element_node_, node, element_input_, &output))
                    SET_FLAG(output.state_, ImGuiNodesConnectorStateFlag_Consider);
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
                if (state_ != ImGuiNodesState_DragingInput)
                {
                    SET_FLAG(output.state_, ImGuiNodesConnectorStateFlag_Hovered);
                    continue;
                }

                if (IS_SET(output.state_, ImGuiNodesConnectorStateFlag_Consider))
                    SET_FLAG(output.state_, ImGuiNodesConnectorStateFlag_Hovered);
            }
        }	
    }

    if (hovered_node)
        SET_FLAG(hovered_node->state_, ImGuiNodesNodeStateFlag_Hovered);

    return hovered_node;
}

ImGuiNodesNode* ImGuiNodes::CreateNode(const std::string& name, ImColor color, ImVec2 pos, 
                                       const std::vector<std::string>& inputs, const std::vector<std::string>& outputs)
{
    ImGuiNodesNode* node = new ImGuiNodesNode(name, color);

    for (const auto& input_name : inputs)
        node->inputs_.push_back(ImGuiNodesInput(input_name));
    
    for (const auto& output_name : outputs)
        node->outputs_.push_back(ImGuiNodesOutput(output_name));

    
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

    return node;
}

bool ImGuiNodes::SortSelectedNodesOrder()
{
    bool selected = false;

    ImVector<ImGuiNodesNode*> nodes_unselected;
    nodes_unselected.reserve(nodes_.size());

    ImVector<ImGuiNodesNode*> nodes_selected;
    nodes_selected.reserve(nodes_.size());

    for (ImGuiNodesNode** iterator = nodes_.begin(); iterator != nodes_.end(); ++iterator)
    {
        ImGuiNodesNode* node = ((ImGuiNodesNode*)*iterator);

        if (IS_SET(node->state_, ImGuiNodesNodeStateFlag_MarkedForSelection) || IS_SET(node->state_, ImGuiNodesNodeStateFlag_Selected))
        {
            selected = true;
            CLEAR_FLAG(node->state_, ImGuiNodesNodeStateFlag_MarkedForSelection);
            SET_FLAG(node->state_, ImGuiNodesNodeStateFlag_Selected);
            nodes_selected.push_back(node);
        }
        else
            nodes_unselected.push_back(node);
    }

    int node_idx = 0;

    for (int unselected_idx = 0; unselected_idx < nodes_unselected.size(); ++unselected_idx)
        nodes_[node_idx++] = nodes_unselected[unselected_idx];

    for (int selected_idx = 0; selected_idx < nodes_selected.size(); ++selected_idx)
        nodes_[node_idx++] = nodes_selected[selected_idx];

    return selected;
}

void ImGuiNodes::Update()
{
    const ImGuiIO& io = ImGui::GetIO();
    
    UpdateCanvasGeometry(ImGui::GetWindowDrawList());

    ImGuiNodesNode* hovered_node = UpdateNodesFromCanvas();

    bool consider_hover = state_ == ImGuiNodesState_Default;
    consider_hover |= state_ == ImGuiNodesState_HoveringNode;
    consider_hover |= state_ == ImGuiNodesState_HoveringInput;
    consider_hover |= state_ == ImGuiNodesState_HoveringOutput;

    if (hovered_node && consider_hover)
    {
        element_input_ = NULL;
        element_output_ = NULL;

        for (int input_idx = 0; input_idx < hovered_node->inputs_.size(); ++input_idx)
        {
            if (IS_SET(hovered_node->inputs_[input_idx].state_, ImGuiNodesConnectorStateFlag_Hovered))
            {
                element_input_ = &hovered_node->inputs_[input_idx];
                state_ = ImGuiNodesState_HoveringInput;
                break;
            }
        }						
        
        for (int output_idx = 0; output_idx < hovered_node->outputs_.size(); ++output_idx)
        {
            if (IS_SET(hovered_node->outputs_[output_idx].state_, ImGuiNodesConnectorStateFlag_Hovered))
            {
                element_output_ = &hovered_node->outputs_[output_idx];
                state_ = ImGuiNodesState_HoveringOutput;
                break;
            }
        }					

        if (!element_input_ && !element_output_)
            state_ = ImGuiNodesState_HoveringNode;	
    }

    if (state_ == ImGuiNodesState_DragingInput)
    {
        element_output_ = NULL;
    
        if (hovered_node)
            for (int output_idx = 0; output_idx < hovered_node->outputs_.size(); ++output_idx)
            {
                ImGuiNodesConnectorState state = hovered_node->outputs_[output_idx].state_;

                if (HAS_ALL_FLAGS(state, ImGuiNodesConnectorStateFlag_Hovered | ImGuiNodesConnectorStateFlag_Consider))
                    element_output_ = &hovered_node->outputs_[output_idx];
            }
    }
    
    if (state_ == ImGuiNodesState_DragingOutput)
    {
        element_input_ = NULL;
    
        if (hovered_node)
            for (int input_idx = 0; input_idx < hovered_node->inputs_.size(); ++input_idx)
            {
                ImGuiNodesConnectorState state = hovered_node->inputs_[input_idx].state_;

                if (HAS_ALL_FLAGS(state, ImGuiNodesConnectorStateFlag_Hovered | ImGuiNodesConnectorStateFlag_Consider))
                    element_input_ = &hovered_node->inputs_[input_idx];
            }
    }

    if (consider_hover)
    {
        element_node_ = hovered_node;

        if (!hovered_node)
            state_ = ImGuiNodesState_Default;
    }					

    if (ImGui::IsMouseDoubleClicked(0))
    {
        switch (state_)
        {
            case ImGuiNodesState_Default:
            {
                bool selected = false;

                for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                {
                    ImGuiNodesState& state = nodes_[node_idx]->state_;

                    if (IS_SET(state, ImGuiNodesNodeStateFlag_Selected))
                        selected = true;

                    CLEAR_FLAGS(state, ImGuiNodesNodeStateFlag_Selected | ImGuiNodesNodeStateFlag_MarkedForSelection | ImGuiNodesNodeStateFlag_Hovered);
                }

                return;
            };

            case ImGuiNodesState_HoveringInput:
            {
                if (element_input_->source_node_)
                {
                    element_input_->source_output_->connections_count_--;
                    element_input_->source_output_ = NULL;
                    element_input_->source_node_ = NULL;

                    state_ = ImGuiNodesState_DragingInput;
                }

                return;
            }

            case ImGuiNodesState_HoveringNode:
            {
                IM_ASSERT(element_node_);                if (IS_SET(element_node_->state_, ImGuiNodesNodeStateFlag_Collapsed))
                {
                    CLEAR_FLAG(element_node_->state_, ImGuiNodesNodeStateFlag_Collapsed);
                    element_node_->area_node_.Max.y += element_node_->body_height_;
                    element_node_->TranslateNode(ImVec2(0.0f, element_node_->body_height_ * -0.5f));
                }
                else
                {
                    SET_FLAG(element_node_->state_, ImGuiNodesNodeStateFlag_Collapsed);
                    element_node_->area_node_.Max.y -= element_node_->body_height_;

                    //const ImVec2 click = (mouse_ - scroll_ - pos_) / scale_;
                    //const ImVec2 position = click - element_node_->area_node_.GetCenter();

                    element_node_->TranslateNode(ImVec2(0.0f, element_node_->body_height_ * 0.5f));
                }

                state_ = ImGuiNodesState_Draging;
                return;
            }
        }
    }

    if (ImGui::IsMouseDoubleClicked(1))
    {
        switch (state_)
        {
            case ImGuiNodesState_HoveringNode:
            {
                IM_ASSERT(hovered_node);

                if (IS_SET(hovered_node->state_, ImGuiNodesNodeStateFlag_Disabled))
                    hovered_node->state_ &= ~(ImGuiNodesNodeStateFlag_Disabled);
                else
                    hovered_node->state_ |= (ImGuiNodesNodeStateFlag_Disabled);

                return;
            }
        }
    }

    if (ImGui::IsMouseClicked(0))
    {
        switch (state_)
        {				
            case ImGuiNodesState_HoveringNode:
            {
                if (io.KeyCtrl)
                    TOGGLE_FLAG(element_node_->state_, ImGuiNodesNodeStateFlag_Selected);

                if (io.KeyShift)
                    SET_FLAG(element_node_->state_, ImGuiNodesNodeStateFlag_Selected);

                bool selected = IS_SET(element_node_->state_, ImGuiNodesNodeStateFlag_Selected);
                if (!selected)
                {
                    if (!io.KeyCtrl && !io.KeyShift)
                    {
                        for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                            CLEAR_FLAG(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected);

                        ClearAllConnectorSelections();
                    }
                    
                    SET_FLAG(element_node_->state_, ImGuiNodesNodeStateFlag_Selected);
                }
                // Note: We don't clear other selections when clicking on an already selected node
                // This allows dragging multiple selected nodes. If the user wants to deselect others,
                // they can use Ctrl+click or click in empty space first.

                SortSelectedNodesOrder();

                state_ = ImGuiNodesState_Draging;
                return;
            }

            case ImGuiNodesState_HoveringInput:
            {
                if (!element_input_->source_node_)
                    state_ = ImGuiNodesState_DragingInput;
                else
                    state_ = ImGuiNodesState_Draging;

                return;
            }

            case ImGuiNodesState_HoveringOutput:
            {
                state_ = ImGuiNodesState_DragingOutput;
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
        switch (state_)
        {
            case ImGuiNodesState_Default:
            {
                ImRect canvas(pos_, pos_ + size_);
                if (!canvas.Contains(mouse_))
                    return;

                if (!io.KeyShift)
                {
                    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                        nodes_[node_idx]->state_ &= ~(ImGuiNodesNodeStateFlag_Selected | ImGuiNodesNodeStateFlag_MarkedForSelection);
                    
                    ClearAllConnectorSelections();
                }

                state_ = ImGuiNodesState_Selecting;
                return;
            }

            case ImGuiNodesState_Selecting:
            {
                const ImVec2 pos = mouse_ - ImGui::GetMouseDragDelta(0);

                area_.Min = ImMin(pos, mouse_);
                area_.Max = ImMax(pos, mouse_);

                return;
            }

            case ImGuiNodesState_Draging:
            {
                if (element_input_ && element_input_->source_output_ && element_input_->source_output_->connections_count_ > 0)
                {
                    element_node_ = element_input_->source_node_;
                    element_output_ = element_input_->source_output_;

                    element_input_->source_output_->connections_count_--;
                    element_input_->source_output_ = NULL;
                    element_input_->source_node_ = NULL;

                    state_ = ImGuiNodesState_DragingOutput;
                    return;
                }

                if (!IS_SET(element_node_->state_, ImGuiNodesNodeStateFlag_Selected))
                    element_node_->TranslateNode(io.MouseDelta / scale_, false);
                else
                    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                        nodes_[node_idx]->TranslateNode(io.MouseDelta / scale_, true);

                return;
            }

            case ImGuiNodesState_DragingInput:
            {
                ImVec2 offset = pos_ + scroll_;
                ImVec2 p1 = offset + (element_input_->pos_ * scale_);
                ImVec2 p4 = element_output_ ? (offset + (element_output_->pos_ * scale_)) : mouse_;

                connection_ = ImVec4(p1.x, p1.y, p4.x, p4.y);
                return;
            }

            case ImGuiNodesState_DragingOutput:
            {
                ImVec2 offset = pos_ + scroll_;
                ImVec2 p1 = offset + (element_output_->pos_ * scale_);
                ImVec2 p4 = element_input_ ? (offset + (element_input_->pos_ * scale_)) : mouse_;

                connection_ = ImVec4(p4.x, p4.y, p1.x, p1.y);
                return;
            }
        }

        return;
    }

    if (ImGui::IsMouseReleased(0))
    {
        switch (state_)
        {
        case ImGuiNodesState_Selecting:
        {
            element_node_ = NULL;
            element_input_ = NULL;
            element_output_ = NULL;

            area_ = {};

            SortSelectedNodesOrder();
            state_ = ImGuiNodesState_Default;
            return;
        }

        case ImGuiNodesState_Draging:
        {
            // Check if this was a click on an input/output without much dragging
            if (io.MouseDragMaxDistanceSqr[0] < (io.MouseDragThreshold * io.MouseDragThreshold))
            {
                if (element_input_)
                {
                    if (io.KeyCtrl)
                    {
                        TOGGLE_FLAG(element_input_->state_, ImGuiNodesConnectorStateFlag_Selected);
                    }
                    else
                    {
                        ClearAllConnectorSelections();
                        for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                            CLEAR_FLAG(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected);
                        SET_FLAG(element_input_->state_, ImGuiNodesConnectorStateFlag_Selected);
                    }
                    state_ = ImGuiNodesState_HoveringInput;
                }
                else if (element_output_)
                {
                    if (io.KeyCtrl)
                    {
                        TOGGLE_FLAG(element_output_->state_, ImGuiNodesConnectorStateFlag_Selected);
                    }
                    else
                    {
                        ClearAllConnectorSelections();
                        for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                            CLEAR_FLAG(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected);
                        SET_FLAG(element_output_->state_, ImGuiNodesConnectorStateFlag_Selected);
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
                if (element_input_)
                    state_ = ImGuiNodesState_HoveringInput;
                else if (element_output_)
                    state_ = ImGuiNodesState_HoveringOutput;
                else
                    state_ = ImGuiNodesState_HoveringNode;
            }
            return;
        }

        case ImGuiNodesState_DragingInput:
        case ImGuiNodesState_DragingOutput:
        {
            if (element_input_ && element_output_)
            {
                IM_ASSERT(hovered_node);
                IM_ASSERT(element_node_);
                element_input_->source_node_ = state_ == ImGuiNodesState_DragingInput ? hovered_node : element_node_;

                if (element_input_->source_output_)
                    element_input_->source_output_->connections_count_--;

                element_input_->source_output_ = element_output_;
                element_output_->connections_count_++;
            }
            else
            {
                if (io.MouseDragMaxDistanceSqr[0] < (io.MouseDragThreshold * io.MouseDragThreshold))
                {
                    if (state_ == ImGuiNodesState_DragingInput && element_input_)
                    {
                        if (io.KeyCtrl)
                            TOGGLE_FLAG(element_input_->state_, ImGuiNodesConnectorStateFlag_Selected);
                        else
                        {
                            ClearAllConnectorSelections();
                            for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                                CLEAR_FLAG(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected);
                            SET_FLAG(element_input_->state_, ImGuiNodesConnectorStateFlag_Selected);
                        }
                    }
                    else if (state_ == ImGuiNodesState_DragingOutput && element_output_)
                    {
                        if (io.KeyCtrl)
                            TOGGLE_FLAG(element_output_->state_, ImGuiNodesConnectorStateFlag_Selected);
                        else
                        {
                            ClearAllConnectorSelections();
                            for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                                CLEAR_FLAG(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected);
                            SET_FLAG(element_output_->state_, ImGuiNodesConnectorStateFlag_Selected);
                        }
                    }
                }
            }

            connection_ = ImVec4();
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
                element_node_ = NULL;
                element_input_ = NULL;
                element_output_ = NULL;

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
                    IM_ASSERT(output.connections_ == 0);
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

    const ImVec2 offset = pos_ + scroll_;

    ImGui::SetWindowFontScale(scale_);

    bool any_node_hovered_ = false;
    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
    {
        const ImGuiNodesNode* node = nodes_[node_idx];
        IM_ASSERT(node);
        if (IS_SET(node->state_, ImGuiNodesNodeStateFlag_Selected))
        {
            any_node_hovered_ = true;
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
            
            if (const ImGuiNodesNode* target = input.source_node_)
            {
                IM_ASSERT(target);

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

                if (IS_SET(target->state_, ImGuiNodesNodeStateFlag_Collapsed))
                {
                    ImVec2 collapsed_output = { 0, (target->area_node_.Max.y - target->area_node_.Min.y) * 0.5f };					
                    
                    p4 += ((target->area_node_.Max - collapsed_output) * scale_);
                }
                else
                {
                    p4 += (input.source_output_->pos_ * scale_);		
                }

                ImColor color = !any_node_hovered_ || IS_SET(node->state_, ImGuiNodesNodeStateFlag_Selected) || IS_SET(target->state_, ImGuiNodesNodeStateFlag_Selected)
                    ? ImColor(1.0f, 1.0f, 1.0f, 1.0f)
                    : ImColor(0.4f, 0.4f, 0.4f, 0.4f);
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

    if (connection_.x != connection_.z && connection_.y != connection_.w)
        RenderConnection(ImVec2(connection_.x, connection_.y), ImVec2(connection_.z, connection_.w), ImColor(0.0f, 1.0f, 0.0f, 1.0f));

    ImGui::SetWindowFontScale(1.0f);

    if (state_ == ImGuiNodesState_Selecting)
    {
        draw_list->AddRectFilled(area_.Min, area_.Max, ImColor(1.0f, 1.0f, 0.0f, 0.1f));
        draw_list->AddRect(area_.Min, area_.Max, ImColor(1.0f, 1.0f, 0.0f, 0.5f));
    }

    ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
    ImGui::NewLine();
    
    switch (state_)
    {
        case ImGuiNodesState_Default: ImGui::Text("ImGuiNodesState_Default"); break;
        case ImGuiNodesState_HoveringNode: ImGui::Text("ImGuiNodesState_HoveringNode"); break;
        case ImGuiNodesState_HoveringInput: ImGui::Text("ImGuiNodesState_HoveringInput"); break;
        case ImGuiNodesState_HoveringOutput: ImGui::Text("ImGuiNodesState_HoveringOutput"); break;
        case ImGuiNodesState_Draging: ImGui::Text("ImGuiNodesState_Draging"); break;
        case ImGuiNodesState_DragingInput: ImGui::Text("ImGuiNodesState_DragingInput"); break;
        case ImGuiNodesState_DragingOutput: ImGui::Text("ImGuiNodesState_DragingOutput"); break;
        case ImGuiNodesState_Selecting: ImGui::Text("ImGuiNodesState_Selecting"); break;
        default: ImGui::Text("UNKNOWN"); break;
    }

    ImGui::NewLine();

    ImGui::Text("Position: %.2f, %.2f", pos_.x, pos_.y);
    ImGui::Text("Size: %.2f, %.2f", size_.x, size_.y);
    ImGui::Text("Mouse: %.2f, %.2f", mouse_.x, mouse_.y);
    ImGui::Text("Scroll: %.2f, %.2f", scroll_.x, scroll_.y);
    ImGui::Text("Scale: %.2f", scale_);

    ImGui::NewLine();
    
    if (element_node_)
        ImGui::Text("Element_node: %s", element_node_->name_.c_str());
    if (element_input_)
        ImGui::Text("Element_input: %s", element_input_->name_.c_str());
    if (element_output_)
        ImGui::Text("Element_output: %s", element_output_->name_.c_str());

    ImGui::NewLine();
}


void ImGuiNodes::ProcessContextMenu()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

    if (ImGui::BeginPopup("NodesContextMenu"))
    {
        ImVec2 position = (mouse_ - scroll_ - pos_) / scale_;
        if (ImGui::MenuItem("Test"))
        {
            ImGuiNodesNode* node = CreateNode(
                "Test", 
                ImColor(0.2f, 0.3f, 0.6f, 0.0f), 
                position,
                {"Float", "Int", "TextStream"},
                {"Float"});
            nodes_.push_back(node);
        }
        
        if (ImGui::MenuItem("InputBox"))
        {
            ImGuiNodesNode* node = CreateNode(
                "InputBox", 
                ImColor(0.3f, 0.5f, 0.5f, 0.0f), 
                position,
                {"Float1", "Float2", "Int1", "Int2", "GenericSink", "Vector", "Image", "Text"},
                {"TextStream", "Float", "Int"});
            nodes_.push_back(node);
        }
        
        if (ImGui::MenuItem("OutputBox"))
        {
            ImGuiNodesNode* node = CreateNode(
                "OutputBox", 
                ImColor(0.4f, 0.3f, 0.5f, 0.0f), 
                position,
                {"GenericSink1", "GenericSink2", "Float", "Int", "Text"},
                {"Vector", "Image", "Text", "Float", "Int", "Generic"});
            nodes_.push_back(node);
        }

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

ImGuiNodesInput::ImGuiNodesInput(const std::string& name)
{
    state_ = ImGuiNodesConnectorStateFlag_Default;
    source_node_ = NULL;
    source_output_ = NULL;
    name_ = name;

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
    if (state != ImGuiNodesState_Draging && IS_SET(state_, ImGuiNodesConnectorStateFlag_Hovered) && !IS_SET(state_, ImGuiNodesConnectorStateFlag_Consider))
    {
        const ImColor color = source_node_ == NULL ? ImColor(0.0f, 0.0f, 1.0f, 0.5f) : ImColor(1.0f, 0.5f, 0.0f, 0.5f);
        draw_list->AddRectFilled((area_input_.Min * scale) + offset, (area_input_.Max * scale) + offset, color);
    }

    if (HAS_ANY_FLAG(state_, ImGuiNodesConnectorStateFlag_Consider | ImGuiNodesConnectorStateFlag_Draging))
        draw_list->AddRectFilled((area_input_.Min * scale) + offset, (area_input_.Max * scale) + offset, ImColor(0.0f, 1.0f, 0.0f, 0.5f));

    if (IS_SET(state_, ImGuiNodesConnectorStateFlag_Selected))
        draw_list->AddRect((area_input_.Min * scale) + offset, (area_input_.Max * scale) + offset, ImColor(1.0f, 1.0f, 1.0f, 1.0f), 0.0f, 0, 2.0f * scale);

    bool consider_fill = false;
    consider_fill |= IS_SET(state_, ImGuiNodesConnectorStateFlag_Draging);
    consider_fill |= HAS_ALL_FLAGS(state_, ImGuiNodesConnectorStateFlag_Hovered | ImGuiNodesConnectorStateFlag_Consider);

    ImColor color = consider_fill ? ImColor(0.0f, 1.0f, 0.0f, 1.0f) : ImColor(1.0f, 1.0f, 1.0f, 1.0f);
            
    consider_fill |= bool(source_node_);

    if (consider_fill)
        draw_list->AddCircleFilled((pos_ * scale) + offset, (ImGuiNodesConnectorDotDiameter * 0.5f) * area_name_.GetHeight() * scale, color);
    else
        draw_list->AddCircle((pos_ * scale) + offset, (ImGuiNodesConnectorDotDiameter * 0.5f) * area_name_.GetHeight() * scale, color);

    ImGui::SetCursorScreenPos((area_name_.Min * scale) + offset);
    ImGui::Text("%s", name_.c_str());
}

void ImGuiNodesOutput::TranslateOutput(ImVec2 delta)
{
    pos_ += delta;
    area_output_.Translate(delta);
    area_name_.Translate(delta);
}

ImGuiNodesOutput::ImGuiNodesOutput(const std::string& name)
{
    state_ = ImGuiNodesConnectorStateFlag_Default;
    connections_count_ = 0;
    name_ = name;

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
    if (state != ImGuiNodesState_Draging && IS_SET(state_, ImGuiNodesConnectorStateFlag_Hovered) && !IS_SET(state_, ImGuiNodesConnectorStateFlag_Consider))
        draw_list->AddRectFilled((area_output_.Min * scale) + offset, (area_output_.Max * scale) + offset, ImColor(0.0f, 0.0f, 1.0f, 0.5f));

    if (HAS_ANY_FLAG(state_, ImGuiNodesConnectorStateFlag_Consider | ImGuiNodesConnectorStateFlag_Draging))
        draw_list->AddRectFilled((area_output_.Min * scale) + offset, (area_output_.Max * scale) + offset, ImColor(0.0f, 1.0f, 0.0f, 0.5f));

    if (IS_SET(state_, ImGuiNodesConnectorStateFlag_Selected))
        draw_list->AddRect((area_output_.Min * scale) + offset, (area_output_.Max * scale) + offset, ImColor(1.0f, 1.0f, 1.0f, 1.0f), 0.0f, 0, 2.0f * scale);

    bool consider_fill = false;
    consider_fill |= IS_SET(state_, ImGuiNodesConnectorStateFlag_Draging);
    consider_fill |= HAS_ALL_FLAGS(state_, ImGuiNodesConnectorStateFlag_Hovered | ImGuiNodesConnectorStateFlag_Consider);

    ImColor color = consider_fill ? ImColor(0.0f, 1.0f, 0.0f, 1.0f) : ImColor(1.0f, 1.0f, 1.0f, 1.0f);

    consider_fill |= bool(connections_count_ > 0);

    if (consider_fill)
        draw_list->AddCircleFilled((pos_ * scale) + offset, (ImGuiNodesConnectorDotDiameter * 0.5f) * area_name_.GetHeight() * scale, color);
    else
        draw_list->AddCircle((pos_ * scale) + offset, (ImGuiNodesConnectorDotDiameter * 0.5f) * area_name_.GetHeight() * scale, color);

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

ImGuiNodesNode::ImGuiNodesNode(const std::string& name, ImColor color)
{
    name_ = name;
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

    ImGui::SetCursorScreenPos(((area_name_.Min + ImVec2(2, 2)) * scale) + offset);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
    ImGui::Text("%s", name_.c_str());
    ImGui::PopStyleColor();

    ImGui::SetCursorScreenPos((area_name_.Min * scale) + offset);
    ImGui::Text("%s", name_.c_str());

    if (state_ & (ImGuiNodesNodeStateFlag_MarkedForSelection | ImGuiNodesNodeStateFlag_Selected))
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

void ImGuiNodes::RenderConnection(ImVec2 p1, ImVec2 p4, ImColor color)
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
    draw_list->AddBezierCubic(p1, p2, p3, p4, color, 1.5f * scale_);
    // draw_list->AddCircle(p2, 3.0f * scale_, color);
    // draw_list->AddCircle(p3, 3.0f * scale_, color);
}

bool ImGuiNodes::ConnectionMatrix(ImGuiNodesNode* input_node, ImGuiNodesNode* output_node, ImGuiNodesInput* input, ImGuiNodesOutput* output)
{
    return !input->source_node_;
}

ImGuiNodes::ImGuiNodes()
{
    scale_ = 1.0f;
    state_ = ImGuiNodesState_Default;
    element_node_ = NULL;
    element_input_ = NULL;
    element_output_ = NULL;
}

ImGuiNodes::~ImGuiNodes()
{
    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
        delete nodes_[node_idx];
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
