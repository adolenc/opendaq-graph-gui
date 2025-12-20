#include "nodes.h"
#include "IconsFontAwesome6.h"
#include <algorithm>

using namespace ImGui;


#define IS_SET(state, flag)         ((state) & (flag))
#define SET_FLAG(state, flag)       ((state) |= (flag))
#define CLEAR_FLAG(state, flag)     ((state) &= ~(flag))
#define TOGGLE_FLAG(state, flag)    ((state) ^= (flag))
#define HAS_ALL_FLAGS(state, flags) (((state) & (flags)) == (flags))
#define HAS_ANY_FLAG(state, flags)  ((state) & (flags))
#define CLEAR_FLAGS(state, flags)   ((state) &= ~(flags))

void ImGuiNodes::MoveSelectedNodesIntoView()
{
    std::vector<ImGuiNodesNode*> selected_nodes;
    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
    {
        if (IS_SET(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected))
            selected_nodes.push_back(nodes_[node_idx]);
    }

    if (selected_nodes.empty() || nodes_imgui_window_size_.x <= 0 || nodes_imgui_window_size_.y <= 0)
        return;

    ImRect union_rect = selected_nodes[0]->area_node_;
    for (size_t i = 1; i < selected_nodes.size(); ++i)
    {
        union_rect.Min = ImMin(union_rect.Min, selected_nodes[i]->area_node_.Min);
        union_rect.Max = ImMax(union_rect.Max, selected_nodes[i]->area_node_.Max);
    }

    if (selected_nodes.size() > 1)
    {
        ImRect visible_rect;
        visible_rect.Min = -scroll_ / scale_;
        visible_rect.Max = (-scroll_ + nodes_imgui_window_size_) / scale_;

        if (visible_rect.Contains(union_rect))
            return;
    }

    const float padding = 50.0f;
    ImVec2 available_size = nodes_imgui_window_size_ - ImVec2(padding * 2, padding * 2);
    available_size.x = ImMax(available_size.x, 100.0f);
    available_size.y = ImMax(available_size.y, 100.0f);

    ImVec2 required_size = union_rect.Max - union_rect.Min;

    if (selected_nodes.size() > 1)
    {
        if (required_size.x * scale_ > available_size.x || required_size.y * scale_ > available_size.y)
        {
            float scale_x = available_size.x / required_size.x;
            float scale_y = available_size.y / required_size.y;
            scale_ = ImMin(scale_x, scale_y);
            scale_ = std::clamp(scale_, 0.3f, 3.0f);
        }
    }

    ImVec2 center_node_space = union_rect.GetCenter();
    ImVec2 center_screen_space = center_node_space * scale_;
    ImVec2 window_center = nodes_imgui_window_size_ * 0.5f;

    scroll_ = window_center - center_screen_space;
}

void ImGuiNodes::SetSelectedNodes(const std::vector<ImGuiNodesUid>& selected_ids)
{
    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
    {
        CLEAR_FLAG(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected);
        CLEAR_FLAG(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_MarkedForSelection);
    }
    ClearAllConnectorSelections();

    for (const auto& id : selected_ids)
    {
        if (auto it = nodes_by_uid_.find(id); it != nodes_by_uid_.end())
            SET_FLAG(it->second->state_, ImGuiNodesNodeStateFlag_Selected);
        else if (auto it = inputs_by_uid_.find(id); it != inputs_by_uid_.end())
            SET_FLAG(it->second->state_, ImGuiNodesConnectorStateFlag_Selected);
        else if (auto it = outputs_by_uid_.find(id); it != outputs_by_uid_.end())
            SET_FLAG(it->second.output->state_, ImGuiNodesConnectorStateFlag_Selected);
    }

    SortSelectedNodesOrder();
}

static bool OtherImGuiWindowIsBlockingInteraction()
{
    return ImGui::GetIO().WantCaptureMouse && !ImGui::IsWindowHovered();
}

unsigned int ImGuiNodesIdentifier::id_counter_ = 0;

