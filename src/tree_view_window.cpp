#include "tree_view_window.h"
#include "imgui.h"
#include "imsearch.h"
#include "utils.h"
#include "IconsFontAwesome6.h"
#include <string>


void TreeViewWindow::OnSelectionChanged(const std::vector<std::string>& selected_ids, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components)
{
    selected_component_guids_.clear();
    for (const auto& id : selected_ids)
        selected_component_guids_.insert(id);
}

void TreeViewWindow::Render(const CachedComponent* root, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components)
{
    ImGui::Begin("Tree", nullptr);
    if (ImSearch::BeginSearch())
    {
        ImSearch::SearchBar();
        if (root)
            RenderTreeNode(root, all_components);
        ImSearch::EndSearch();
    }
    assert(pending_expansion_states_.empty());
    ImGui::End();
}

void TreeViewWindow::RenderTreeNode(const CachedComponent* component, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components, const CachedComponent* parent)
{
    std::string name = component->name_;
    std::string component_guid = component->component_.getGlobalId().toStdString();
    bool has_children = !component->children_.empty();

    if (has_children)
    {
        if (name == "Sig" || name == "IP")
            return;

        if (parent && name == "FB" && canCastTo<daq::IFunctionBlock>(parent->component_))
        {
            // skip the nested "FB" folder for function blocks, just make sure to properly propagate expansion state to immediate children
            bool propagate_state = false;
            bool expand = false;
            if (auto it = pending_expansion_states_.find(component_guid); it != pending_expansion_states_.end())
            {
                propagate_state = true;
                expand = it->second;
                pending_expansion_states_.erase(it);
            }

            for (const auto& child_id : component->children_)
            {
                if (auto it = all_components.find(child_id.id_); it != all_components.end())
                {
                    if (propagate_state && !it->second->children_.empty())
                        pending_expansion_states_[child_id.id_] = expand;

                    RenderTreeNode(it->second.get(), all_components, component);
                }
            }
            return;
        }
    }
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DrawLinesToNodes;
    if (selected_component_guids_.find(component_guid) != selected_component_guids_.end())
        flags |= ImGuiTreeNodeFlags_Selected;

    if (has_children)
        flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    else
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    
    auto render_node_logic = [this, component, &all_components, component_guid, flags, has_children](const char* label) -> bool {
        if (has_children)
        {
            if (auto it = pending_expansion_states_.find(component_guid); it != pending_expansion_states_.end())
            {
                ImGui::SetNextItemOpen(it->second, ImGuiCond_Always);
                pending_expansion_states_.erase(it);
            }
        }

        ImGuiTreeNodeFlags local_flags = flags;
        if (has_children && ImGui::IsPopupOpen(component_guid.c_str()))
            local_flags |= ImGuiTreeNodeFlags_Selected;

        bool has_color = false;
        if (!component->is_active_)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
            has_color = true;
        }
        else if (!component->error_message_.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, COLOR_ERROR);
            has_color = true;
        }
        else if (!component->warning_message_.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, COLOR_WARNING);
            has_color = true;
        }

        std::string display_label = label;
        if (component->is_locked_)
            display_label = ICON_FA_LOCK " " + display_label;

        if (!component->operation_mode_.empty())
            display_label += " [" + component->operation_mode_ + "]";

        bool open = ImGui::TreeNodeEx(component_guid.c_str(), local_flags, "%s", display_label.c_str());

        if (has_color)
            ImGui::PopStyleColor();

        if (has_color && ImGui::IsItemHovered())
        {
            if (!component->error_message_.empty())
                ImGui::SetTooltip("%s", component->error_message_.c_str());
            else if (!component->warning_message_.empty())
                ImGui::SetTooltip("%s", component->warning_message_.c_str());
        }

        if (has_children && ImGui::BeginPopupContextItem())
        {
            enum ExpandCollapseOption { None, Expand, Collapse } expand_or_collapse_triggered = ExpandCollapseOption::None;
            if (ImGui::MenuItem("Collapse children"))
                expand_or_collapse_triggered = ExpandCollapseOption::Collapse;
            if (ImGui::MenuItem("Expand children"))
                expand_or_collapse_triggered = ExpandCollapseOption::Expand;
            if (expand_or_collapse_triggered != ExpandCollapseOption::None)
            {
                for (const auto& child_id : component->children_)
                {
                    if (auto it = all_components.find(child_id.id_); it != all_components.end())
                    {
                        if (!it->second->children_.empty())
                            pending_expansion_states_[child_id.id_] = (expand_or_collapse_triggered == ExpandCollapseOption::Expand);
                    }
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Select all children"))
            {
                SelectChildrenRecursive(component, all_components);
                if (on_selection_changed_callback_)
                {
                    std::vector<std::string> selected(selected_component_guids_.begin(), selected_component_guids_.end());
                    on_selection_changed_callback_(selected);
                }
            }

            ImGui::EndPopup();
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        {
            if (on_node_double_clicked_callback_)
                on_node_double_clicked_callback_(component_guid);
        }

        if (!ImGui::IsItemToggledOpen())
            CheckTreeNodeClicked(component_guid);

        return open;
    };

    if (has_children)
    {
        bool node_open = ImSearch::PushSearchable(name.c_str(), render_node_logic);
        if (node_open)
        {
            for (const auto& child_id : component->children_)
            {
                if (auto it = all_components.find(child_id.id_); it != all_components.end())
                    RenderTreeNode(it->second.get(), all_components, component);
            }
            ImSearch::PopSearchable([](){ ImGui::TreePop(); });
        }
    }
    else
    {
        ImSearch::SearchableItem(name.c_str(), render_node_logic);
    }
}

void TreeViewWindow::SelectChildrenRecursive(const CachedComponent* component, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components)
{
    for (const auto& child_id : component->children_)
    {
        if (auto it = all_components.find(child_id.id_); it != all_components.end())
        {
            selected_component_guids_.insert(child_id.id_);
            SelectChildrenRecursive(it->second.get(), all_components);
        }
    }
}

void TreeViewWindow::CheckTreeNodeClicked(const std::string& component_guid)
{
    if (!ImGui::IsItemClicked())
        return;

    if (ImGui::GetIO().KeyCtrl)
    {
        if (selected_component_guids_.count(component_guid))
            selected_component_guids_.erase(component_guid);
        else
            selected_component_guids_.insert(component_guid);
    }
    else
    {
        selected_component_guids_.clear();
        selected_component_guids_.insert(component_guid);
    }

    if (on_selection_changed_callback_)
    {
        std::vector<std::string> selected(selected_component_guids_.begin(), selected_component_guids_.end());
        on_selection_changed_callback_(selected);
    }
}
