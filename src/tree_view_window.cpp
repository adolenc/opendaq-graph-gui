#include "tree_view_window.h"
#include "imgui.h"
#include "utils.h"
#include <string>

void TreeViewWindow::Render(const daq::ComponentPtr& component)
{
    ImGui::Begin("Tree View", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (component.assigned())
    {
        RenderTreeNode(component);
    }
    ImGui::End();
}

void TreeViewWindow::RenderTreeNode(const daq::ComponentPtr& component)
{
    bool is_folder = canCastTo<daq::IFolder>(component);
    daq::FolderPtr folder;
    
    if (is_folder)
    {
        folder = castTo<daq::IFolder>(component);
        if (folder.isEmpty())
            return;
    }

    std::string name = component.getName().toStdString();
    std::string globalId = component.getGlobalId().toStdString();
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DrawLinesToNodes;
    
    if (is_folder)
    {
        flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (name != "Sig" && name != "IP")
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
            
        if (ImGui::TreeNodeEx(globalId.c_str(), flags, "%s", name.c_str()))
        {
            for (const auto& item : folder.getItems())
            {
                RenderTreeNode(item);
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
