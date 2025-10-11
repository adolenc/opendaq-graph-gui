#include "signals_window.h"
#include "opendaq_control.h"
#include "imgui.h"
#include "implot.h"


void SignalsWindow::OnSelectionChanged(const std::vector<daq::ComponentPtr>& selected_components)
{
    signals_map_.clear();
    total_min_ = std::numeric_limits<float>::max();
    total_max_ = std::numeric_limits<float>::lowest();

    for (const auto& component : selected_components)
    {
        if (!component.assigned())
            continue;

        if (canCastTo<daq::IFunctionBlock>(component))
        {
            daq::FunctionBlockPtr fb = castTo<daq::IFunctionBlock>(component);
            for (const auto& signal : fb.getSignals())
            {
                std::string signal_id = signal.getGlobalId().toStdString();
                signals_map_[signal_id] = OpenDAQSignal(signal, 5.0, 2000);
            }
        }
        else if (canCastTo<daq::IDevice>(component))
        {
            daq::DevicePtr device = castTo<daq::IDevice>(component);
            for (const auto& signal : device.getSignals())
            {
                std::string signal_id = signal.getGlobalId().toStdString();
                signals_map_[signal_id] = OpenDAQSignal(signal, 5.0, 2000);
            }
        }
        else if (canCastTo<daq::ISignal>(component))
        {
            daq::SignalPtr signal = castTo<daq::ISignal>(component);
            std::string signal_id = signal.getGlobalId().toStdString();
            signals_map_[signal_id] = OpenDAQSignal(signal, 5.0, 2000);
        }
    }

    for (auto& [_, signal] : signals_map_)
    {
        total_min_ = std::min(total_min_, signal.value_range_min_);
        total_max_ = std::max(total_max_, signal.value_range_max_);
    }
}

void SignalsWindow::Render()
{
    ImGui::Begin("Signal Viewer", NULL);

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
    if (ImPlot::BeginPlot("##Signals", plot_size))
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
