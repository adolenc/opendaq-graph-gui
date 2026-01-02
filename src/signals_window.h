#pragma once
#include <opendaq/opendaq.h>
#include <unordered_map>
#include <string>
#include <functional>
#include <memory>
#include "component_cache.h"
#include "signal.h"

struct Signal
{
    OpenDAQSignal live;
    OpenDAQSignal paused;
    ImVec4 color;
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
    void UpdateSignalColor(const std::string& signal_id, ImVec4 color);
    
    std::function<void(SignalsWindow*)> on_clone_click_;
    std::function<void(const std::vector<std::string>&)> on_reselect_click_;
    std::function<ImVec4(const std::string&)> get_signal_color_callback_;
    bool is_open_ = true;

private:
    struct Subplot {
        std::vector<std::string> signal_ids;
        int uid;

        Subplot(std::vector<std::string> ids = {}) : signal_ids(std::move(ids)) {
            static int next_uid = 0;
            uid = next_uid++;
        }
    };

    bool freeze_selection_ = false;
    bool is_cloned_ = false;
    bool is_paused_ = false;

    std::vector<std::string> selected_component_ids_;
    std::unordered_map<std::string, Signal> signals_map_;
    std::vector<Subplot> subplots_;
    float total_min_ = 0.0f;
    float total_max_ = 0.0f;
    float seconds_shown_ = 5.0f;
    int plot_unique_id_ = 0; // id used to reset plot (especially min/max axis) whenever inputs change
};