ImVec2 ImGuiNodes::UpdateEdgeScrolling()
{
    const ImGuiIO& io = ImGui::GetIO();

    if (io.MouseDragMaxDistanceSqr[0] <= 25.0f * 25.0f)
        return ImVec2(0.0f, 0.0f);

    const float edge_zone = 80.0f;
    const float max_scroll_speed = 50.0f;

    ImVec2 canvas_min = nodes_imgui_window_pos_;
    ImVec2 canvas_max = nodes_imgui_window_pos_ + nodes_imgui_window_size_;

    ImVec2 scroll_delta(0.0f, 0.0f);
    if (mouse_.x < canvas_min.x + edge_zone)
    {
        float t = std::clamp((canvas_min.x + edge_zone - mouse_.x) / edge_zone, 0.0f, 1.0f);
        scroll_delta.x = t * max_scroll_speed;
    }
    else if (mouse_.x > canvas_max.x - edge_zone)
    {
        float t = std::clamp((mouse_.x - (canvas_max.x - edge_zone)) / edge_zone, 0.0f, 1.0f);
        scroll_delta.x = -t * max_scroll_speed;
    }

    if (mouse_.y < canvas_min.y + edge_zone)
    {
        float t = std::clamp((canvas_min.y + edge_zone - mouse_.y) / edge_zone, 0.0f, 1.0f);
        scroll_delta.y = t * max_scroll_speed;
    }
    else if (mouse_.y > canvas_max.y - edge_zone)
    {
        float t = std::clamp((mouse_.y - (canvas_max.y - edge_zone)) / edge_zone, 0.0f, 1.0f);
        scroll_delta.y = -t * max_scroll_speed;
    }

    scroll_ += scroll_delta;
    return scroll_delta;
}

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

    // First limit the minimap size to 250px in the largest dimension
    float max_minimap_size = 250.0f;
    float aspect_ratio = (nodes_imgui_window_size_.x > 0.0f) ? (nodes_imgui_window_size_.y / nodes_imgui_window_size_.x) : 1.0f;
    ImVec2 minimap_size;
    if (aspect_ratio > 1.0f)
        minimap_size = ImVec2(max_minimap_size / aspect_ratio, max_minimap_size);
    else
        minimap_size = ImVec2(max_minimap_size, max_minimap_size * aspect_ratio);
    // After that limit minimap to 20% of canvas width or height, so smaller canvases have smaller minimaps
    if (nodes_imgui_window_size_.x > 0.0f && nodes_imgui_window_size_.y > 0.0f)
        minimap_size *= ImMin(1.0f, ImMin((nodes_imgui_window_size_.x * 0.2f) / minimap_size.x, (nodes_imgui_window_size_.y * 0.2f) / minimap_size.y));
    ImVec2 padding(20.0f, 20.0f);
    minimap_rect_.Max = nodes_imgui_window_pos_ + nodes_imgui_window_size_ - padding;
    minimap_rect_.Min = minimap_rect_.Max - minimap_size;

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

        if (io.MouseWheel != 0.0f)
        {
            if (!OtherImGuiWindowIsBlockingInteraction())
            {
                if (minimap_rect_.Contains(mouse_))
                {
                    if (io.MouseWheel < 0.0f)
                        for (float zoom = io.MouseWheel; zoom < 0.0f; zoom += 1.0f)
                            minimap_preview_scale_ = ImMax(0.3f, minimap_preview_scale_ / 1.15f);

                    if (io.MouseWheel > 0.0f)
                        for (float zoom = io.MouseWheel; zoom > 0.0f; zoom -= 1.0f)
                            minimap_preview_scale_ = ImMin(3.0f, minimap_preview_scale_ * 1.15f);
                }
                else
                {
                    ImVec2 focus = (mouse_ - scroll_ - nodes_imgui_window_pos_) / scale_;

                    if (io.MouseWheel < 0.0f)
                        for (float zoom = io.MouseWheel; zoom < 0.0f; zoom += 1.0f)
                            scale_ = ImMax(0.3f, scale_ / 1.15f);

                    if (io.MouseWheel > 0.0f)
                        for (float zoom = io.MouseWheel; zoom > 0.0f; zoom -= 1.0f)
                            scale_ = ImMin(3.0f, scale_ * 1.15f);

                    ImVec2 shift = scroll_ + (focus * scale_);
                    scroll_ += mouse_ - shift - nodes_imgui_window_pos_;

                    minimap_preview_scale_ = scale_;
                }
            }
        }
        else if (!minimap_rect_.Contains(mouse_))
        {
            minimap_preview_scale_ = scale_;
        }
    }

    const float grid = 64.0f * scale_;
    for (float x = fmodf(scroll_.x, grid); x < nodes_imgui_window_size_.x; x += grid)
    {		
        for (float y = fmodf(scroll_.y, grid); y < nodes_imgui_window_size_.y; y += grid)
        {
            int mark_x = (int)((x - scroll_.x) / grid);
            int mark_y = (int)((y - scroll_.y) / grid);
            
            ImVec4 grid_base_color = ImGui::GetStyle().Colors[ImGuiCol_Text];
            float alpha = ((mark_y % 5) || (mark_x % 5)) ? 0.05f : 0.2f;
            // In dark mode (light text), we can use slightly higher alpha
            if (grid_base_color.x > 0.5f)
                alpha *= 1.5f;
                
            ImColor color = ImColor(grid_base_color.x, grid_base_color.y, grid_base_color.z, alpha);
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

    if (OtherImGuiWindowIsBlockingInteraction())
        hovered_node = NULL;

    bool mouse_in_minimap = minimap_rect_.Contains(mouse_);

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

        if (!hovered_node && !OtherImGuiWindowIsBlockingInteraction() && !mouse_in_minimap && node_rect.Contains(mouse_))
            hovered_node = node;

        // Also check if mouse is over the add button area (which extends beyond the node)
        if (!hovered_node && !OtherImGuiWindowIsBlockingInteraction() && !mouse_in_minimap && IS_SET(node->state_, ImGuiNodesNodeStateFlag_Visible))
        {
            ImRect add_button_rect = node->area_add_button_;
            add_button_rect.Min *= scale_;
            add_button_rect.Max *= scale_;
            add_button_rect.Translate(offset);
            
            if (add_button_rect.Contains(mouse_))
            {
                hovered_node = node;
            }
            else
            {
                ImRect active_button_rect = node->area_active_button_;
                active_button_rect.Min *= scale_;
                active_button_rect.Max *= scale_;
                active_button_rect.Translate(offset);

                if (active_button_rect.Contains(mouse_))
                    hovered_node = node;
                else
                {
                    ImRect trash_button_rect = node->area_trash_button_;
                    trash_button_rect.Min *= scale_;
                    trash_button_rect.Max *= scale_;
                    trash_button_rect.Translate(offset);

                    if (trash_button_rect.Contains(mouse_))
                        hovered_node = node;
                }
            }
        }

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
            else
            {
                ImRect btn_rect = output.area_active_button_;
                btn_rect.Min *= scale_;
                btn_rect.Max *= scale_;
                btn_rect.Translate(offset);

                if (btn_rect.Contains(mouse_))
                    SET_FLAG(output.state_, ImGuiNodesConnectorStateFlag_Hovered);
            }
        }	
    }

    if (hovered_node)
        SET_FLAG(hovered_node->state_, ImGuiNodesNodeStateFlag_Hovered);

    return hovered_node;
}

void ImGuiNodes::AddNode(const ImGuiNodesIdentifier& name, int color_index, 
                         const std::vector<ImGuiNodesIdentifier>& inputs,
                         const std::vector<ImGuiNodesIdentifier>& outputs,
                         ImGuiNodesUid parent_uid)
{
    ImVec2 pos(0.0f, 0.0f);

    if (auto it = node_cache_.find(name.id_); it != node_cache_.end())
    {
        pos = it->second.pos;
        color_index = it->second.color_index;
        AddNode(name, color_index, pos, inputs, outputs, parent_uid);

        if (it->second.is_selected)
        {
            if (auto new_node_it = nodes_by_uid_.find(name.id_); new_node_it != nodes_by_uid_.end())
                SET_FLAG(new_node_it->second->state_, ImGuiNodesNodeStateFlag_Selected);
        }
        return;
    }

    // If we are in batch mode but have a cache, we are likely rebuilding/updating the graph.
    // In this case, we want to manually position new nodes (not in cache) instead of relying on EndBatchAdd layout,
    // because EndBatchAdd layout would overwrite the positions of our cached nodes.
    bool use_auto_layout = batch_add_mode_ && node_cache_.empty();

    if (!use_auto_layout)
    {
        ImGuiNodesNode* parent = nullptr;
        if (!parent_uid.empty())
        {
            if (auto it = nodes_by_uid_.find(parent_uid); it != nodes_by_uid_.end())
                parent = it->second;
        }

        ImGuiNodesNode* bottom_most_sibling = nullptr;
        float max_y = -1.0e30f;

        for (int i = 0; i < nodes_.size(); ++i)
        {
            if (nodes_[i]->parent_node_ == parent)
            {
                if (nodes_[i]->area_node_.Max.y > max_y)
                {
                    max_y = nodes_[i]->area_node_.Max.y;
                    bottom_most_sibling = nodes_[i];
                }
            }
        }

        if (bottom_most_sibling)
        {
            float spacing = 20.0f;
            float sibling_height = bottom_most_sibling->area_node_.GetHeight();
            float estimated_height = ImMax(sibling_height, 80.0f);

            ImVec2 sibling_center = bottom_most_sibling->area_node_.GetCenter();
            pos.x = sibling_center.x;
            pos.y = sibling_center.y + (sibling_height * 0.5f) + spacing + (estimated_height * 0.5f);
        }
        else
        {
            pos = parent ? parent->area_node_.GetCenter() + ImVec2(300.0f, 0.0f) : ImVec2(50.0f, 50.0f);
        }
    }
    AddNode(name, color_index, pos, inputs, outputs, parent_uid);
}

