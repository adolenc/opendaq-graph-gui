#pragma once
#include <opendaq/opendaq.h>
#include <vector>
#include <functional>
#include "component_cache.h"

class PropertiesWindow
{
public:
    PropertiesWindow() = default;
    PropertiesWindow(const PropertiesWindow& other);

    void Render();
    void OnSelectionChanged(const std::vector<CachedComponent*>& cached_components);
    void RefreshComponents();
    
    std::function<void(PropertiesWindow*)> on_clone_click_;
    bool is_open_ = true;

private:
    void RenderCachedProperty(CachedProperty& cached_prop);
    void RenderCachedComponent(CachedComponent& cached_component);
    
    std::vector<CachedComponent*> cached_components_;
    bool freeze_selection_ = false;
    bool show_parents_ = false;
    bool tabbed_interface_ = true;
    bool show_detail_properties_ = false;
    bool is_cloned_ = false;
};
