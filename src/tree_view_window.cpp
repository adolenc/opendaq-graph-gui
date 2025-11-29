#include "tree_view_window.h"
#include "imgui.h"
#include <string>


void TreeViewWindow::OnSelectionChanged(const std::vector<CachedComponent*>& selected_components)
{
    // return;
}

void TreeViewWindow::Render(const CachedComponent* root, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components)
{
    ImGui::Begin("Tree View", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (root)
        RenderTreeNode(root, all_components);
    ImGui::End();
}

void TreeViewWindow::RenderTreeNode(const CachedComponent* component, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components)
{
    std::string name = component->component_.getName().toStdString();
    std::string globalId = component->component_.getGlobalId().toStdString();
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DrawLinesToNodes;
    
    if (!component->children_.empty())
    {
        if (name == "Sig" || name == "IP")
            return;

        flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DefaultOpen;
        if (ImGui::TreeNodeEx(globalId.c_str(), flags, "%s", name.c_str()))
        {
            for (const auto& child_id : component->children_)
            {
                if (auto it = all_components.find(child_id.id_); it != all_components.end())
                    RenderTreeNode(it->second.get(), all_components);
            }
            ImGui::TreePop();
        }
    }
    else
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        ImGui::TreeNodeEx(globalId.c_str(), flags, "%s", name.c_str());
    }
}
