#include "tree_view_window.h"
#include "imgui.h"
#include "utils.h"
#include <string>


void TreeViewWindow::OnSelectionChanged(const std::vector<std::string>& selected_ids, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components)
{
    selected_component_guids_.clear();
    for (const auto& id : selected_ids)
        selected_component_guids_.insert(id);
}

void TreeViewWindow::Render(const CachedComponent* root, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components)
{
    ImGui::Begin("Tree view", nullptr);
    if (root)
        RenderTreeNode(root, all_components);
    assert(pending_expansion_states_.empty());
    ImGui::End();
}

void TreeViewWindow::RenderTreeNode(const CachedComponent* component, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components, const CachedComponent* parent)
{
    std::string name = component->component_.getName().toStdString();
    std::string component_guid = component->component_.getGlobalId().toStdString();
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DrawLinesToNodes;
    if (selected_component_guids_.find(component_guid) != selected_component_guids_.end())
        flags |= ImGuiTreeNodeFlags_Selected;
    
    if (!component->children_.empty())
    {
        if (name == "Sig" || name == "IP")
            return;

        flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DefaultOpen;
        if (parent && name == "FB" && canCastTo<daq::IFunctionBlock>(parent->component_))
        {
            // skip the nested "FB" folder for function blocks
            for (const auto& child_id : component->children_)
            {
                if (auto it = all_components.find(child_id.id_); it != all_components.end())
                    RenderTreeNode(it->second.get(), all_components, component);
            }
        }
        else
        {
            if (auto it = pending_expansion_states_.find(component_guid); it != pending_expansion_states_.end())
            {
                ImGui::SetNextItemOpen(it->second, ImGuiCond_Always);
                pending_expansion_states_.erase(it);
            }

            if (ImGui::IsPopupOpen(component_guid.c_str()))
                flags |= ImGuiTreeNodeFlags_Selected;

            bool node_open = ImGui::TreeNodeEx(component_guid.c_str(), flags, "%s", name.c_str());

            if (ImGui::BeginPopupContextItem())
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

            if (node_open)
            {
                for (const auto& child_id : component->children_)
                {
                    if (auto it = all_components.find(child_id.id_); it != all_components.end())
                        RenderTreeNode(it->second.get(), all_components, component);
                }
                ImGui::TreePop();
            }
        }
    }
    else
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        ImGui::TreeNodeEx(component_guid.c_str(), flags, "%s", name.c_str());
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        {
            if (on_node_double_clicked_callback_)
                on_node_double_clicked_callback_(component_guid);
        }
        CheckTreeNodeClicked(component_guid);
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