void ImGuiNodes::AddNode(const ImGuiNodesIdentifier& name, int color_index, ImVec2 pos, 
                         const std::vector<ImGuiNodesIdentifier>& inputs,
                         const std::vector<ImGuiNodesIdentifier>& outputs,
                        ImGuiNodesUid parent_uid)
{
    ImGuiNodesNode* node = new ImGuiNodesNode(name, color_index);
    node->parent_node_ = NULL;
    if (!parent_uid.empty())
    {
        if (auto it = nodes_by_uid_.find(parent_uid); it != nodes_by_uid_.end())
        {
            node->parent_node_ = it->second;
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
    nodes_by_uid_[node->uid_] = node;
    
    for (int input_idx = 0; input_idx < node->inputs_.size(); ++input_idx)
        inputs_by_uid_[node->inputs_[input_idx].uid_] = &node->inputs_[input_idx];
    
    for (int output_idx = 0; output_idx < node->outputs_.size(); ++output_idx)
        outputs_by_uid_[node->outputs_[output_idx].uid_] = {&node->outputs_[output_idx], node};
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
        }
        else
            nodes_unselected.push_back(node);
            
        // Always check for selected connectors, regardless of node selection state
        for (int input_idx = 0; input_idx < node->inputs_.size(); ++input_idx)
            if (IS_SET(node->inputs_[input_idx].state_, ImGuiNodesConnectorStateFlag_Selected))
                selected_ids.push_back(node->inputs_[input_idx].uid_);
        for (int output_idx = 0; output_idx < node->outputs_.size(); ++output_idx)
            if (IS_SET(node->outputs_[output_idx].state_, ImGuiNodesConnectorStateFlag_Selected))
                selected_ids.push_back(node->outputs_[output_idx].uid_);
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
    bool was_hovering_input = (state_ == ImGuiNodesState_HoveringInput);
    ImGuiNodesUid previous_active_output_uid = active_output_ ? active_output_->uid_ : "";
    ImGuiNodesUid previous_active_input_uid = active_input_ ? active_input_->uid_ : "";

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

    if (state_ == ImGuiNodesState_HoveringInput)
    {
        if (interaction_handler_ && active_input_)
        {
            if (previous_active_input_uid != active_input_->uid_)
                interaction_handler_->OnInputHover("");
            interaction_handler_->OnInputHover(active_input_->uid_);
        }
    }
    else if (was_hovering_input && interaction_handler_)
        interaction_handler_->OnInputHover("");

    ProcessNodes();
}

void ImGuiNodes::ProcessInteractions()
{
    static bool blocked_by_imgui_interaction = false;

    const ImGuiIO& io = ImGui::GetIO();
    
    UpdateCanvasGeometry(ImGui::GetWindowDrawList());

    if (state_ == ImGuiNodesState_Default && ImGui::IsMouseClicked(0) && minimap_rect_.Contains(mouse_) && !OtherImGuiWindowIsBlockingInteraction())
    {
        if (!nodes_.empty())
        {
             ImRect world_bounds = nodes_[0]->area_node_;
             for (auto* node : nodes_)
                 world_bounds.Add(node->area_node_);

             ImRect view_rect;
             view_rect.Min = -scroll_ / scale_;
             view_rect.Max = (-scroll_ + nodes_imgui_window_size_) / scale_;
             world_bounds.Add(view_rect);

             ImVec2 world_size = world_bounds.GetSize();
             ImVec2 minimap_size = minimap_rect_.GetSize();

             if (world_size.x > 0.0f && world_size.y > 0.0f)
             {
                 float scale_x = minimap_size.x / world_size.x;
                 float scale_y = minimap_size.y / world_size.y;
                 float mm_scale = ImMin(scale_x, scale_y);

                 ImVec2 mm_content_size = world_size * mm_scale;
                 ImVec2 mm_offset = minimap_rect_.Min + (minimap_size - mm_content_size) * 0.5f;

                 scale_ = minimap_preview_scale_;
                 ImVec2 target_world_center = (mouse_ - mm_offset) / mm_scale + world_bounds.Min;
                 scroll_ = nodes_imgui_window_size_ * 0.5f - target_world_center * scale_;
             }
        }
    }

    ImGuiNodesNode* hovered_node = UpdateNodesFromCanvas();

    bool consider_hover = state_ == ImGuiNodesState_Default;
    consider_hover |= state_ == ImGuiNodesState_HoveringNode;
    consider_hover |= state_ == ImGuiNodesState_HoveringInput;
    consider_hover |= state_ == ImGuiNodesState_HoveringOutput;
    consider_hover |= state_ == ImGuiNodesState_HoveringAddButton;
    consider_hover |= state_ == ImGuiNodesState_HoveringActiveButton;
    consider_hover |= state_ == ImGuiNodesState_HoveringTrashButton;
    consider_hover |= state_ == ImGuiNodesState_HoveringOutputActiveButton;

    if (hovered_node && consider_hover && !OtherImGuiWindowIsBlockingInteraction())
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
                
                ImRect btn_rect = active_output_->area_active_button_;
                btn_rect.Min *= scale_;
                btn_rect.Max *= scale_;
                btn_rect.Translate(nodes_imgui_window_pos_ + scroll_);

                if (btn_rect.Contains(mouse_))
                    state_ = ImGuiNodesState_HoveringOutputActiveButton;
                else
                    state_ = ImGuiNodesState_HoveringOutput;
                break;
            }
        }					

        if (!active_input_ && !active_output_)
        {
            ImVec2 offset = nodes_imgui_window_pos_ + scroll_;
            ImRect add_button_rect = hovered_node->area_add_button_;
            add_button_rect.Min *= scale_;
            add_button_rect.Max *= scale_;
            add_button_rect.Translate(offset);
            
            if (add_button_rect.Contains(mouse_))
                state_ = ImGuiNodesState_HoveringAddButton;
            else
            {
                ImRect active_button_rect = hovered_node->area_active_button_;
                active_button_rect.Min *= scale_;
                active_button_rect.Max *= scale_;
                active_button_rect.Translate(offset);

                if (active_button_rect.Contains(mouse_))
                    state_ = ImGuiNodesState_HoveringActiveButton;
                else
                {
                    ImRect trash_button_rect = hovered_node->area_trash_button_;
                    trash_button_rect.Min *= scale_;
                    trash_button_rect.Max *= scale_;
                    trash_button_rect.Translate(offset);

                    if (trash_button_rect.Contains(mouse_))
                        state_ = ImGuiNodesState_HoveringTrashButton;
                    else
                        state_ = ImGuiNodesState_HoveringNode;
                }
            }	
        }
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
        if (OtherImGuiWindowIsBlockingInteraction() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
            return;

        switch (state_)
        {
            case ImGuiNodesState_Default:
                return;

            case ImGuiNodesState_HoveringInput:
            {
                if (active_input_->source_node_)
                {
                    RemoveConnection(active_input_->uid_);
                    if (interaction_handler_)
                        interaction_handler_->OnConnectionRemoved(active_input_->uid_);
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
        if (OtherImGuiWindowIsBlockingInteraction() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
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

            case ImGuiNodesState_HoveringAddButton:
            {
                state_ = ImGuiNodesState_DraggingParentConnection;
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

        return;
    }

    if (ImGui::IsMouseDragging(0))
    {
        if (blocked_by_imgui_interaction || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
            return;
            
        switch (state_)
        {
            case ImGuiNodesState_Default:
            {
                ImRect canvas(nodes_imgui_window_pos_, nodes_imgui_window_pos_ + nodes_imgui_window_size_);
                if (!canvas.Contains(mouse_) || minimap_rect_.Contains(mouse_))
                    return;

                if (!io.KeyCtrl)
                {
                    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                        CLEAR_FLAGS(nodes_[node_idx]->state_, ImGuiNodesNodeStateFlag_Selected | ImGuiNodesNodeStateFlag_MarkedForSelection);

                    ClearAllConnectorSelections();
                    SortSelectedNodesOrder();
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

                    RemoveConnection(active_input_->uid_);
                    if (interaction_handler_)
                        interaction_handler_->OnConnectionRemoved(active_input_->uid_);

                    state_ = ImGuiNodesState_DraggingOutput;
                    return;
                }

                ImVec2 edge_scroll_delta = UpdateEdgeScrolling();
                ImVec2 total_delta = (io.MouseDelta - edge_scroll_delta) / scale_;
                if (!IS_SET(active_node_->state_, ImGuiNodesNodeStateFlag_Selected))
                    active_node_->TranslateNode(total_delta, false);
                else
                    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
                        nodes_[node_idx]->TranslateNode(total_delta, true);

                return;
            }

            case ImGuiNodesState_DraggingInput:
            case ImGuiNodesState_DraggingOutput:
            {
                ImVec2 offset = nodes_imgui_window_pos_ + scroll_;

                if (state_ == ImGuiNodesState_DraggingInput)
                {
                    ImVec2 p1 = offset + (active_input_->pos_ * scale_);
                    ImVec2 p4 = active_output_ ? (offset + (active_output_->pos_ * scale_)) : mouse_;
                    active_dragging_connection_ = ImVec4(p1.x, p1.y, p4.x, p4.y);
                }
                else // state_ == ImGuiNodesState_DraggingOutput
                {
                    ImVec2 p1 = offset + (active_output_->pos_ * scale_);
                    ImVec2 p4 = active_input_ ? (offset + (active_input_->pos_ * scale_)) : mouse_;
                    active_dragging_connection_ = ImVec4(p4.x, p4.y, p1.x, p1.y);
                }

                UpdateEdgeScrolling();
                return;
            }

            case ImGuiNodesState_DraggingParentConnection:
            {
                ImVec2 p1 = mouse_;
                ImVec2 offset = nodes_imgui_window_pos_ + scroll_;
                ImVec2 button_center = active_node_->area_add_button_.GetCenter();
                ImVec2 p4 = offset + (button_center * scale_);

                active_dragging_connection_ = ImVec4(p1.x, p1.y, p4.x, p4.y);
                return;
            }
        }

        return;
    }

    if (ImGui::IsMouseReleased(0))
    {
        if (OtherImGuiWindowIsBlockingInteraction() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
            blocked_by_imgui_interaction = true;
        else
            blocked_by_imgui_interaction = false;

        switch (state_)
        {
        case ImGuiNodesState_Default:
        {
            if (io.MouseDragMaxDistanceSqr[0] < (io.MouseDragThreshold * io.MouseDragThreshold) &&
                !OtherImGuiWindowIsBlockingInteraction() &&
                !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) &&
                !minimap_rect_.Contains(mouse_))
            {
                if (interaction_handler_)
                {
                    ImVec2 position = (mouse_ - scroll_ - nodes_imgui_window_pos_) / scale_;
                    interaction_handler_->OnEmptySpaceClick(position);
                }
            }
            return;
        }

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

        case ImGuiNodesState_HoveringActiveButton:
        {
            if (interaction_handler_)
                interaction_handler_->OnNodeActiveToggle(active_node_->uid_);
            
            state_ = ImGuiNodesState_Default;
            return;
        }

        case ImGuiNodesState_HoveringTrashButton:
        {
            if (interaction_handler_)
                interaction_handler_->OnNodeTrashClick(active_node_->uid_);
            
            state_ = ImGuiNodesState_Default;
            return;
        }

        case ImGuiNodesState_HoveringOutputActiveButton:
        {
            if (interaction_handler_)
                interaction_handler_->OnSignalActiveToggle(active_output_->uid_);
            
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
                    SortSelectedNodesOrder();
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
                    SortSelectedNodesOrder();
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
                
                AddConnection(active_output_->uid_, active_input_->uid_);
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
                        SortSelectedNodesOrder();
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
                        SortSelectedNodesOrder();
                    }
                }
                else
                {
                    if (state_ == ImGuiNodesState_DraggingInput)
                    {
                        if (interaction_handler_)
                            interaction_handler_->OnInputDropped(active_input_->uid_, std::nullopt);
                    }
                }
            }

            active_dragging_connection_ = ImVec4();
            state_ = ImGuiNodesState_Default;
            return;
        }

        case ImGuiNodesState_DraggingParentConnection:
        {
            if (io.MouseDragMaxDistanceSqr[0] < (io.MouseDragThreshold * io.MouseDragThreshold))
            {
                if (interaction_handler_)
                    interaction_handler_->OnAddButtonClick(active_node_->uid_, std::nullopt);
            }
            else
            {
                ImVec2 drop_position = (mouse_ - scroll_ - nodes_imgui_window_pos_) / scale_;
                if (interaction_handler_)
                    interaction_handler_->OnAddButtonClick(active_node_->uid_, drop_position);
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
                    RemoveConnection(input.uid_);
                    if (interaction_handler_)
                        interaction_handler_->OnConnectionRemoved(input.uid_);
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
                node_cache_[node->uid_] = { node->area_node_.GetCenter(), node->color_index_, true };

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
                            RemoveConnection(input.uid_);
                            if (interaction_handler_)
                                interaction_handler_->OnConnectionRemoved(input.uid_);
                        }
                    }
                }

                for (int input_idx = 0; input_idx < node->inputs_.size(); ++input_idx)
                {
                    ImGuiNodesInput& input = node->inputs_[input_idx];
                    RemoveConnection(input.uid_);
                    if (interaction_handler_)
                        interaction_handler_->OnConnectionRemoved(input.uid_);
                    input.name_.clear();
                }

                for (int output_idx = 0; output_idx < node->outputs_.size(); ++output_idx)
                {
                    ImGuiNodesOutput& output = node->outputs_[output_idx];
                    IM_ASSERT(output.connections_count_ == 0);
                }
    
                nodes_by_uid_.erase(node->uid_);
                for (int input_idx = 0; input_idx < node->inputs_.size(); ++input_idx)
                    inputs_by_uid_.erase(node->inputs_[input_idx].uid_);
                for (int output_idx = 0; output_idx < node->outputs_.size(); ++output_idx)
                    outputs_by_uid_.erase(node->outputs_[output_idx].uid_);
                
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

    if (HAS_ANY_FLAG(state_, ImGuiNodesState_HoveringNode | ImGuiNodesState_HoveringInput | ImGuiNodesState_HoveringOutput | ImGuiNodesState_HoveringAddButton | ImGuiNodesState_HoveringActiveButton | ImGuiNodesState_HoveringTrashButton | ImGuiNodesState_HoveringOutputActiveButton | ImGuiNodesState_Dragging | ImGuiNodesState_DraggingInput | ImGuiNodesState_DraggingOutput | ImGuiNodesState_DraggingParentConnection))
    {
        for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
        {
            const ImGuiNodesNode* node = nodes_[node_idx];
            IM_ASSERT(node);
            if (node->parent_node_)
            {
                ImVec2 head_offset(0.0f, node->area_name_.GetHeight() * 0.8f);
                ImColor color = ImGuiNodes::color_palette_[node->parent_node_->color_index_];
                color.Value.w = 0.125f;
                RenderConnection(offset + (node->area_node_.GetTL() + head_offset) * scale_, offset + (node->parent_node_->area_node_.GetTR() + head_offset) * scale_, color, 10.0f);
            }
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
                    ? ImColor(ImGui::GetStyle().Colors[ImGuiCol_Text])
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

    // Display tooltip for warning/error nodes when hovering over header area
    if (state_ == ImGuiNodesState_HoveringNode && active_node_ && !OtherImGuiWindowIsBlockingInteraction())
    {
        bool show_tooltip = false;
        std::string tooltip_message;
        
        // Error takes precedence over warning
        if (IS_SET(active_node_->state_, ImGuiNodesNodeStateFlag_Error) && !active_node_->error_message_.empty())
        {
            show_tooltip = true;
            tooltip_message = active_node_->error_message_;
        }
        else if (IS_SET(active_node_->state_, ImGuiNodesNodeStateFlag_Warning) && !active_node_->warning_message_.empty())
        {
            show_tooltip = true;
            tooltip_message = active_node_->warning_message_;
        }
        
        if (show_tooltip)
        {
            ImRect node_rect = active_node_->area_node_;
            node_rect.Min *= scale_;
            node_rect.Max *= scale_;
            node_rect.Translate(offset);
            ImRect header_area = ImRect(node_rect.GetTL(), node_rect.GetTR() + ImVec2(0.0f, active_node_->title_height_ * scale_));

            if (header_area.Contains(mouse_))
                ImGui::SetTooltip("%s", tooltip_message.c_str());
        }
    }

    if (!OtherImGuiWindowIsBlockingInteraction())
    {
        if (state_ == ImGuiNodesState_HoveringActiveButton)
            ImGui::SetTooltip("Toggle node active state");
        else if (state_ == ImGuiNodesState_HoveringTrashButton)
            ImGui::SetTooltip("Delete node");
        else if (state_ == ImGuiNodesState_HoveringAddButton)
            ImGui::SetTooltip("Add nested node");
    }

    if (active_dragging_connection_.x != active_dragging_connection_.z && active_dragging_connection_.y != active_dragging_connection_.w)
    {
        if (state_ == ImGuiNodesState_DraggingParentConnection)
        {
            ImColor color = ImGuiNodes::color_palette_[active_node_->color_index_];
            color.Value.w = 0.125f;
            RenderConnection(ImVec2(active_dragging_connection_.x, active_dragging_connection_.y), 
                             ImVec2(active_dragging_connection_.z, active_dragging_connection_.w), 
                             color,
                             10.0f);
        }
        else
        {
            RenderConnection(ImVec2(active_dragging_connection_.x, active_dragging_connection_.y), 
                            ImVec2(active_dragging_connection_.z, active_dragging_connection_.w), 
                            ImColor(ImGui::GetStyle().Colors[ImGuiCol_Text]));
        }
    }

    RenderMinimap(draw_list);

    ImGui::SetWindowFontScale(1.0f);

    if (state_ == ImGuiNodesState_Selecting)
    {
        // Use the theme's NavHighlight color (usually blue) for selection in both modes
        ImVec4 base_color = ImGui::GetStyle().Colors[ImGuiCol_NavHighlight];
        
        ImColor fill_color = base_color;
        fill_color.Value.w = 0.2f;
        
        ImColor border_color = base_color;
        border_color.Value.w = 0.8f;

        draw_list->AddRectFilled(active_dragging_selection_area_.Min, active_dragging_selection_area_.Max, fill_color);
        draw_list->AddRect(active_dragging_selection_area_.Min, active_dragging_selection_area_.Max, border_color);
    }

    ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));

    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::NewLine();

#ifndef NDEBUG
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
        case ImGuiNodesState_HoveringAddButton: ImGui::Text("ImGuiNodesState_HoveringAddButton"); break;
        case ImGuiNodesState_Dragging: ImGui::Text("ImGuiNodesState_Draging"); break;
        case ImGuiNodesState_DraggingInput: ImGui::Text("ImGuiNodesState_DragingInput"); break;
        case ImGuiNodesState_DraggingOutput: ImGui::Text("ImGuiNodesState_DragingOutput"); break;
        case ImGuiNodesState_DraggingParentConnection: ImGui::Text("ImGuiNodesState_DraggingParentConnection"); break;
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
#endif
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
        draw_list->AddRectFilled((area_input_.Min * scale) + offset, (area_input_.Max * scale) + offset, ImGui::GetColorU32(ImGuiCol_Text, 0.2f));
    }

    if (HAS_ANY_FLAG(state_, ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget | ImGuiNodesConnectorStateFlag_Dragging))
        draw_list->AddRectFilled((area_input_.Min * scale) + offset, (area_input_.Max * scale) + offset, ImGui::GetColorU32(ImGuiCol_Text, 0.2f));

    if (IS_SET(state_, ImGuiNodesConnectorStateFlag_Selected))
        draw_list->AddRect((area_input_.Min * scale) + offset, (area_input_.Max * scale) + offset, ImGui::GetColorU32(ImGuiCol_Text, 0.5f), 0.0f, 0, 2.0f * scale);

    bool consider_fill = false;
    consider_fill |= IS_SET(state_, ImGuiNodesConnectorStateFlag_Dragging);
    consider_fill |= HAS_ALL_FLAGS(state_, ImGuiNodesConnectorStateFlag_Hovered | ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget);
    consider_fill |= bool(source_node_);
    if (consider_fill)
        draw_list->AddCircleFilled((pos_ * scale) + offset, (ImGuiNodesConnectorDotDiameter * 0.5f) * area_name_.GetHeight() * scale, ImGuiNodes::connection_color_);
    draw_list->AddCircle((pos_ * scale) + offset, (ImGuiNodesConnectorDotDiameter * 0.5f) * area_name_.GetHeight() * scale, ImGuiNodes::text_color_);

    ImGui::SetCursorScreenPos((area_name_.Min * scale) + offset);
    ImGui::TextColored(ImGuiNodes::text_color_, "%s", name_.c_str());
}

void ImGuiNodesOutput::TranslateOutput(ImVec2 delta)
{
    pos_ += delta;
    area_output_.Translate(delta);
    area_name_.Translate(delta);
    area_active_button_.Translate(delta);
}

ImGuiNodesOutput::ImGuiNodesOutput(const ImGuiNodesIdentifier& name)
{
    state_ = ImGuiNodesConnectorStateFlag_Default;
    connections_count_ = 0;
    name_ = name.name_;
    uid_ = name.id_;

    area_name_.Min = ImVec2(0.0f, 0.0f);
    area_name_.Max = ImGui::CalcTextSize(name_.c_str());

    float button_size = area_name_.GetHeight();
    area_active_button_ = ImRect(0.0f, 0.0f, button_size, button_size);

    area_output_.Min = ImVec2(0.0f, 0.0f);
    area_output_.Max.x = ImGuiNodesConnectorDotPadding + ImGuiNodesConnectorDotDiameter + ImGuiNodesConnectorDotPadding;
    area_output_.Max.y = ImGuiNodesConnectorDistance;
    area_output_.Max *= area_name_.GetHeight();

    ImVec2 offset = ImVec2(0.0f, 0.0f) - area_output_.GetCenter();

    area_name_.Translate(ImVec2(-area_name_.GetWidth(), (area_output_.GetHeight() - area_name_.GetHeight()) * 0.5f));
    area_active_button_.Translate(ImVec2(area_name_.Min.x - button_size - (ImGuiNodesConnectorDotPadding * area_name_.GetHeight()), (area_output_.GetHeight() - button_size) * 0.5f));

    area_output_.Min.x = area_active_button_.Min.x;

    area_output_.Translate(offset);
    area_name_.Translate(offset);
    area_active_button_.Translate(offset);
}

void ImGuiNodesOutput::Render(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const
{
    if (state != ImGuiNodesState_Dragging && IS_SET(state_, ImGuiNodesConnectorStateFlag_Hovered) && !IS_SET(state_, ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget))
        draw_list->AddRectFilled((area_output_.Min * scale) + offset, (area_output_.Max * scale) + offset, ImGui::GetColorU32(ImGuiCol_Text, 0.2f));

    if (HAS_ANY_FLAG(state_, ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget | ImGuiNodesConnectorStateFlag_Dragging))
        draw_list->AddRectFilled((area_output_.Min * scale) + offset, (area_output_.Max * scale) + offset, ImGui::GetColorU32(ImGuiCol_Text, 0.2f));

    if (IS_SET(state_, ImGuiNodesConnectorStateFlag_Selected))
        draw_list->AddRect((area_output_.Min * scale) + offset, (area_output_.Max * scale) + offset, ImGui::GetColorU32(ImGuiCol_Text, 0.5f), 0.0f, 0, 2.0f * scale);

    if (HAS_ANY_FLAG(state_, ImGuiNodesConnectorStateFlag_Hovered | ImGuiNodesConnectorStateFlag_Selected))
    {
        ImRect active_btn_rect = area_active_button_;
        active_btn_rect.Min *= scale;
        active_btn_rect.Max *= scale;
        active_btn_rect.Translate(offset);

        ImColor btn_color = ImColor(0.9f, 0.9f, 0.9f, 1.0f);
        draw_list->AddRectFilled(active_btn_rect.Min, active_btn_rect.Max, btn_color, 0.0f);
        
        ImGui::SetWindowFontScale(scale * 0.7f);
        ImVec2 text_size = ImGui::CalcTextSize(ICON_FA_POWER_OFF);
        ImGui::SetCursorScreenPos(active_btn_rect.GetCenter() - text_size * 0.5f);
        ImGui::TextColored(ImColor(0.2f, 0.2f, 0.2f, 1.0f), ICON_FA_POWER_OFF);
        ImGui::SetWindowFontScale(scale);
    }

    bool consider_fill = false;
    consider_fill |= IS_SET(state_, ImGuiNodesConnectorStateFlag_Dragging);
    consider_fill |= HAS_ALL_FLAGS(state_, ImGuiNodesConnectorStateFlag_Hovered | ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget);
    consider_fill |= bool(connections_count_ > 0);
    if (consider_fill)
        draw_list->AddCircleFilled((pos_ * scale) + offset, (ImGuiNodesConnectorDotDiameter * 0.5f) * area_name_.GetHeight() * scale, ImGuiNodes::connection_color_);
    draw_list->AddCircle((pos_ * scale) + offset, (ImGuiNodesConnectorDotDiameter * 0.5f) * area_name_.GetHeight() * scale, ImGuiNodes::text_color_);

    ImGui::SetCursorScreenPos((area_name_.Min * scale) + offset);
    ImColor text_color = IS_SET(state_, ImGuiNodesConnectorStateFlag_Inactive) ? ImColor(0.6f, 0.6f, 0.6f, 1.0f) : ImGuiNodes::text_color_;
    ImGui::TextColored(text_color, "%s", name_.c_str());
}

void ImGuiNodesNode::TranslateNode(ImVec2 delta, bool selected_only)
{
    if (selected_only && !IS_SET(state_, ImGuiNodesNodeStateFlag_Selected))
        return;

    area_node_.Translate(delta);
    area_name_.Translate(delta);
    area_add_button_.Translate(delta);
    area_active_button_.Translate(delta);
    area_trash_button_.Translate(delta);

    for (int input_idx = 0; input_idx < inputs_.size(); ++input_idx)
        inputs_[input_idx].TranslateInput(delta);

    for (int output_idx = 0; output_idx < outputs_.size(); ++output_idx)
        outputs_[output_idx].TranslateOutput(delta);
}

ImGuiNodesNode::ImGuiNodesNode(const ImGuiNodesIdentifier& name, int color_index)
{
    name_ = name.name_;
    uid_ = name.id_;
    state_ = ImGuiNodesNodeStateFlag_Default;
    color_index_ = color_index % ImGuiNodes::color_palette_size_;

    area_name_.Min = ImVec2(0.0f, 0.0f);
    area_name_.Max = ImGui::CalcTextSize(name_.c_str());
    title_height_ = ImGuiNodesTitleHight * area_name_.GetHeight();
    
    area_add_button_ = ImRect(0.0f, 0.0f, 0.0f, 0.0f);
    area_active_button_ = ImRect(0.0f, 0.0f, 0.0f, 0.0f);
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

    float add_button_size = title_height_ * 0.6f;
    area_add_button_.Min = ImVec2(area_node_.Max.x - add_button_size * 0.3f, area_node_.Min.y + (title_height_ - add_button_size) * 0.5f);
    area_add_button_.Max = area_add_button_.Min + ImVec2(add_button_size, add_button_size);

    area_trash_button_.Min = area_node_.Min - ImVec2(add_button_size * 0.5f, add_button_size * 0.5f);
    area_trash_button_.Max = area_trash_button_.Min + ImVec2(add_button_size, add_button_size);

    area_active_button_.Min = ImVec2(area_trash_button_.Max.x + add_button_size * 0.2f, area_trash_button_.Min.y);
    area_active_button_.Max = area_active_button_.Min + ImVec2(add_button_size, add_button_size);

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

    ImColor color = ImGuiNodes::color_palette_[color_index_];
    ImColor head_color = ImColor(0.f, 0.f, 0.f, 0.15f), body_color = color;
    body_color.Value.w = 0.9f;		

    if (IS_SET(state_, ImGuiNodesNodeStateFlag_Warning))
        head_color = ImGuiNodes::warning_color_;
    
    if (IS_SET(state_, ImGuiNodesNodeStateFlag_Error))
        head_color = ImGuiNodes::error_color_;

    if (IS_SET(state_, ImGuiNodesNodeStateFlag_Inactive))
    {
        body_color = ImColor(0.5f, 0.5f, 0.5f, 0.9f);
        head_color = ImColor(0.3f, 0.3f, 0.3f, 0.5f);
    }

    const ImVec2 outline(3.0f * scale, 3.0f * scale);

    const ImDrawFlags rounding_corners_flags = ImDrawFlags_RoundCornersAll;

    if (IS_SET(state_, ImGuiNodesNodeStateFlag_Disabled))
    {
        body_color.Value.w = 0.25f;

        if (IS_SET(state_, ImGuiNodesNodeStateFlag_Collapsed))
            head_color.Value.w = 0.25f;
    }

    draw_list->AddRectFilled(node_rect.Min, node_rect.Max, body_color);

    const ImVec2 head = node_rect.GetTR() + ImVec2(0.0f, title_height_ * scale);
    draw_list->AddRectFilled(node_rect.Min, head, head_color);	

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
    ImGui::TextColored(ImGuiNodes::text_color_, "%s", name_.c_str());

    ImVec4 text_color = ImGui::GetStyle().Colors[ImGuiCol_Text];
    ImColor border_color = text_color;
    border_color.Value.w = (text_color.x > 0.5f) ? 0.2f : 0.5f;
    draw_list->AddRect(node_rect.Min - outline * 0.5f, node_rect.Max + outline * 0.5f, border_color, 0, 0, 3.0f * scale);

    if (HAS_ANY_FLAG(state_, ImGuiNodesNodeStateFlag_MarkedForSelection | ImGuiNodesNodeStateFlag_Selected))
    {
        ImColor selection_color = ImGui::GetStyle().Colors[ImGuiCol_NavHighlight];
        draw_list->AddRect(node_rect.Min - outline*1.5f, node_rect.Max + outline*1.5f, selection_color, 0, 0, 3.0f * scale);
    }

    if (IS_SET(state_, ImGuiNodesNodeStateFlag_Hovered))
    {
        ImRect active_btn_rect = area_active_button_;
        active_btn_rect.Min *= scale;
        active_btn_rect.Max *= scale;
        active_btn_rect.Translate(offset);
        ImColor btn_color = (state == ImGuiNodesState_HoveringActiveButton)
            ? ImColor(1.0f, 1.0f, 1.0f, 1.0f)
            : ImColor(0.9f, 0.9f, 0.9f, 1.0f);
        draw_list->AddRectFilled(active_btn_rect.Min, active_btn_rect.Max, btn_color, 0.0f);
        ImGui::SetWindowFontScale(scale * 0.75f);
        ImVec2 text_size = ImGui::CalcTextSize(ICON_FA_POWER_OFF);
        ImGui::SetCursorScreenPos(active_btn_rect.GetCenter() - text_size * 0.5f);
        ImGui::TextColored(ImColor(0.2f, 0.2f, 0.2f, 1.0f), ICON_FA_POWER_OFF);
        ImGui::SetWindowFontScale(scale);

        ImRect trash_btn_rect = area_trash_button_;
        trash_btn_rect.Min *= scale;
        trash_btn_rect.Max *= scale;
        trash_btn_rect.Translate(offset);
        btn_color = (state == ImGuiNodesState_HoveringTrashButton)
            ? ImColor(1.0f, 1.0f, 1.0f, 1.0f)
            : ImColor(0.9f, 0.9f, 0.9f, 1.0f);
        draw_list->AddRectFilled(trash_btn_rect.Min, trash_btn_rect.Max, btn_color, 0.0f);
        ImGui::SetWindowFontScale(scale * 0.75f);
        text_size = ImGui::CalcTextSize(ICON_FA_TRASH);
        ImGui::SetCursorScreenPos(trash_btn_rect.GetCenter() - text_size * 0.5f);
        ImGui::TextColored(ImColor(0.8f, 0.1f, 0.1f, 1.0f), ICON_FA_TRASH);
        ImGui::SetWindowFontScale(scale);

        ImRect add_button_rect = area_add_button_;
        add_button_rect.Min *= scale;
        add_button_rect.Max *= scale;
        add_button_rect.Translate(offset);
        btn_color = (state == ImGuiNodesState_HoveringAddButton)
            ? ImColor(1.0f, 1.0f, 1.0f, 1.0f)
            : ImColor(0.9f, 0.9f, 0.9f, 1.0f);
        draw_list->AddRectFilled(add_button_rect.Min, add_button_rect.Max, btn_color, 0.0f);
        ImGui::SetWindowFontScale(scale * 1.2f);
        text_size = ImGui::CalcTextSize(ICON_FA_PLUS);
        ImGui::SetCursorScreenPos(add_button_rect.GetCenter() - text_size * 0.5f);
        ImGui::TextColored(ImColor(0.2f, 0.2f, 0.2f, 1.0f), ICON_FA_PLUS);
        ImGui::SetWindowFontScale(scale);
    }
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
    minimap_preview_scale_ = 1.0f;
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
    if (auto it = nodes_by_uid_.find(uid); it != nodes_by_uid_.end())
    {
        ImGuiNodesNode* node = it->second;
        SET_FLAG(node->state_, ImGuiNodesNodeStateFlag_Warning);
        node->warning_message_ = message;
    }
}

void ImGuiNodes::SetError(const ImGuiNodesUid& uid, const std::string& message)
{
    if (auto it = nodes_by_uid_.find(uid); it != nodes_by_uid_.end())
    {
        ImGuiNodesNode* node = it->second;
        SET_FLAG(node->state_, ImGuiNodesNodeStateFlag_Error);
        node->error_message_ = message;
    }
}

void ImGuiNodes::SetOk(const ImGuiNodesUid& uid)
{
    if (auto it = nodes_by_uid_.find(uid); it != nodes_by_uid_.end())
    {
        ImGuiNodesNode* node = it->second;
        CLEAR_FLAG(node->state_, ImGuiNodesNodeStateFlag_Warning);
        CLEAR_FLAG(node->state_, ImGuiNodesNodeStateFlag_Error);
        node->warning_message_.clear();
        node->error_message_.clear();
    }
}

void ImGuiNodes::SetActive(const ImGuiNodesUid& uid, bool active)
{
    if (auto it = nodes_by_uid_.find(uid); it != nodes_by_uid_.end())
    {
        ImGuiNodesNode* node = it->second;
        if (active)
            CLEAR_FLAG(node->state_, ImGuiNodesNodeStateFlag_Inactive);
        else
            SET_FLAG(node->state_, ImGuiNodesNodeStateFlag_Inactive);
    }
    else if (auto it = outputs_by_uid_.find(uid); it != outputs_by_uid_.end())
    {
        ImGuiNodesOutput* output = it->second.output;
        if (active)
            CLEAR_FLAG(output->state_, ImGuiNodesConnectorStateFlag_Inactive);
        else
            SET_FLAG(output->state_, ImGuiNodesConnectorStateFlag_Inactive);
    }
}

void ImGuiNodes::ClearNodeConnections(const ImGuiNodesUid& node_uid)
{
    if (auto it = nodes_by_uid_.find(node_uid); it != nodes_by_uid_.end())
    {
        for (const auto& input : it->second->inputs_)
            RemoveConnection(input.uid_);
    }
}

void ImGuiNodes::Clear()
{
    active_node_ = NULL;
    active_input_ = NULL;
    active_output_ = NULL;

    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
    {
        ImGuiNodesNode* node = nodes_[node_idx];
        bool selected = IS_SET(node->state_, ImGuiNodesNodeStateFlag_Selected);
        node_cache_[node->uid_] = { node->area_node_.GetCenter(), node->color_index_, selected };
        delete node;
    }
    
    nodes_.clear();
    nodes_by_uid_.clear();
    inputs_by_uid_.clear();
    outputs_by_uid_.clear();
}

void ImGuiNodes::AddConnection(const ImGuiNodesUid& output_uid, const ImGuiNodesUid& input_uid)
{
    auto input_it = inputs_by_uid_.find(input_uid);
    auto output_it = outputs_by_uid_.find(output_uid);
    
    if (input_it != inputs_by_uid_.end() && output_it != outputs_by_uid_.end())
    {
        ImGuiNodesInput* input = input_it->second;
        ImGuiNodesOutput* output = output_it->second.output;
        ImGuiNodesNode* source_node = output_it->second.node;
        
        if (input->source_output_)
            input->source_output_->connections_count_--;
        
        input->source_node_ = source_node;
        input->source_output_ = output;
        output->connections_count_++;
    }
}

void ImGuiNodes::RemoveConnection(const ImGuiNodesUid& input_uid)
{
    if (auto input_it = inputs_by_uid_.find(input_uid); input_it != inputs_by_uid_.end())
    {
        ImGuiNodesInput* input = input_it->second;

        if (input->source_output_)
            input->source_output_->connections_count_--;

        input->source_node_ = NULL;
        input->source_output_ = NULL;
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

void ImGuiNodes::BeginBatchAdd()
{
    batch_add_mode_ = true;
}

void ImGuiNodes::EndBatchAdd()
{
    batch_add_mode_ = false;

    if (nodes_.empty())
        return;

    // If we have cached nodes, we assume the graph is being rebuilt/updated and we want to preserve positions.
    // Running the auto-layout would overwrite all positions, including the cached ones.
    if (!node_cache_.empty())
        return;

    std::unordered_map<ImGuiNodesUid, std::vector<ImGuiNodesNode*>> children_map;
    std::vector<ImGuiNodesNode*> root_nodes;

    for (int node_idx = 0; node_idx < nodes_.size(); ++node_idx)
    {
        ImGuiNodesNode* node = nodes_[node_idx];
        if (node->parent_node_)
            children_map[node->parent_node_->uid_].push_back(node);
        else
            root_nodes.push_back(node);
    }

    float horizontal_spacing = 250.0f;
    float vertical_spacing = 20.0f;
    float start_x = 100.0f;

    auto layout_tree = [&](auto& self, ImGuiNodesNode* node, float x, float y) -> float {
        std::vector<ImGuiNodesNode*>& children = children_map[node->uid_];
        
        if (children.empty())
        {
            ImVec2 target_pos(x, y + node->area_node_.GetHeight() * 0.5f);
            node->TranslateNode(target_pos - node->area_node_.GetCenter());
            return y + node->area_node_.GetHeight() + vertical_spacing;
        }

        float child_y = y;
        float first_child_center_y = 0.0f;
        float last_child_center_y = 0.0f;
        
        for (int i = 0; i < children.size(); ++i)
        {
            ImGuiNodesNode* child = children[i];
            child_y = self(self, child, x + horizontal_spacing, child_y);
            
            if (i == 0)
                first_child_center_y = child->area_node_.GetCenter().y;
            if (i == children.size() - 1)
                last_child_center_y = child->area_node_.GetCenter().y;
        }

        float center_y = (first_child_center_y + last_child_center_y) * 0.5f;
        ImVec2 target_pos(x, center_y);
        node->TranslateNode(target_pos - node->area_node_.GetCenter());
        
        return child_y;
    };

    float current_y = 100.0f;
    for (ImGuiNodesNode* root : root_nodes)
        current_y = layout_tree(layout_tree, root, start_x, current_y);
}

void ImGuiNodes::RenderMinimap(ImDrawList* draw_list)
{
    ImColor bg_color = ImGui::GetStyle().Colors[ImGuiCol_PopupBg];
    bg_color.Value.w = 0.9f;
    draw_list->AddRectFilled(minimap_rect_.Min, minimap_rect_.Max, bg_color);
    draw_list->AddRect(minimap_rect_.Min, minimap_rect_.Max, ImGui::GetColorU32(ImGuiCol_Border));

    if (nodes_.empty()) return;

    ImRect world_bounds = nodes_[0]->area_node_;
    for (auto* node : nodes_)
        world_bounds.Add(node->area_node_);

    ImRect view_rect;
    view_rect.Min = -scroll_ / scale_;
    view_rect.Max = (-scroll_ + nodes_imgui_window_size_) / scale_;
    world_bounds.Add(view_rect);

    ImVec2 world_size = world_bounds.GetSize();
    ImVec2 minimap_size = minimap_rect_.GetSize();

    if (world_size.x <= 0.0f || world_size.y <= 0.0f) return;

    float scale_x = minimap_size.x / world_size.x;
    float scale_y = minimap_size.y / world_size.y;
    float mm_scale = ImMin(scale_x, scale_y);

    // Center the content in minimap
    ImVec2 mm_content_size = world_size * mm_scale;
    ImVec2 mm_offset = minimap_rect_.Min + (minimap_size - mm_content_size) * 0.5f;

    for (auto* node : nodes_)
    {
        ImRect node_rect = node->area_node_;
        ImVec2 min = (node_rect.Min - world_bounds.Min) * mm_scale + mm_offset;
        ImVec2 max = (node_rect.Max - world_bounds.Min) * mm_scale + mm_offset;

        ImColor color = ImGuiNodes::color_palette_[node->color_index_];

        if (IS_SET(node->state_, ImGuiNodesNodeStateFlag_Warning))
            color = ImGuiNodes::warning_color_;
        if (IS_SET(node->state_, ImGuiNodesNodeStateFlag_Error))
            color = ImGuiNodes::error_color_;

        color.Value.w = 0.8f;
        if (IS_SET(node->state_, ImGuiNodesNodeStateFlag_Inactive))
            color = ImColor(0.5f, 0.5f, 0.5f);
        if (IS_SET(node->state_, ImGuiNodesNodeStateFlag_Selected))
            color = ImColor(ImMin(color.Value.x + 0.3f, 1.0f),
                            ImMin(color.Value.y + 0.3f, 1.0f),
                            ImMin(color.Value.z + 0.3f, 1.0f),
                            color.Value.w);

        draw_list->AddRectFilled(min, max, color);
    }

    {
        ImVec2 min = (view_rect.Min - world_bounds.Min) * mm_scale + mm_offset;
        ImVec2 max = (view_rect.Max - world_bounds.Min) * mm_scale + mm_offset;

        // Clamp to minimap rect
        min = ImMax(min, minimap_rect_.Min);
        max = ImMin(max, minimap_rect_.Max);

        ImColor view_color = ImGui::GetStyle().Colors[ImGuiCol_Text];
        view_color.Value.w = 0.8f;
        draw_list->AddRect(min, max, view_color);
    }

    if (minimap_rect_.Contains(mouse_))
    {
        ImVec2 target_world_center = (mouse_ - mm_offset) / mm_scale + world_bounds.Min;
        ImVec2 view_size_world = nodes_imgui_window_size_ / minimap_preview_scale_;
        ImRect target_view_rect;
        target_view_rect.Min = target_world_center - view_size_world * 0.5f;
        target_view_rect.Max = target_world_center + view_size_world * 0.5f;

        ImVec2 min = (target_view_rect.Min - world_bounds.Min) * mm_scale + mm_offset;
        ImVec2 max = (target_view_rect.Max - world_bounds.Min) * mm_scale + mm_offset;

        // Clamp to minimap rect
        min = ImMax(min, minimap_rect_.Min);
        max = ImMin(max, minimap_rect_.Max);

        ImColor target_color = ImGui::GetStyle().Colors[ImGuiCol_Text];
        target_color.Value.w = 0.4f;
        draw_list->AddRect(min, max, target_color);
    }
}


#undef IS_SET
#undef SET_FLAG
#undef CLEAR_FLAG
#undef TOGGLE_FLAG
#undef HAS_ALL_FLAGS
#undef HAS_ANY_FLAG
#undef CLEAR_FLAGS
