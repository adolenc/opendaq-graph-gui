#pragma once
#include <opendaq/opendaq.h>
#include "component_cache.h"

class TreeViewWindow
{
public:
    void Render(const CachedComponent* root, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components);
    void OnSelectionChanged(const std::vector<CachedComponent*>& selected_components);

private:
    void RenderTreeNode(const CachedComponent* component, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components);
};
