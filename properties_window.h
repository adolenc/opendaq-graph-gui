#pragma once
#include <opendaq/opendaq.h>
#include <vector>
#include <string>
#include <memory>
#include "property_cache.h"

class PropertiesWindow
{
public:
    void Render();
    void OnSelectionChanged(const std::vector<CachedComponent*>& cached_components);
    
private:
    void RenderCachedProperty(CachedProperty& cached_prop);
    void RenderCachedComponent(CachedComponent& cached_component);
    
    std::vector<CachedComponent*> cached_components_;
    bool freeze_selection_ = false;
    bool show_parents_ = false;
    bool tabbed_interface_ = false;
    bool show_detail_properties_ = false;
};
