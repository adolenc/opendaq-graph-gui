#include "tree_view_window.h"
#include "imgui.h"
#include "utils.h"
#include <string>


void TreeViewWindow::OnSelectionChanged(const std::vector<CachedComponent*>& selected_components)
{
    // return;
}

void TreeViewWindow::Render()
{
    ImGui::Begin("Tree View", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (root_)
        RenderTreeNode(root_);
    ImGui::End();
}

void TreeViewWindow::RenderTreeNode(CachedComponent* component)
{
    bool is_folder = canCastTo<daq::IFolder>(component->component_);

    std::string name = component->component_.getName().toStdString();
    std::string globalId = component->component_.getGlobalId().toStdString();
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DrawLinesToNodes;
    
    if (is_folder && !component->children_.empty())
    {
        if (name == "Sig" || name == "IP")
            return;

        flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DefaultOpen;
        if (ImGui::TreeNodeEx(globalId.c_str(), flags, "%s", name.c_str()))
        {
            for (auto* child : component->children_)
                RenderTreeNode(child);
            ImGui::TreePop();
        }
    }
    else
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        ImGui::TreeNodeEx(globalId.c_str(), flags, "%s", name.c_str());
    }
}
