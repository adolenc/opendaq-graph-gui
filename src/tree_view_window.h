#pragma once
#include <opendaq/opendaq.h>
#include "component_cache.h"
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <memory>

class TreeViewWindow
{
public:
    void Render(const CachedComponent* root, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components);
    void OnSelectionChanged(const std::vector<std::string>& selected_ids, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components);

    std::function<void(const std::vector<std::string>&)> on_selection_changed_callback_;
    std::function<void(const std::string&)> on_node_double_clicked_callback_;

private:
    void RenderTreeNode(const CachedComponent* component,
                        const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components,
                        const CachedComponent* parent = nullptr);
    void CheckTreeNodeClicked(const std::string& component_guid);

    std::unordered_set<std::string> selected_component_guids_;
    std::unordered_map<std::string, bool> pending_expansion_states_;
};
