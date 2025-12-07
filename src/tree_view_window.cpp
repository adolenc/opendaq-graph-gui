#include "tree_view_window.h"
#include "imgui.h"
#include "utils.h"
#include <string>


void TreeViewWindow::OnSelectionChanged(const std::vector<CachedComponent*>& selected_components)
{
    selected_component_guids_.clear();
    for (CachedComponent* comp : selected_components)
        selected_component_guids_.insert(comp->component_.getGlobalId().toStdString());
}

void TreeViewWindow::Render(const CachedComponent* root, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components)
{
    ImGui::Begin("Tree View", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (root)
        RenderTreeNode(root, all_components);
    ImGui::End();
}

void TreeViewWindow::RenderTreeNode(const CachedComponent* component, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components, const CachedComponent* parent)
{
    std::string name = component->component_.getName().toStdString();
    std::string globalId = component->component_.getGlobalId().toStdString();
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DrawLinesToNodes;
    if (selected_component_guids_.find(globalId) != selected_component_guids_.end())
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
            if (ImGui::TreeNodeEx(globalId.c_str(), flags, "%s", name.c_str()))
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
        ImGui::TreeNodeEx(globalId.c_str(), flags, "%s", name.c_str());
    }
}
