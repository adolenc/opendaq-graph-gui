#pragma once
#include <opendaq/opendaq.h>
#include <unordered_map>
#include <string>
#include <functional>
#include "signal.h"
#include "component_cache.h"


class SignalsWindow
{
public:
    SignalsWindow() = default;
    SignalsWindow(const SignalsWindow& other);

    void Render();
    void OnSelectionChanged(const std::vector<CachedComponent*>& cached_components);
    void RebuildInvalidSignals();
    
    std::function<void(SignalsWindow*)> on_clone_click_;
    bool is_open_ = true;

private:
    bool freeze_selection_ = false;
    bool is_cloned_ = false;

    std::unordered_map<std::string, OpenDAQSignal> signals_map_;
    float total_min_ = 0.0f;
    float total_max_ = 0.0f;
    int plot_unique_id_ = 0; // id used to reset plot (especially min/max axis) whenever inputs change
};
