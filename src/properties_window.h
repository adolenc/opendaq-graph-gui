#pragma once
#include <opendaq/opendaq.h>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>
#include "component_cache.h"


struct SharedCachedProperty : public CachedProperty
{
    bool is_multi_value_ = false;
    std::vector<CachedProperty*> target_properties_;
};

struct SharedCachedComponent
{
    explicit SharedCachedComponent(const std::vector<CachedComponent*>& components, const std::string& group_name = "");

    std::string name_;
    std::vector<SharedCachedProperty> attributes_;
    std::vector<SharedCachedProperty> properties_;
    std::vector<SharedCachedProperty> signal_descriptor_properties_;
    std::vector<SharedCachedProperty> signal_domain_descriptor_properties_;

    std::vector<CachedComponent*> source_components_;
    bool needs_refresh_ = false;
};


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

    void SaveSettings(ImGuiTextBuffer* buf)
    {
        buf->appendf("ShowParentsAndChildren=%d\n", show_parents_and_children_);
        buf->appendf("TabbedInterface=%d\n", tabbed_interface_);
        buf->appendf("ShowDebugProperties=%d\n", show_debug_properties_);
        buf->appendf("GroupComponents=%d\n", group_components_);
    }

    void LoadSettings(const char* line)
    {
        int i;
        if (sscanf(line, "ShowParentsAndChildren=%d", &i) == 1) show_parents_and_children_ = (bool)i;
        else if (sscanf(line, "TabbedInterface=%d", &i) == 1) tabbed_interface_ = (bool)i;
        else if (sscanf(line, "ShowDebugProperties=%d", &i) == 1) show_debug_properties_ = (bool)i;
        else if (sscanf(line, "GroupComponents=%d", &i) == 1) group_components_ = (bool)i;
    }

private:
    void RebuildComponents();
    
    void RenderProperty(SharedCachedProperty& cached_prop, SharedCachedComponent* owner);

    void RenderComponent(SharedCachedComponent& component, bool draw_header = true);
    void AddGroupedComponentsTooltip(SharedCachedComponent& shared_cached_component);
    void RenderComponentWithParents(SharedCachedComponent& component);
    void RenderChildren(SharedCachedComponent& component);
    
    std::vector<SharedCachedComponent> grouped_selected_components_;
    std::vector<std::string> selected_component_ids_;
    const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>* all_components_ = nullptr;
    bool freeze_selection_ = false;
    bool show_parents_and_children_ = true;
    bool tabbed_interface_ = true;
    bool show_debug_properties_ = false;
    bool is_cloned_ = false;
    bool group_components_ = false;
};
