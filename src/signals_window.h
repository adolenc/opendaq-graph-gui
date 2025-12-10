#pragma once
#include <opendaq/opendaq.h>
#include <unordered_map>
#include <string>
#include <functional>
#include <memory>
#include "signal.h"
#include "component_cache.h"

struct PausedSignalData
{
    std::vector<double> values_avg;
    std::vector<double> values_min;
    std::vector<double> values_max;
    std::vector<double> times_seconds;

    size_t pos_in_buffer = 0;
    size_t points_in_buffer = 0;
    double end_time_seconds = 0;
};

struct Signal
{
    OpenDAQSignal live;
    PausedSignalData paused;
};

class SignalsWindow
{
public:
    SignalsWindow() = default;
    SignalsWindow(const SignalsWindow& other);

    void Render();
    void OnSelectionChanged(const std::vector<std::string>& selected_ids, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components);
    void RestoreSelection(const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components);
    void RebuildInvalidSignals();
    
    std::function<void(SignalsWindow*)> on_clone_click_;
    std::function<void(const std::vector<std::string>&)> on_reselect_click_;
    bool is_open_ = true;

private:
    bool freeze_selection_ = false;
    bool is_cloned_ = false;
    bool is_paused_ = false;

    std::vector<std::string> selected_component_ids_;
    std::unordered_map<std::string, Signal> signals_map_;
    float total_min_ = 0.0f;
    float total_max_ = 0.0f;
    int plot_unique_id_ = 0; // id used to reset plot (especially min/max axis) whenever inputs change
};
