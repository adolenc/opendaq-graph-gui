#include "signals_window.h"
#include "utils.h"
#include "IconsFontAwesome6.h"
#include "imgui.h"
#include "implot.h"
#include <unordered_set>


SignalsWindow::SignalsWindow(const SignalsWindow& other)
{
    for (const auto& [id, signal] : other.signals_map_)
    {
        signals_map_[id] = { OpenDAQSignal(signal.live.signal_, other.seconds_shown_), OpenDAQSignal(), signal.color };
    }

    is_cloned_ = true;
    freeze_selection_ = true;
    selected_component_ids_ = other.selected_component_ids_;

    total_min_ = other.total_min_;
    total_max_ = other.total_max_;
    seconds_shown_ = other.seconds_shown_;
    plot_unique_id_ = other.plot_unique_id_;
    on_reselect_click_ = other.on_reselect_click_;
    get_signal_color_callback_ = other.get_signal_color_callback_;
}

void SignalsWindow::OnSelectionChanged(const std::vector<std::string>& selected_ids, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components)
{
    if (freeze_selection_)
        return;

    selected_component_ids_ = selected_ids;
    RestoreSelection(all_components);
}

void SignalsWindow::RestoreSelection(const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components)
{
    std::unordered_set<std::string> selected_signal_ids;

    auto add_signal = [&](const daq::SignalPtr& signal)
    {
        std::string signal_id = signal.getGlobalId().toStdString();
        selected_signal_ids.insert(signal_id);
        if (signals_map_.find(signal_id) == signals_map_.end())
        {
            ImVec4 color = ImVec4(1,1,1,1);
            if (auto it = all_components.find(signal_id); it != all_components.end())
            {
                if (it->second->signal_color_.has_value())
                    color = it->second->signal_color_.value();
                else if (get_signal_color_callback_)
                    color = get_signal_color_callback_(signal_id);
            }
            else if (get_signal_color_callback_)
            {
                color = get_signal_color_callback_(signal_id);
            }

            signals_map_[signal_id] = { OpenDAQSignal(signal, seconds_shown_), OpenDAQSignal(), color };
        }
    };

    for (const auto& id : selected_component_ids_)
    {
        auto it = all_components.find(id);
        if (it == all_components.end())
            continue;

        const CachedComponent* cached = it->second.get();
        if (!cached || !cached->component_.assigned())
            continue;

        const daq::ComponentPtr component = cached->component_;

        if (canCastTo<daq::IFunctionBlock>(component))
        {
            daq::FunctionBlockPtr fb = castTo<daq::IFunctionBlock>(component);
            for (const auto& signal : fb.getSignals())
                add_signal(signal);
        }
        else if (canCastTo<daq::IDevice>(component))
        {
            daq::DevicePtr device = castTo<daq::IDevice>(component);
            for (const auto& signal : device.getSignals())
                add_signal(signal);
        }
        else if (canCastTo<daq::IInputPort>(component))
        {
            daq::InputPortPtr port = castTo<daq::IInputPort>(component);
            if (port.getSignal().assigned())
                add_signal(port.getSignal());
        }
        else if (canCastTo<daq::ISignal>(component))
        {
            add_signal(castTo<daq::ISignal>(component));
        }
    }

    for (auto it = signals_map_.begin(); it != signals_map_.end(); )
    {
        if (std::find(selected_signal_ids.begin(), selected_signal_ids.end(), it->second.live.signal_id_) == selected_signal_ids.end())
            it = signals_map_.erase(it);
        else
            ++it;
    }

    total_min_ = std::numeric_limits<float>::max();
    total_max_ = std::numeric_limits<float>::lowest();
    for (auto& [_, signal] : signals_map_)
    {
        total_min_ = std::min(total_min_, signal.live.value_range_min_);
        total_max_ = std::max(total_max_, signal.live.value_range_max_);
    }
    plot_unique_id_ += 1;
}

