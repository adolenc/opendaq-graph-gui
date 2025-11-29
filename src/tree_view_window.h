#pragma once
#include <opendaq/opendaq.h>
#include "component_cache.h"

class TreeViewWindow
{
public:
    void ResetRoot(CachedComponent* root) { root_ = root; }
    void Render();
    void OnSelectionChanged(const std::vector<CachedComponent*>& selected_components);

private:
    void RenderTreeNode(CachedComponent* component);

    CachedComponent* root_;
};
