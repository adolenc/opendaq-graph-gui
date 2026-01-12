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
    subplots_ = other.subplots_;

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

                if (subplots_.empty())
                    subplots_.emplace_back();
                subplots_[0].signal_ids.push_back(signal_id);
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
        {
            std::string id_to_remove = it->second.live.signal_id_;
            for (auto& subplot : subplots_)
            {
                auto& ids = subplot.signal_ids;
                ids.erase(std::remove(ids.begin(), ids.end(), id_to_remove), ids.end());
            }
            it = signals_map_.erase(it);
        }
        else
            ++it;
    }
    subplots_.erase(std::remove_if(subplots_.begin(), subplots_.end(),
                                   [](const Subplot& s) { return s.signal_ids.empty(); }),
                    subplots_.end());

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

    std::string title = !is_cloned_ ? std::string("Signals") : "Signals (cloned)##" + std::to_string((uintptr_t)this);
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
    float temp_seconds_shown = seconds_shown_;
    if (ImGui::InputFloat("##SecondsShown", &temp_seconds_shown, 0, 0, "%.1f s"), ImGui::IsItemDeactivatedAfterEdit())
    {
        seconds_shown_ = ImClamp(temp_seconds_shown, 0.1f, 3600.0f);
        plot_unique_id_++;
    }

    if (signals_map_.empty())
    {
        ImGui::Text("No signals found on selected components");
        ImGui::End();
        return;
    }

    for (auto& [_, signal] : signals_map_)
        signal.live.Update();

    int max_points = std::max((int)ImGui::GetIO().DisplaySize.x, 100);
    for (auto& [_, signal] : signals_map_)
        signal.live.UpdateConfiguration(seconds_shown_, max_points);

    std::string dnd_payload_name = "DND_SIGNAL" + std::to_string((uintptr_t)this);
    float drop_height = 0.0f;
    if (const ImGuiPayload* payload = ImGui::GetDragDropPayload(); payload && payload->IsDataType(dnd_payload_name.c_str()))
        drop_height = 40.0f;
    float spacing_between_subplots = ImGui::GetStyle().ItemSpacing.y;
    float total_spacing = (float)std::max((int)subplots_.size() - 1, 0) * spacing_between_subplots;
    float plot_height = (ImGui::GetContentRegionAvail().y - drop_height - total_spacing) / (float)std::max((size_t)1, subplots_.size());
    plot_height = std::max(plot_height, 150.0f);

    static ImPlotAxisFlags flags = ImPlotAxisFlags_ShowEdgeLabels;
    std::function<void()> deferred_action = nullptr;

    for (auto& subplot : subplots_)
    {
        if (ImPlot::BeginPlot(("##SignalsWindow" + std::to_string(plot_unique_id_) + "_" + std::to_string(subplot.uid)).c_str(), ImVec2(-1, plot_height)))
        {
            bool is_multi_dim = false;
            std::string x_label = "Time";
            for (const auto& id : subplot.signal_ids)
            {
                if (signals_map_.count(id) && !signals_map_[id].live.axes_.empty())
                {
                    is_multi_dim = true;
                    x_label = signals_map_[id].live.axes_[0].name_;
                    if (!signals_map_[id].live.axes_[0].unit_.empty())
                        x_label += " [" + signals_map_[id].live.axes_[0].unit_ + "]";
                    break;
                }
            }

            ImPlot::SetupAxes(x_label.c_str(), nullptr, flags, flags);
            if (!is_multi_dim)
                ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);

            double max_end_time = 0;
            float sub_min = std::numeric_limits<float>::max();
            float sub_max = std::numeric_limits<float>::lowest();
            bool has_signals = false;

            for (const auto& id : subplot.signal_ids)
            {
                if (signals_map_.find(id) == signals_map_.end()) continue;
                has_signals = true;
                auto& signal = signals_map_[id];
                if (is_paused_)
                    max_end_time = ImMax(max_end_time, signal.paused.end_time_seconds_);
                else
                    max_end_time = ImMax(max_end_time, signal.live.end_time_seconds_);

                sub_min = std::min(sub_min, signal.live.value_range_min_);
                sub_max = std::max(sub_max, signal.live.value_range_max_);
            }

            if (!has_signals) { sub_min = 0; sub_max = 1; }

            if (!is_multi_dim)
                ImPlot::SetupAxisLimits(ImAxis_X1, max_end_time - seconds_shown_, max_end_time, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, sub_min, sub_max);

            for (const auto& id : subplot.signal_ids)
            {
                if (signals_map_.find(id) == signals_map_.end()) continue;
                auto& signal = signals_map_[id];

                std::string label = signal.live.signal_name_;
                if (!signal.live.signal_unit_.empty())
                    label += " [" + signal.live.signal_unit_ + "]";
                label += "##" + signal.live.signal_id_;

                if (!signal.live.axes_.empty())
                {
                    auto& axis = signal.live.axes_[0];
                    ImPlot::SetNextLineStyle(signal.color);
                    if (std::holds_alternative<std::vector<double>>(axis.values_))
                    {
                        const auto& x_values = std::get<std::vector<double>>(axis.values_);
                        ImPlot::PlotLine(label.c_str(), x_values.data(), signal.live.plot_values_avg_.data(), (int)std::min(x_values.size(), signal.live.plot_values_avg_.size()));
                    }
                    else
                    {
                        ImPlot::PlotLine(label.c_str(), signal.live.plot_values_avg_.data(), (int)signal.live.plot_values_avg_.size());
                    }
                }
                else
                {
                    OpenDAQSignal& to_plot = is_paused_ ? signal.paused : signal.live;
                    ImPlot::SetNextLineStyle(signal.color);
                    ImPlot::PlotLine(label.c_str(), to_plot.plot_times_seconds_.data(), to_plot.plot_values_avg_.data(), (int)to_plot.points_in_plot_buffer_, ImPlotLineFlags_None, (int)to_plot.pos_in_plot_buffer_);
                    ImPlot::SetNextFillStyle(signal.color, 0.25f);
                    ImPlot::PlotShaded(label.c_str(), to_plot.plot_times_seconds_.data(), to_plot.plot_values_min_.data(), to_plot.plot_values_max_.data(), (int)to_plot.points_in_plot_buffer_, (ImPlotShadedFlags)ImPlotItemFlags_NoLegend, (int)to_plot.pos_in_plot_buffer_);
                }

                if (ImPlot::BeginDragDropSourceItem(label.c_str()))
                {
                    ImGui::SetDragDropPayload(dnd_payload_name.c_str(), id.c_str(), id.size() + 1);
                    ImGui::ColorButton("##SignalColor", signal.color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoOptions, ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()));
                    ImGui::SameLine();
                    ImGui::Text("%s%s", signal.live.signal_name_.c_str() , signal.live.signal_unit_.empty() ? "" : (" [" + signal.live.signal_unit_ + "]").c_str());
                    ImPlot::EndDragDropSource();
                }
            }

            if (ImPlot::BeginDragDropTargetPlot())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(dnd_payload_name.c_str()))
                {
                    std::string signal_id = (const char*)payload->Data;
                    int target_uid = subplot.uid;
                    deferred_action = [this, signal_id, target_uid]()
                        {
                            for (Subplot& subplot : subplots_)
                            {
                                std::vector<std::string>& ids = subplot.signal_ids;
                                ids.erase(std::remove(ids.begin(), ids.end(), signal_id), ids.end());
                            }
                            for (Subplot& subplot : subplots_)
                            {
                                if (subplot.uid == target_uid)
                                {
                                    subplot.signal_ids.push_back(signal_id);
                                    break;
                                }
                            }
                            subplots_.erase(std::remove_if(subplots_.begin(), subplots_.end(),
                                                           [](const Subplot& subplot) { return subplot.signal_ids.empty(); }),
                                            subplots_.end());
                        };
                }
                ImPlot::EndDragDropTarget();
            }

            ImPlot::EndPlot();
        }
    }

    if (drop_height > 0.0f)
    {
        ImGui::Button("Drop signal here to create a subplot", ImVec2(-1, drop_height));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* accepted_payload = ImGui::AcceptDragDropPayload(dnd_payload_name.c_str()))
            {
                std::string signal_id = (const char*)accepted_payload->Data;
                deferred_action = [this, signal_id]()
                    {
                        for (auto& s : subplots_)
                        {
                            auto& ids = s.signal_ids;
                            ids.erase(std::remove(ids.begin(), ids.end(), signal_id), ids.end());
                        }
                        subplots_.push_back(Subplot({signal_id}));
                        subplots_.erase(std::remove_if(subplots_.begin(), subplots_.end(),
                                                       [](const Subplot& s) { return s.signal_ids.empty(); }),
                                        subplots_.end());
                    };
            }
            ImGui::EndDragDropTarget();
        }
    }

    if (deferred_action)
        deferred_action();

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

