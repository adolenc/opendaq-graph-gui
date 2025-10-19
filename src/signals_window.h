#pragma once
#include <opendaq/opendaq.h>
#include <unordered_map>
#include <string>
#include "signal.h"
#include "component_cache.h"


class SignalsWindow
{
public:
    void Render();
    void OnSelectionChanged(const std::vector<CachedComponent*>& cached_components);
    void RebuildInvalidSignals();
    
private:
    bool freeze_selection_ = false;
    std::unordered_map<std::string, OpenDAQSignal> signals_map_;
    float total_min_ = 0.0f;
    float total_max_ = 0.0f;
    int plot_unique_id_ = 0; // id used to reset plot (especially min/max axis) whenever inputs change
};
