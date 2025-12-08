#include "signals_window.h"
#include "utils.h"
#include "imgui.h"
#include "implot.h"
#include <unordered_set>


void SignalsWindow::OnSelectionChanged(const std::vector<CachedComponent*>& cached_components)
{
    if (freeze_selection_)
        return;

    std::unordered_set<std::string> selected_signal_ids;

    auto add_signal = [&](const daq::SignalPtr& signal)
    {
        std::string signal_id = signal.getGlobalId().toStdString();
        selected_signal_ids.insert(signal_id);
        if (signals_map_.find(signal_id) == signals_map_.end())
            signals_map_[signal_id] = OpenDAQSignal(signal, 5.0, 5000);
    };

    for (const CachedComponent* cached : cached_components)
    {
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
        if (std::find(selected_signal_ids.begin(), selected_signal_ids.end(), it->second.signal_id_) == selected_signal_ids.end())
            it = signals_map_.erase(it);
        else
            ++it;
    }

    total_min_ = std::numeric_limits<float>::max();
    total_max_ = std::numeric_limits<float>::lowest();
    for (auto& [_, signal] : signals_map_)
    {
        total_min_ = std::min(total_min_, signal.value_range_min_);
        total_max_ = std::max(total_max_, signal.value_range_max_);
    }
    plot_unique_id_ += 1;
}

void SignalsWindow::Render()
{
    ImGui::SetNextWindowPos(ImVec2(500.f, 20.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800.f, 500.f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Signal Viewer");

    if (ImGui::Button(freeze_selection_ ? " " ICON_FA_LOCK " ": " " ICON_FA_LOCK_OPEN))
        freeze_selection_ = !freeze_selection_;
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(freeze_selection_ ? "Unlock selection" : "Lock selection");
        ImGui::EndTooltip();
    }

    if (signals_map_.empty())
    {
        ImGui::Text("No signals available in selected components");
        ImGui::End();
        return;
    }

    for (auto& [_, signal] : signals_map_)
        signal.Update();

    ImVec2 plot_size = ImGui::GetContentRegionAvail();
    plot_size.y = ImMax(plot_size.y, 400.0f);

    static ImPlotAxisFlags flags = ImPlotAxisFlags_ShowEdgeLabels;
    if (ImPlot::BeginPlot(("##SignalsWindow" + std::to_string(plot_unique_id_)).c_str(), plot_size))
    {
        ImPlot::SetupAxes("Time", nullptr, flags, flags);

        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        double max_end_time = 0;
        for (auto& [_, signal] : signals_map_)
            max_end_time = ImMax(max_end_time, signal.end_time_seconds_);
        ImPlot::SetupAxisLimits(ImAxis_X1, max_end_time - 5.0, max_end_time, ImGuiCond_Always);

        ImPlot::SetupAxisLimits(ImAxis_Y1, total_min_, total_max_);

        for (auto& [_, signal] : signals_map_)
        {
            std::string label = signal.signal_name_;
            if (!signal.signal_unit_.empty())
                label += " [" + signal.signal_unit_ + "]";
                
            // ImPlot::SetNextFillStyle(ImColor(0xff66ffff), 0.3);
            ImPlot::PlotShaded(label.c_str(), signal.plot_times_seconds_.data(), signal.plot_values_min_.data(), signal.plot_values_max_.data(), (int)signal.points_in_plot_buffer_, 0, signal.pos_in_plot_buffer_);
            // ImPlot::SetNextLineStyle(ImColor(0xff66ffff));
            ImPlot::PlotLine(label.c_str(), signal.plot_times_seconds_.data(), signal.plot_values_avg_.data(), (int)signal.points_in_plot_buffer_, 0, signal.pos_in_plot_buffer_);
        }

        ImPlot::EndPlot();
    }

    ImGui::End();
}

void SignalsWindow::RebuildInvalidSignals()
{
    for (auto& [id, sig] : signals_map_)
        sig.RebuildIfInvalid();

    total_min_ = std::numeric_limits<float>::max();
    total_max_ = std::numeric_limits<float>::lowest();
    for (auto& [_, signal] : signals_map_)
    {
        total_min_ = std::min(total_min_, signal.value_range_min_);
        total_max_ = std::max(total_max_, signal.value_range_max_);
    }
    plot_unique_id_ += 1;
}
