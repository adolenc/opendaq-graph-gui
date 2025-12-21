#pragma once
#include <opendaq/opendaq.h>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>
#include "component_cache.h"

class PropertiesWindow
{
public:
    PropertiesWindow() = default;
    PropertiesWindow(const PropertiesWindow& other);

    void Render();
    void OnSelectionChanged(const std::vector<std::string>& selected_ids, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components);
    void RefreshComponents();
    void RestoreSelection(const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components);
    
    std::function<void(PropertiesWindow*)> on_clone_click_;
    std::function<void(const std::vector<std::string>&)> on_reselect_click_;
    std::function<void(const std::string&, const std::string&)> on_property_changed_;
    bool is_open_ = true;

private:
    void RenderCachedProperty(CachedProperty& cached_prop);
    void RenderCachedComponent(CachedComponent& cached_component, bool draw_header = true, bool render_children = true);
    void RenderComponentWithParents(CachedComponent& cached_component);
    void RenderChildren(CachedComponent& cached_component);
    
    std::vector<CachedComponent*> selected_cached_components_;
    std::vector<std::string> selected_component_ids_;
    const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>* all_components_ = nullptr;
    bool freeze_selection_ = false;
    bool show_parents_and_children_ = false;
    bool tabbed_interface_ = true;
    bool show_debug_properties_ = false;
    bool is_cloned_ = false;
};