void SignalsWindow::Render()
{
    ImGui::SetNextWindowPos(ImVec2(500.f, 20.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800.f, 500.f), ImGuiCond_FirstUseEver);

    std::string title = !is_cloned_ ? std::string("Signal viewer") : "Signal viewer (cloned)##" + std::to_string((uintptr_t)this);
    if (!ImGui::Begin(title.c_str(), is_cloned_ ? &is_open_ : nullptr))
    {
        ImGui::End();
        return;
    }

    if (is_cloned_)
    {
        if (ImGui::Button(ICON_FA_ARROWS_ROTATE))
        {
            if (on_reselect_click_)
                on_reselect_click_(selected_component_ids_);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Reapply selection");
        ImGui::SameLine();
    }

    if (!is_cloned_)
    {
        if (ImGui::Button(freeze_selection_ ? " " ICON_FA_LOCK " ": " " ICON_FA_LOCK_OPEN))
            freeze_selection_ = !freeze_selection_;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(freeze_selection_ ? "Unlock selection" : "Lock selection");
        ImGui::SameLine();
        ImGui::BeginDisabled(signals_map_.empty());
        if (ImGui::Button(ICON_FA_CLONE))
        {
            if (on_clone_click_)
                on_clone_click_(this);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Clone into a new window");
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    ImGui::BeginDisabled(signals_map_.empty());
    if (ImGui::Button(is_paused_ ? ICON_FA_CIRCLE_PLAY : ICON_FA_CIRCLE_PAUSE))
    {
        is_paused_ = !is_paused_;
        if (is_paused_)
        {
            for (auto& [_, signal] : signals_map_)
                signal.paused = signal.live;
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(is_paused_ ? "Resume updating signals" : "Pause updating signals");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::DragFloat("##SecondsShown", &seconds_shown_, 0.1f, 0.1f, 600.0f, "%.1f s"))
        plot_unique_id_++;

    if (signals_map_.empty())
    {
        ImGui::Text("No signals available in selected components");
        ImGui::End();
        return;
    }

    for (auto& [_, signal] : signals_map_)
        signal.live.Update();

    ImVec2 plot_size = ImGui::GetContentRegionAvail();
    plot_size.y = ImMax(plot_size.y, 400.0f);

    int max_points = std::max((int)ImGui::GetIO().DisplaySize.x, 100);
    for (auto& [_, signal] : signals_map_)
        signal.live.UpdateConfiguration(seconds_shown_, max_points);

    static ImPlotAxisFlags flags = ImPlotAxisFlags_ShowEdgeLabels;
    if (ImPlot::BeginPlot(("##SignalsWindow" + std::to_string(plot_unique_id_)).c_str(), plot_size))
    {
        ImPlot::SetupAxes("Time", nullptr, flags, flags);

        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        double max_end_time = 0;
        for (auto& [_, signal] : signals_map_)
        {
            if (is_paused_)
                max_end_time = ImMax(max_end_time, signal.paused.end_time_seconds_);
            else
                max_end_time = ImMax(max_end_time, signal.live.end_time_seconds_);
        }
        ImPlot::SetupAxisLimits(ImAxis_X1, max_end_time - seconds_shown_, max_end_time, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, total_min_, total_max_);
        for (auto& [_, signal] : signals_map_)
        {
            std::string label = signal.live.signal_name_;
            if (!signal.live.signal_unit_.empty())
                label += " [" + signal.live.signal_unit_ + "]";

            if (is_paused_)
            {
                ImPlot::SetNextLineStyle(signal.color);
                ImPlot::PlotLine(label.c_str(), signal.paused.plot_times_seconds_.data(), signal.paused.plot_values_avg_.data(), (int)signal.paused.points_in_plot_buffer_, 0, signal.paused.pos_in_plot_buffer_);
                ImPlot::SetNextFillStyle(signal.color, 0.25f);
                ImPlot::PlotShaded(label.c_str(), signal.paused.plot_times_seconds_.data(), signal.paused.plot_values_min_.data(), signal.paused.plot_values_max_.data(), (int)signal.paused.points_in_plot_buffer_, (ImPlotShadedFlags)ImPlotItemFlags_NoLegend, signal.paused.pos_in_plot_buffer_);
            }
            else
            {
                ImPlot::SetNextLineStyle(signal.color);
                ImPlot::PlotLine(label.c_str(), signal.live.plot_times_seconds_.data(), signal.live.plot_values_avg_.data(), (int)signal.live.points_in_plot_buffer_, 0, signal.live.pos_in_plot_buffer_);
                ImPlot::SetNextFillStyle(signal.color, 0.25f);
                ImPlot::PlotShaded(label.c_str(), signal.live.plot_times_seconds_.data(), signal.live.plot_values_min_.data(), signal.live.plot_values_max_.data(), (int)signal.live.points_in_plot_buffer_, (ImPlotShadedFlags)ImPlotItemFlags_NoLegend, signal.live.pos_in_plot_buffer_);
            }
        }

        ImPlot::EndPlot();
    }

    ImGui::End();
}

void SignalsWindow::RebuildInvalidSignals()
{
    for (auto& [id, sig] : signals_map_)
        sig.live.RebuildIfInvalid();

    total_min_ = std::numeric_limits<float>::max();
    total_max_ = std::numeric_limits<float>::lowest();
    for (auto& [_, signal] : signals_map_)
    {
        total_min_ = std::min(total_min_, signal.live.value_range_min_);
        total_max_ = std::max(total_max_, signal.live.value_range_max_);
    }
    plot_unique_id_ += 1;
}

void SignalsWindow::UpdateSignalColor(const std::string& signal_id, ImVec4 color)
{
    if (auto it = signals_map_.find(signal_id); it != signals_map_.end())
        it->second.color = color;
}

