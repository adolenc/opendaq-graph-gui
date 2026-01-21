#include "opendaq_control.h"
#include "utils.h"
#include "ImGuiNotify.hpp"
#include "imgui_stdlib.h"
#include "implot.h"
#include "imsearch.h"
#include "signal.h"
#include "IconsFontAwesome6.h"
#include <cstdlib>
#include <ctime>
#include <chrono>


OpenDAQNodeEditor::OpenDAQNodeEditor()
    : instance_(daq::Instance("."))
{
}

#include "imgui_internal.h"

static void* PropertiesWindowSettingsHandler_ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
{
    return (void*)name;
}

static void PropertiesWindowSettingsHandler_ReadLine(ImGuiContext*, ImGuiSettingsHandler* handler, void* /*entry*/, const char* line)
{
    OpenDAQNodeEditor* editor = (OpenDAQNodeEditor*)handler->UserData;
    editor->properties_window_.LoadSettings(line);
}

static void PropertiesWindowSettingsHandler_WriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    OpenDAQNodeEditor* editor = (OpenDAQNodeEditor*)handler->UserData;
    buf->appendf("[%s][Settings]\n", handler->TypeName);
    editor->properties_window_.SaveSettings(buf);
    buf->append("\n");
}

static void* SignalsWindowSettingsHandler_ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
{
    return (void*)name;
}

static void SignalsWindowSettingsHandler_ReadLine(ImGuiContext*, ImGuiSettingsHandler* handler, void* /*entry*/, const char* line)
{
    OpenDAQNodeEditor* editor = (OpenDAQNodeEditor*)handler->UserData;
    editor->signals_window_.LoadSettings(line);
}

static void SignalsWindowSettingsHandler_WriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    OpenDAQNodeEditor* editor = (OpenDAQNodeEditor*)handler->UserData;
    buf->appendf("[%s][Settings]\n", handler->TypeName);
    editor->signals_window_.SaveSettings(buf);
    buf->append("\n");
}

static void* NodeEditorSettingsHandler_ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
{
    return (void*)name;
}

static void NodeEditorSettingsHandler_ReadLine(ImGuiContext*, ImGuiSettingsHandler* handler, void* /*entry*/, const char* line)
{
    OpenDAQNodeEditor* editor = (OpenDAQNodeEditor*)handler->UserData;
    editor->nodes_.LoadSettings(line);
}

static void NodeEditorSettingsHandler_WriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    OpenDAQNodeEditor* editor = (OpenDAQNodeEditor*)handler->UserData;
    buf->appendf("[%s][Settings]\n", handler->TypeName);
    editor->nodes_.SaveSettings(buf);
    buf->append("\n");
}

void OpenDAQNodeEditor::InitImGui()
{
    ImGuiSettingsHandler ini_handler;
    ini_handler.TypeName = "PropertiesWindow";
    ini_handler.TypeHash = ImHashStr("PropertiesWindow");
    ini_handler.ReadOpenFn = PropertiesWindowSettingsHandler_ReadOpen;
    ini_handler.ReadLineFn = PropertiesWindowSettingsHandler_ReadLine;
    ini_handler.WriteAllFn = PropertiesWindowSettingsHandler_WriteAll;
    ini_handler.UserData = this;
    ImGui::GetCurrentContext()->SettingsHandlers.push_back(ini_handler);

    ImGuiSettingsHandler ini_handler2;
    ini_handler2.TypeName = "SignalsWindow";
    ini_handler2.TypeHash = ImHashStr("SignalsWindow");
    ini_handler2.ReadOpenFn = SignalsWindowSettingsHandler_ReadOpen;
    ini_handler2.ReadLineFn = SignalsWindowSettingsHandler_ReadLine;
    ini_handler2.WriteAllFn = SignalsWindowSettingsHandler_WriteAll;
    ini_handler2.UserData = this;
    ImGui::GetCurrentContext()->SettingsHandlers.push_back(ini_handler2);

    ImGuiSettingsHandler ini_handler3;
    ini_handler3.TypeName = "NodeEditor";
    ini_handler3.TypeHash = ImHashStr("NodeEditor");
    ini_handler3.ReadOpenFn = NodeEditorSettingsHandler_ReadOpen;
    ini_handler3.ReadLineFn = NodeEditorSettingsHandler_ReadLine;
    ini_handler3.WriteAllFn = NodeEditorSettingsHandler_WriteAll;
    ini_handler3.UserData = this;
    ImGui::GetCurrentContext()->SettingsHandlers.push_back(ini_handler3);
}

void OpenDAQNodeEditor::Init()
{
    instance_.getContext().getOnCoreEvent() += [&](const daq::ComponentPtr& comp, const daq::CoreEventArgsPtr& args)
    {
        std::lock_guard<std::mutex> lock(event_mutex_);
        event_id_queue_.push_back({comp, args});
    };

    nodes_.callbacks.on_output_hover = [this](const ImGui::ImGuiNodesUid& id) { OnOutputHover(id); };
    nodes_.callbacks.on_input_hover = [this](const ImGui::ImGuiNodesUid& id) { OnInputHover(id); };
    nodes_.callbacks.on_selection_changed = [this](const std::vector<ImGui::ImGuiNodesUid>& ids) { OnSelectionChanged(ids); };
    nodes_.callbacks.on_connection_created = [this](const ImGui::ImGuiNodesUid& out_id, const ImGui::ImGuiNodesUid& in_id) { OnConnectionCreated(out_id, in_id); };
    nodes_.callbacks.on_connection_removed = [this](const ImGui::ImGuiNodesUid& id) { OnConnectionRemoved(id); };
    nodes_.callbacks.render_popup_menu = [this](ImGui::ImGuiNodes* nodes, ImVec2 pos) { RenderPopupMenu(nodes, pos); };
    nodes_.callbacks.on_add_button_click = [this](const ImGui::ImGuiNodesUid& id, std::optional<ImVec2> pos) { OnAddButtonClick(id, pos); };
    nodes_.callbacks.on_node_active_toggle = [this](const ImGui::ImGuiNodesUid& id) { OnNodeActiveToggle(id); };
    nodes_.callbacks.on_node_delete = [this](const std::vector<ImGui::ImGuiNodesUid>& ids) { OnNodeDelete(ids); };
    nodes_.callbacks.on_signal_active_toggle = [this](const ImGui::ImGuiNodesUid& id) { OnSignalActiveToggle(id); };
    nodes_.callbacks.on_input_dropped = [this](const ImGui::ImGuiNodesUid& id, std::optional<ImVec2> pos) { OnInputDropped(id, pos); };
    nodes_.callbacks.on_empty_space_click = [this](ImVec2 pos) { OnEmptySpaceClick(pos); };

    tree_view_window_.on_selection_changed_callback_ =
        [this](const std::vector<std::string>& selected_ids)
        {
            nodes_.SetSelectedNodes(selected_ids);
            OnSelectionChanged(selected_ids);
        };

    tree_view_window_.on_node_double_clicked_callback_ =
        [this](const std::string& node_id)
        {
            nodes_.SetSelectedNodes({node_id});
            nodes_.MoveSelectedNodesIntoView();
        };

    signals_window_.on_clone_click_ =
        [this](SignalsWindow* w)
        {
            auto new_window = std::make_unique<SignalsWindow>(*w);
            cloned_signals_windows_.push_back(std::move(new_window));
        };

    properties_window_.on_clone_click_ =
        [this](PropertiesWindow* w)
        {
            auto new_window = std::make_unique<PropertiesWindow>(*w);
            cloned_properties_windows_.push_back(std::move(new_window));
        };

    signals_window_.on_reselect_click_ =
    properties_window_.on_reselect_click_ =
        [this](const std::vector<std::string>& ids)
        {
            nodes_.SetSelectedNodes(ids);
            nodes_.MoveSelectedNodesIntoView();
            OnSelectionChanged(ids);
        };

    properties_window_.on_property_changed_ =
        [this](const std::string& component_id, const std::string& property_name)
        {
            if (property_name == "@SignalColor")
            {
                auto it = all_components_.find(component_id);
                if (it == all_components_.end())
                    return;
                ImVec4 color = it->second->GetSignalColor();
                signals_window_.UpdateSignalColor(component_id, color);
                for (auto& w : cloned_signals_windows_)
                    w->UpdateSignalColor(component_id, color);

                for (const auto& [input_id, cached_input] : input_ports_)
                {
                    if (cached_input->component_.assigned())
                    {
                        if (daq::InputPortPtr input_port = castTo<daq::IInputPort>(cached_input->component_); input_port.assigned())
                        {
                            daq::SignalPtr signal = input_port.getSignal();
                            if (signal.assigned() && signal.getGlobalId().toStdString() == component_id)
                                nodes_.SetConnectionColor(input_id, color);
                        }
                    }
                }
            }
            else if (property_name == "@Locked")
            {
                for (auto& [id, component] : all_components_)
                    component->UpdateState(); // refresh the Locked state because there is no core event for that one
            }
        };
}

void OpenDAQNodeEditor::UpdateSignalsActiveState(CachedComponent* cached)
{
    for (const auto& signal_id_struct : cached->output_signals_)
    {
        std::string signal_id = signal_id_struct.id_;
        if (auto it = signals_.find(signal_id); it != signals_.end())
        {
            if (it->second->component_.assigned())
            {
                bool active = it->second->component_.getActive();
                nodes_.SetActive(signal_id, active);
            }
        }
    }
}

void OpenDAQNodeEditor::RetrieveTopology(daq::ComponentPtr component, std::string parent_id, daq::ComponentPtr owner)
{
    if (component == nullptr)
        return;

    if (canCastTo<daq::IFolder>(component) && daq::FolderPtr(castTo<daq::IFolder>(component)).isEmpty())
        return;

    if (component.getName() == "IP" || component.getName() == "Sig")
        // input ports and signals are handled by checking them per component instead, so we skip them here
        return;

    CachedComponent* cached = nullptr;
    std::string component_id = component.getGlobalId().toStdString();
    if (auto it = all_components_.find(component_id); it != all_components_.end())
    {
        cached = it->second.get();
    }
    else
    {
        all_components_[component_id] = std::make_unique<CachedComponent>(component);
        cached = all_components_[component_id].get();
    }
    cached->owner_ = owner;
    cached->RefreshStructure();
    cached->children_.clear();

    if (canCastTo<daq::IFunctionBlock>(component))
    {
        daq::FunctionBlockPtr function_block = castTo<daq::IFunctionBlock>(component);
        for (const daq::InputPortPtr& input_port : function_block.getInputPorts())
        {
            std::string input_id = input_port.getGlobalId().toStdString();
            auto input_cached = std::make_unique<CachedComponent>(input_port);
            input_ports_[input_id] = input_cached.get();
            input_cached->parent_ = component;
            input_cached->owner_ = component;
            all_components_[input_id] = std::move(input_cached);
        }

        for (const daq::SignalPtr& signal : function_block.getSignals())
        {
            std::string signal_id = signal.getGlobalId().toStdString();
            auto signal_cached = std::make_unique<CachedComponent>(signal);
            signals_[signal_id] = signal_cached.get();
            signal_cached->parent_ = component;
            signal_cached->owner_ = component;
            all_components_[signal_id] = std::move(signal_cached);
        }
    }
    if (canCastTo<daq::IDevice>(component))
    {
        daq::DevicePtr device = castTo<daq::IDevice>(component);
        for (const daq::SignalPtr& signal : device.getSignals())
        {
            std::string signal_id = signal.getGlobalId().toStdString();
            auto signal_cached = std::make_unique<CachedComponent>(signal);
            signals_[signal_id] = signal_cached.get();
            signal_cached->parent_ = component;
            signal_cached->owner_ = component;
            all_components_[signal_id] = std::move(signal_cached);
        }
    }


    std::string new_parent_id = "";
    if (component == instance_ || component.getName() == "IO" || component.getName() == "AI" || component.getName() == "AO" || component.getName() == "Dev" || component.getName() == "FB")
    {
        // just a dummy folder we should skip
        assert(cached->input_ports_.empty());
        assert(cached->output_signals_.empty());
        new_parent_id = parent_id;
    }
    else
    {
        cached->parent_ = parent_id.empty() ? nullptr : folders_[parent_id]->component_;
        
        if (canCastTo<daq::IDevice>(component))
            cached->color_index_ = next_color_index_++;
        else if (!parent_id.empty())
            cached->color_index_ = folders_[parent_id]->color_index_;

        nodes_.AddNode({component.getName().toStdString(), component_id}, 
                        GetNodeColor(cached->color_index_),
                        cached->input_ports_,
                        cached->output_signals_,
                        parent_id);

        if (!component.getActive())
            nodes_.SetActive(component_id, false);

        UpdateSignalsActiveState(cached);

        cached->RefreshStatus();
        if (!cached->error_message_.empty())
            nodes_.SetError(component_id, cached->error_message_);
        else if (!cached->warning_message_.empty())
            nodes_.SetWarning(component_id, cached->warning_message_);

        new_parent_id = component_id;
        folders_[component_id] = cached;
    }

    if (canCastTo<daq::IFolder>(component))
    {
        daq::ComponentPtr new_owner = owner;
        if (canCastTo<daq::IInstance>(component) || canCastTo<daq::IDevice>(component) || canCastTo<daq::IFunctionBlock>(component))
            new_owner = component;

        daq::FolderPtr folder = castTo<daq::IFolder>(component);
        for (const auto& item : folder.getItems())
        {
            RetrieveTopology(item, new_parent_id, new_owner);
            std::string item_id = item.getGlobalId().toStdString();
            if (all_components_.find(item_id) != all_components_.end())
                cached->children_.push_back({item.getName().toStdString(), item_id});
        }
    }
}

void OpenDAQNodeEditor::RetrieveConnections()
{
    for (const auto& [input_uid, cached] : input_ports_)
    {
        daq::InputPortPtr input_port = castTo<daq::IInputPort>(cached->component_);
        if (input_port.assigned() && input_port.getSignal().assigned())
        {
            daq::SignalPtr connected_signal = input_port.getSignal();
            std::string signal_uid = connected_signal.getGlobalId().toStdString();
            ImVec4 color = ImVec4(1,1,1,1);
            if (auto it = signals_.find(signal_uid); it != signals_.end())
                color = it->second->GetSignalColor();
            nodes_.AddConnection(signal_uid, input_uid, color);
        }
    }
}

void OpenDAQNodeEditor::RebuildNodeConnections(const std::string& node_id)
{
    nodes_.ClearNodeConnections(node_id);

    if (auto it = folders_.find(node_id); it != folders_.end())
    {
        CachedComponent* cached = it->second;
        for (const auto& input_id_struct : cached->input_ports_)
        {
            std::string input_id = input_id_struct.id_;
            if (auto port_it = input_ports_.find(input_id); port_it != input_ports_.end())
            {
                daq::InputPortPtr input_port = castTo<daq::IInputPort>(port_it->second->component_);
                if (input_port.assigned() && input_port.getSignal().assigned())
                {
                    daq::SignalPtr signal = input_port.getSignal();
                    std::string signal_id = signal.getGlobalId().toStdString();
                    ImVec4 color = ImVec4(1,1,1,1);
                    if (auto sig_it = signals_.find(signal_id); sig_it != signals_.end())
                        color = sig_it->second->GetSignalColor();
                    nodes_.AddConnection(signal_id, input_id, color);
                }
            }
        }
    }
}

void OpenDAQNodeEditor::RebuildStructure()
{
    nodes_.Clear();
    all_components_.clear();
    folders_.clear();
    input_ports_.clear();
    signals_.clear();
    next_color_index_ = 1;

    nodes_.BeginBatchAdd();
    RetrieveTopology(instance_);
    nodes_.EndBatchAdd();
    RetrieveConnections();

    properties_window_.RestoreSelection(all_components_);
    for (auto& w : cloned_properties_windows_)
        w->RestoreSelection(all_components_);

    signals_window_.RestoreSelection(all_components_);
    for (auto& w : cloned_signals_windows_)
        w->RestoreSelection(all_components_);
}

void OpenDAQNodeEditor::SetNodeActiveRecursively(const std::string& node_id)
{
    if (auto it = all_components_.find(node_id); it != all_components_.end())
    {
        CachedComponent* cached = it->second.get();
        if (!cached->component_.assigned())
            return;

        cached->is_active_ = cached->component_.getActive();
        nodes_.SetActive(node_id, cached->is_active_);

        UpdateSignalsActiveState(cached);

        for (const auto& child : cached->children_)
            SetNodeActiveRecursively(child.id_);
    }
}

void OpenDAQNodeEditor::OnConnectionCreated(const ImGui::ImGuiNodesUid& output_id, const ImGui::ImGuiNodesUid& input_id)
{
    daq::SignalPtr signal;
    daq::InputPortPtr input_port;
    if (auto it = signals_.find(output_id); it != signals_.end())
        signal = castTo<daq::ISignal>(it->second->component_);
    if (auto it = input_ports_.find(input_id); it != input_ports_.end())
        input_port = castTo<daq::IInputPort>(it->second->component_);

    if (signal.assigned() && input_port.assigned())
        input_port.connect(signal);
}

void OpenDAQNodeEditor::OnConnectionRemoved(const ImGui::ImGuiNodesUid& input_id)
{
    daq::InputPortPtr input_port;
    if (auto it = input_ports_.find(input_id); it != input_ports_.end())
        input_port = castTo<daq::IInputPort>(it->second->component_);

    if (input_port.assigned())
        input_port.disconnect();
}

void OpenDAQNodeEditor::OnOutputHover(const ImGui::ImGuiNodesUid& id)
{
    static std::string last_id{""};
    static OpenDAQSignal signal_preview;

    if (id == "")
    {
        // user stopped hovering
        signal_preview = OpenDAQSignal();
        last_id = "";
        return;
    }

    if (last_id != id && signals_.find(id) != signals_.end())
    {
        // reinitialization with a new signal
        last_id = id;
        daq::SignalPtr signal = castTo<daq::ISignal>(signals_[id]->component_);
        signal_preview = OpenDAQSignal(signal, 2.0, 800);
    }

    signal_preview.Update();

    if (ImGui::BeginTooltip())
    {
        ImVec4 signal_color = ImVec4(1,1,1,1);
        if (auto it = signals_.find(id); it != signals_.end())
            signal_color = it->second->GetSignalColor();
        ImGui::ColorButton("##SignalColor", signal_color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoOptions, ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()));
        ImGui::SameLine();
        ImGui::Text("%s [%s]", signal_preview.signal_name_.c_str(), signal_preview.signal_unit_.c_str());
        
        static ImPlotAxisFlags flags = ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_ShowEdgeLabels;
        if (ImPlot::BeginPlot("##SignalPreview", ImVec2(800,300), ImPlotFlags_NoLegend))
        {
            ImPlot::SetupAxes(nullptr, nullptr, flags, flags);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -5, 5);
            ImPlot::SetupAxisLimits(ImAxis_X1, signal_preview.end_time_seconds_ - 2.0, signal_preview.end_time_seconds_, ImGuiCond_Always);
            ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
            
            ImVec4 plot_color = ImPlot::GetColormapColor(0);

            if (signal_preview.signal_type_ == SignalType::DomainAndValue)
            {
                ImPlot::SetNextFillStyle(plot_color, 0.3f);
                ImPlot::PlotShaded("Uncertain Data", signal_preview.plot_times_seconds_.data(), signal_preview.plot_values_min_.data(), signal_preview.plot_values_max_.data(), (int)signal_preview.points_in_plot_buffer_, 0, (int)signal_preview.pos_in_plot_buffer_);
            }
            else
            {
                static double dummy_ticks[] = {0};
                static const char* dummy_labels[] = {"no value"};
                ImPlot::SetupAxis(ImAxis_Y1, "", ImPlotAxisFlags_AutoFit);
                ImPlot::SetupAxisTicks(ImAxis_Y1, dummy_ticks, 1, dummy_labels, false);
            }
            ImPlot::SetNextLineStyle(plot_color);
            ImPlot::PlotLine("", signal_preview.plot_times_seconds_.data(), signal_preview.plot_values_avg_.data(), (int)signal_preview.points_in_plot_buffer_, 0, (int)signal_preview.pos_in_plot_buffer_);
            ImPlot::EndPlot();
        }
        ImGui::EndTooltip();
    }
}

void OpenDAQNodeEditor::OnInputHover(const ImGui::ImGuiNodesUid& id)
{
    if (id == "")
    {
        OnOutputHover("");
        return;
    }

    auto it = input_ports_.find(id);
    if (it == input_ports_.end())
        return;

    daq::InputPortPtr input_port = it->second->component_.as<daq::IInputPort>();
    if (!input_port.assigned())
        return;

    daq::SignalPtr signal = input_port.getSignal();
    if (!signal.assigned())
        return;

    OnOutputHover(signal.getGlobalId().toStdString());
}

void OpenDAQNodeEditor::OnSelectionChanged(const std::vector<ImGui::ImGuiNodesUid>& selected_ids)
{
    selected_ids_ = selected_ids;
    if (selected_ids.empty())
        selected_ids_.push_back(instance_.getGlobalId().toStdString());

    std::vector<CachedComponent*> selected_cached_components_;
    for (ImGui::ImGuiNodesUid id : selected_ids_)
    {
        if (auto it = folders_.find(id); it != folders_.end())
            selected_cached_components_.push_back(it->second);
        if (auto it = input_ports_.find(id); it != input_ports_.end())
            selected_cached_components_.push_back(it->second);
        if (auto it = signals_.find(id); it != signals_.end())
            selected_cached_components_.push_back(it->second);
    }
    
    properties_window_.OnSelectionChanged(selected_ids_, all_components_);
    signals_window_.OnSelectionChanged(selected_ids_, all_components_);
    tree_view_window_.OnSelectionChanged(selected_ids_, all_components_);
}

void OpenDAQNodeEditor::RenderFunctionBlockOptions(daq::ComponentPtr parent_component, const std::string& parent_id, std::optional<ImVec2> position)
{
    if (!fb_options_cache_valid_)
    {
        try
        {
            if (canCastTo<daq::IInstance>(parent_component))
            {
                daq::InstancePtr instance = castTo<daq::IInstance>(parent_component);
                cached_available_fbs_ = instance.getAvailableFunctionBlockTypes();
            }
            else if (canCastTo<daq::IDevice>(parent_component))
            {
                daq::DevicePtr device = castTo<daq::IDevice>(parent_component);
                cached_available_fbs_ = device.getAvailableFunctionBlockTypes();
            }
            else if (canCastTo<daq::IFunctionBlock>(parent_component))
            {
                daq::FunctionBlockPtr function_block = castTo<daq::IFunctionBlock>(parent_component);
                cached_available_fbs_ = function_block.getAvailableFunctionBlockTypes();
            }
            else
            {
                cached_available_fbs_ = nullptr;
            }
        }
        catch (const std::exception& e)
        {
            ImGui::InsertNotification({ImGuiToastType::Error, DEFAULT_NOTIFICATION_DURATION_MS, "Failed to get available function blocks: %s", e.what()});
            cached_available_fbs_ = nullptr;
        }
        catch (...)
        {
            ImGui::InsertNotification({ImGuiToastType::Error, DEFAULT_NOTIFICATION_DURATION_MS, "Failed to get available function blocks: Unknown error"});
            cached_available_fbs_ = nullptr;
        }
        fb_options_cache_valid_ = true;
    }

    if (!cached_available_fbs_.assigned() || cached_available_fbs_.getCount() == 0)
    {
        ImGui::Text("No function blocks available");
        return;
    }

    if (ImSearch::BeginSearch())
    {
        ImSearch::SearchBar();
        for (const auto [fb_id, desc] : cached_available_fbs_)
        {
            std::string fb_id_str = fb_id.toStdString();
            ImSearch::SearchableItem(fb_id_str.c_str(), [=](const char*) {
                if (ImGui::MenuItem(fb_id_str.c_str()))
                {
                    try
                    {
                        daq::FunctionBlockPtr fb;
                        if (canCastTo<daq::IInstance>(parent_component))
                        {
                            daq::InstancePtr instance = castTo<daq::IInstance>(parent_component);
                            fb = instance.addFunctionBlock(fb_id);
                        }
                        else if (canCastTo<daq::IDevice>(parent_component))
                        {
                            daq::DevicePtr device = castTo<daq::IDevice>(parent_component);
                            fb = device.addFunctionBlock(fb_id);
                        }
                        else if (canCastTo<daq::IFunctionBlock>(parent_component))
                        {
                            daq::FunctionBlockPtr function_block = castTo<daq::IFunctionBlock>(parent_component);
                            fb = function_block.addFunctionBlock(fb_id);
                        }

                        if (fb.assigned())
                        {
                            std::string fb_id_str = fb.getGlobalId().toStdString();

                            auto fb_cached = std::make_unique<CachedComponent>(fb);
                            fb_cached->parent_ = parent_component;
                            fb_cached->owner_ = parent_component;
                            auto parent_it = folders_.find(parent_id);
                            fb_cached->color_index_ = (parent_id.empty() || parent_it == folders_.end()) ? 0 : parent_it->second->color_index_;
                            fb_cached->RefreshStructure();

                            for (const daq::InputPortPtr& input_port : fb.getInputPorts())
                            {
                                std::string input_id = input_port.getGlobalId().toStdString();
                                auto input_cached = std::make_unique<CachedComponent>(input_port);
                                input_cached->parent_ = fb;
                                input_cached->owner_ = fb;
                                input_ports_[input_id] = input_cached.get();
                                all_components_[input_id] = std::move(input_cached);
                            }

                            for (const daq::SignalPtr& signal : fb.getSignals())
                            {
                                std::string signal_id = signal.getGlobalId().toStdString();
                                auto signal_cached = std::make_unique<CachedComponent>(signal);
                                signal_cached->parent_ = fb;
                                signal_cached->owner_ = fb;
                                signals_[signal_id] = signal_cached.get();
                                all_components_[signal_id] = std::move(signal_cached);
                            }

                            if (position.has_value())
                            {
                                nodes_.AddNode({fb.getName().toStdString(), fb_id_str},
                                                GetNodeColor(fb_cached->color_index_),
                                                position.value(),
                                                fb_cached->input_ports_,
                                                fb_cached->output_signals_,
                                                parent_id);
                            }
                            else
                            {
                                nodes_.AddNode({fb.getName().toStdString(), fb_id_str},
                                                GetNodeColor(fb_cached->color_index_),
                                                fb_cached->input_ports_,
                                                fb_cached->output_signals_,
                                                parent_id);
                            }

                            if (!fb.getActive())
                                nodes_.SetActive(fb_id_str, false);

                            UpdateSignalsActiveState(fb_cached.get());

                            folders_[fb_id_str] = fb_cached.get();
                            all_components_[fb_id_str] = std::move(fb_cached);

                            ImGui::CloseCurrentPopup();
                        }
                    }
                    catch (const std::exception& e)
                    {
                        ImGui::InsertNotification({ImGuiToastType::Error, DEFAULT_NOTIFICATION_DURATION_MS, "Failed to add function block: %s", e.what()});
                    }
                    catch (...)
                    {
                        ImGui::InsertNotification({ImGuiToastType::Error, DEFAULT_NOTIFICATION_DURATION_MS, "Failed to add function block: Unknown error"});
                    }
                }

                if (ImGui::BeginItemTooltip())
                {
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                    ImGui::TextUnformatted(desc.getDescription().toStdString().c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
            });
        }
        ImSearch::EndSearch();
    }
}

void OpenDAQNodeEditor::RenderDeviceOptions(daq::ComponentPtr parent_component, const std::string& parent_id, std::optional<ImVec2> position)
{
    if (!canCastTo<daq::IDevice>(parent_component))
        return;

    daq::DevicePtr parent_device = castTo<daq::IDevice>(parent_component);

    if (device_discovery_future_.valid())
    {
        if (device_discovery_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            try
            {
                available_devices_ = device_discovery_future_.get();
            }
            catch (const std::exception& e)
            {
                ImGui::InsertNotification({ImGuiToastType::Error, DEFAULT_NOTIFICATION_DURATION_MS, "Failed to discover devices: %s", e.what()});
                available_devices_ = nullptr;
            }
            catch (...)
            {
                ImGui::InsertNotification({ImGuiToastType::Error, DEFAULT_NOTIFICATION_DURATION_MS, "Failed to discover devices: Unknown error"});
                available_devices_ = nullptr;
            }
            last_refresh_time_ = std::chrono::steady_clock::now();
        }
    }

    bool is_discovering = device_discovery_future_.valid();
    std::string discover_button_label = (available_devices_.assigned() && available_devices_.getCount() > 0) ? "Refresh device list" : "Discover devices";
    if (is_discovering)
    {
        ImGui::BeginDisabled();
        ImGui::Button(discover_button_label.c_str());
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::Checkbox("Auto", &auto_refresh_devices_);
        ImGui::SameLine();
        ImGui::ProgressBar(-1.0f * (float)ImGui::GetTime(), ImVec2(0.0f, 0.0f), "Discovering...");
    }
    else
    {
        bool run_discovery = false;
        run_discovery |= ImGui::Button(discover_button_label.c_str());
        ImGui::SameLine();
        run_discovery |= (ImGui::Checkbox("Auto", &auto_refresh_devices_) && auto_refresh_devices_);
        run_discovery |= (auto_refresh_devices_ && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - last_refresh_time_).count() >= 1);

        if (run_discovery)
            device_discovery_future_ = std::async(std::launch::async,
                [parent_device]()
                {
                    return parent_device.getAvailableDevices();
                });
    }

    auto add_device = [&](const std::string& device_connection_string) -> bool
        {
            try
            {
                const daq::DevicePtr dev = parent_device.addDevice(device_connection_string);
                std::string dev_id = dev.getGlobalId().toStdString();

                auto dev_cached = std::make_unique<CachedComponent>(dev);
                dev_cached->parent_ = parent_component;
                dev_cached->owner_ = parent_component;
                dev_cached->color_index_ = next_color_index_++;
                dev_cached->RefreshStructure();

                if (position.has_value())
                {
                    nodes_.AddNode({dev.getName().toString(), dev_id},
                                    GetNodeColor(dev_cached->color_index_),
                                    position.value(),
                                    dev_cached->input_ports_,
                                    dev_cached->output_signals_,
                                    parent_id);
                }
                else
                {
                    nodes_.AddNode({dev.getName().toString(), dev_id},
                                    GetNodeColor(dev_cached->color_index_),
                                    dev_cached->input_ports_,
                                    dev_cached->output_signals_,
                                    parent_id);
                }

                if (!dev.getActive())
                    nodes_.SetActive(dev_id, false);

                UpdateSignalsActiveState(dev_cached.get());

                folders_[dev_id] = dev_cached.get();
                all_components_[dev_id] = std::move(dev_cached);
                return true;
            }
            catch (const std::exception& e)
            {
                ImGui::InsertNotification({ImGuiToastType::Error, DEFAULT_NOTIFICATION_DURATION_MS, "Failed to add device: %s", e.what()});
                return false;
            }
            catch (...)
            {
                ImGui::InsertNotification({ImGuiToastType::Error, DEFAULT_NOTIFICATION_DURATION_MS, "Failed to add device: Unknown error"});
                return false;
            }
        };

    if (available_devices_.assigned() && available_devices_.getCount() > 0)
    {
        for (const auto& device_info : available_devices_)
        {
            std::string device_connection_name = device_info.getName();
            std::string device_connection_string = device_info.getConnectionString();
            if (ImGui::MenuItem((device_connection_name + " (" + device_connection_string + ")").c_str()))
            {
                if (add_device(device_connection_string))
                    ImGui::CloseCurrentPopup();
            }
        }
    }

    static std::string device_connection_string = "daq.nd://";
    ImGui::InputText("##w", &device_connection_string);
    if (ImGui::IsItemDeactivatedAfterEdit() || (ImGui::SameLine(), ImGui::Button("Connect")))
    {
        if (add_device(device_connection_string))
        {
            device_connection_string = "daq.nd://";
            ImGui::CloseCurrentPopup();
        }
    }
}

void OpenDAQNodeEditor::BuildPopupParentCandidates(const std::string& parent_guid, int depth, int parent_color_index)
{
    if (all_components_.find(parent_guid) == all_components_.end())
        return;

    CachedComponent* cached = all_components_[parent_guid].get();
    if (!cached || !cached->component_.assigned())
        return;

    bool supports_adding_fbs = canCastTo<daq::IDevice>(cached->component_)
                            || canCastTo<daq::IFunctionBlock>(cached->component_);
    if (supports_adding_fbs)
    {
        daq::DictPtr<daq::IString, daq::IFunctionBlockType> available_nested_fbs;
        if (canCastTo<daq::IFunctionBlock>(cached->component_))
        {
            daq::FunctionBlockPtr fb = castTo<daq::IFunctionBlock>(cached->component_);
            available_nested_fbs = fb.getAvailableFunctionBlockTypes();
        }

        if (canCastTo<daq::IDevice>(cached->component_) || (available_nested_fbs.assigned() && available_nested_fbs.getCount() > 0))
        {
            std::string name = cached->component_.getName().toStdString();
            std::string global_id = cached->component_.getGlobalId().toStdString();
            popup_parent_candidates_.push_back({name, global_id, cached, cached->color_index_, depth});
            depth++;
        }
    }

    for (const ImGui::ImGuiNodesIdentifier &child : cached->children_)
      BuildPopupParentCandidates(child.id_, depth, parent_color_index);
}

static void CloseablePopupHeader(const std::string& label)
{
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_Header));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_Header));
    ImGui::SetNextItemOpen(true);
    bool open = true;
    ImGui::CollapsingHeader(label.c_str(), &open, ImGuiTreeNodeFlags_Leaf);
    ImGui::PopStyleColor(2);
    if (!open)
    {
        ImGui::CloseCurrentPopup();
        return;
    }
}

void OpenDAQNodeEditor::RenderPopupMenu(ImGui::ImGuiNodes* /*nodes*/, ImVec2 position)
{
    CloseablePopupHeader("Add a nested component");

    float total_width = 600.0f;
    float left_width = 240.0f;
    float child_height = 500.0f;
    if (ImGui::BeginChild("ParentTree", ImVec2(left_width, child_height), ImGuiChildFlags_None))
    {
        ImGui::TextDisabled("Parent node");
        for (const auto& candidate : popup_parent_candidates_)
        {
            if (candidate.depth > 0)
                ImGui::Indent(candidate.depth * ImGui::GetFontSize());
            ImGui::PushStyleColor(ImGuiCol_Text, GetNodeColor(candidate.color_index).Value);

            bool is_selected = popup_selected_parent_guid_ == candidate.guid;
            if (ImGui::Selectable((candidate.display_name + "###" + candidate.guid).c_str(), is_selected))
            {
                if (!is_selected)
                {
                    if (candidate.cached)
                        popup_selected_parent_guid_ = candidate.guid;
                    else
                        popup_selected_parent_guid_ = instance_.getGlobalId().toStdString();
                    fb_options_cache_valid_ = false;
                }
            }

            ImGui::PopStyleColor();
            if (candidate.depth > 0)
                ImGui::Unindent(candidate.depth * ImGui::GetFontSize());
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    if (ImGui::BeginChild("FunctionBlocks", ImVec2(total_width - left_width - 8, child_height), ImGuiChildFlags_None))
    {
        if (!popup_selected_parent_guid_.empty())
        {
            std::string parent_id = popup_selected_parent_guid_;
            daq::ComponentPtr parent_comp = nullptr;

            if (instance_.assigned() && parent_id == instance_.getGlobalId().toStdString())
                parent_comp = instance_;
            else if (auto it = all_components_.find(parent_id); it != all_components_.end())
                parent_comp = it->second->component_;

            if (parent_comp.assigned())
            {
                ImGui::TextDisabled("Add function block");
                RenderFunctionBlockOptions(parent_comp, parent_id, position);
                if (canCastTo<daq::IDevice>(parent_comp))
                {
                    ImGui::Separator();
                    ImGui::TextDisabled("Connect to device");
                    RenderDeviceOptions(parent_comp, "", position);
                }
            }
        }
        else
        {
            ImGui::Text("Select a parent");
        }
    }
    ImGui::EndChild();
}

void OpenDAQNodeEditor::OnAddButtonClick(const ImGui::ImGuiNodesUid& parent_node_id, std::optional<ImVec2> position)
{
    add_button_drop_position_ = position;
    if (auto it = folders_.find(parent_node_id); it != folders_.end())
        add_button_click_component_ = it->second->component_;
    else
        add_button_click_component_ = nullptr;

    ImGui::OpenPopup("AddNestedNodeMenu");
}

void OpenDAQNodeEditor::RenderNestedNodePopup()
{
    static bool was_context_menu_open = false;
    static bool was_nested_menu_open = false;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

    if (ImGui::BeginPopup("NodesContextMenu", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        if (!was_context_menu_open)
        {
            fb_options_cache_valid_ = false;
            popup_parent_candidates_.clear();
            BuildPopupParentCandidates(instance_.getGlobalId().toStdString());
            bool parent_selection_still_valid = false;
            for (const PopupParentCandidate& candidate : popup_parent_candidates_)
            {
                if (candidate.guid == popup_selected_parent_guid_)
                {
                    parent_selection_still_valid = true;
                    break;
                }
            }
            if (!parent_selection_still_valid)
                popup_selected_parent_guid_ = instance_.getGlobalId().toStdString();
        }
        
        RenderPopupMenu(&nodes_, add_button_drop_position_ ? add_button_drop_position_.value() : ImGui::GetMousePos());
        ImGui::EndPopup();
        was_context_menu_open = true;
        was_nested_menu_open = false;
        ImGui::PopStyleVar();
        return;
    }
    was_context_menu_open = false;

    if (ImGui::BeginPopup("AddNestedNodeMenu", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        if (!was_nested_menu_open)
            fb_options_cache_valid_ = false;

        if (add_button_click_component_ == nullptr)
        {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No component selected");
        }
        else
        {
            CloseablePopupHeader("Add a nested component");
            bool supports_fbs = canCastTo<daq::IInstance>(add_button_click_component_) || 
                                canCastTo<daq::IDevice>(add_button_click_component_) || 
                                canCastTo<daq::IFunctionBlock>(add_button_click_component_);
            bool supports_devices = canCastTo<daq::IDevice>(add_button_click_component_);

            if (supports_fbs)
            {
                ImGui::TextDisabled("Add function block");
                RenderFunctionBlockOptions(add_button_click_component_, 
                                          add_button_click_component_.getGlobalId().toStdString(),
                                          add_button_drop_position_);
            }
            if (supports_fbs && supports_devices)
                ImGui::Separator();
            if (supports_devices)
            {
                ImGui::TextDisabled("Connect to device");
                RenderDeviceOptions(add_button_click_component_, 
                                  add_button_click_component_.getGlobalId().toStdString(),
                                  add_button_drop_position_);
            }

            if (!supports_fbs && !supports_devices)
            {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "This component doesn't support nested components");
            }
        }
        
        ImGui::EndPopup();
        was_nested_menu_open = true;
        ImGui::PopStyleVar();
        return;
    }
    was_nested_menu_open = false;

    if (ImGui::BeginPopup("AddInputMenu", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        CloseablePopupHeader("Connect signal");
        if (ImSearch::BeginSearch())
        {
            ImSearch::SearchBar();

            for (const auto& e : signals_)
            {
                const std::string& id = e.first;
                CachedComponent* cached = e.second;

                std::string entry_name = cached->component_.getName().toStdString() + " (" + id + ")";
                ImSearch::SearchableItem(entry_name.c_str(), [=](const char*)
                    {
                        if (ImGui::Selectable(entry_name.c_str()))
                        {
                            daq::InputPortPtr input_port = castTo<daq::IInputPort>(dragged_input_port_component_);
                            daq::SignalPtr signal = castTo<daq::ISignal>(cached->component_);

                            if (signal.assigned() && input_port.assigned())
                                input_port.connect(signal);
                        }
                    });
            }

            ImSearch::EndSearch();
        }
        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
}

void OpenDAQNodeEditor::ShowStartupPopup()
{
    static bool show_startup_popup_ = true;
    static int current_hint_index = 0;
    static std::vector<std::string> hints;

    if (show_startup_popup_)
    {
        show_startup_popup_ = false;
        ImGui::OpenPopup("Startup");

        hints = {
            "You can auto-connect to a device on startup by using the --connection-string command line argument.",
            "Right-clicking in the Tree window allows you to quickly collapse all items or select all children of a node.",
            "Double-clicking an input port in the Nodes window will focus the connected source node.",
            "Use delete key to quickly delete selected nodes.",
            "Double-clicking any node in the Tree window will focus the Nodes window on that specific node.",
            "Holding ctrl while clicking will add components to the current selection.",
            "Left-click and drag the mouse cursor in Nodes window to create a box selection of multiple components.",
        };
        srand((unsigned int)time(nullptr));
        current_hint_index = rand() % hints.size();

        if (!is_device_discovery_initialized_)
        {
             is_device_discovery_initialized_ = true;
             daq::DevicePtr parent_device = castTo<daq::IDevice>(instance_);
             device_discovery_future_ = std::async(std::launch::async, [parent_device]() {
                 return parent_device.getAvailableDevices();
             });
        }
    }

    ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Startup", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        const char* welcome_text = "openDAQ graph GUI";
        float window_width = ImGui::GetWindowSize().x;
        float text_width = ImGui::CalcTextSize(welcome_text).x;
        ImGui::SetCursorPosX((window_width - text_width) * 0.5f);
        ImGui::Text("%s", welcome_text);

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        ImGui::Text("Getting started:");
        ImGui::Text("- Use the list below to connect to an automatically discovered device, or manually enter a connection string.");
        ImGui::Text("- Once connected, the device structure will appear in the Tree on the left, and in the Nodes.");
        ImGui::Text("- Click on components to view and edit their properties in the Properties.");
        ImGui::Text("- Click on empty space in the Nodes to add new devices or function blocks.");
        ImGui::Text("- Connect signals to input ports by dragging connections between nodes.");
        ImGui::Text("- Add nested function blocks by clicking the '+' button on device or function block nodes in the Nodes.");
        ImGui::Text("- Signals from selected components are drawn in the Signals.");

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::Text("Connect to device");
        RenderDeviceOptions(instance_, "", ImVec2(0,0));

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 5.0f));

        ImGui::TextColored(COLOR_WARNING, "%s Hint:", ICON_FA_LIGHTBULB);
        ImGui::SameLine();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(hints[current_hint_index].c_str());
        ImGui::PopTextWrapPos();

        ImGui::SameLine();

        if (ImGui::Button("One more"))
            current_hint_index = (current_hint_index + 1) % hints.size();

        ImGui::EndPopup();
    }
}

void OpenDAQNodeEditor::OnEmptySpaceClick(ImVec2 position)
{
    add_button_drop_position_ = position;
    ImGui::OpenPopup("NodesContextMenu");
}

void OpenDAQNodeEditor::OnNodeActiveToggle(const ImGui::ImGuiNodesUid& uid)
{
    if (auto it = all_components_.find(uid); it != all_components_.end())
    {
        CachedComponent* cached = it->second.get();
        if (cached->component_.assigned())
        {
            bool active = cached->component_.getActive();
            cached->component_.setActive(!active);
        }
    }
}

void OpenDAQNodeEditor::OnNodeDelete(const std::vector<ImGui::ImGuiNodesUid>& uids)
{
    for (const auto& uid : uids)
    {
        auto it = all_components_.find(uid);
        if (it == all_components_.end())
        {
            ImGui::InsertNotification({ImGuiToastType::Error, DEFAULT_NOTIFICATION_DURATION_MS, "Cannot remove component: Unknown component"});
            continue;
        }
        DeleteComponent(it->second.get());
    }
    RebuildStructure();
}

void OpenDAQNodeEditor::DeleteComponent(CachedComponent* cached_component)
{
    daq::ComponentPtr component = cached_component->component_;
    daq::ComponentPtr owner = cached_component->owner_;

    if (!component.assigned() || !owner.assigned())
    {
        ImGui::InsertNotification({ImGuiToastType::Error, DEFAULT_NOTIFICATION_DURATION_MS, "Cannot remove component: Invalid component or owner"});
        return;
    }

    try
    {
        if (canCastTo<daq::IDevice>(component))
        {
            if (canCastTo<daq::IDevice>(owner))
            {
                 daq::DevicePtr(castTo<daq::IDevice>(owner)).removeDevice(castTo<daq::IDevice>(component));
                 return;
            }
        }

        if (canCastTo<daq::IFunctionBlock>(component))
        {
            if (canCastTo<daq::IDevice>(owner))
            {
                daq::DevicePtr(castTo<daq::IDevice>(owner)).removeFunctionBlock(castTo<daq::IFunctionBlock>(component));
                return;
            }
            else if (canCastTo<daq::IFunctionBlock>(owner))
            {
                daq::FunctionBlockPtr(castTo<daq::IFunctionBlock>(owner)).removeFunctionBlock(castTo<daq::IFunctionBlock>(component));
                return;
            }
        }

        ImGui::InsertNotification({ImGuiToastType::Error, DEFAULT_NOTIFICATION_DURATION_MS, "Cannot remove component: Unsupported component type"});
    }
    catch (const daq::DaqException& e)
    {
        ImGui::InsertNotification({ImGuiToastType::Error, DEFAULT_NOTIFICATION_DURATION_MS, "Failed to remove component: %s", e.what()});
    }
    catch (...)
    {
        ImGui::InsertNotification({ImGuiToastType::Error, DEFAULT_NOTIFICATION_DURATION_MS, "Failed to remove component: Unknown error"});
    }
}

void OpenDAQNodeEditor::OnSignalActiveToggle(const ImGui::ImGuiNodesUid& uid)
{
    if (auto it = signals_.find(uid); it != signals_.end())
    {
        if (it->second->component_.assigned())
        {
            bool active = it->second->component_.getActive();
            it->second->component_.setActive(!active);
        }
    }
}

void OpenDAQNodeEditor::OnInputDropped(const ImGui::ImGuiNodesUid& input_uid, std::optional<ImVec2> /*position*/)
{
    if (auto it = input_ports_.find(input_uid); it != input_ports_.end())
        dragged_input_port_component_ = it->second->component_;
    else
        dragged_input_port_component_ = nullptr;

    ImGui::OpenPopup("AddInputMenu");
}

void OpenDAQNodeEditor::Render()
{
    {
        std::lock_guard<std::mutex> lock(event_mutex_);

        for (auto& [comp, args] : event_id_queue_)
        {
            switch (static_cast<int>(args.getEventId()))
            {
                case static_cast<int>(daq::CoreEventId::StatusChanged):
                {
                    std::string component_id = comp.getGlobalId().toStdString();
                    if (folders_.find(component_id) == folders_.end())
                    {
                        assert(false && "Received status change for unknown component");
                        break;
                    }

                    CachedComponent* cached = folders_[component_id];
                    cached->RefreshStatus();
                    if (!cached->error_message_.empty())
                    {
                        nodes_.SetError(component_id, cached->error_message_);
                        ImGui::InsertNotification({ImGuiToastType::Error, DEFAULT_NOTIFICATION_DURATION_MS, "'%s' error: %s", comp.getName().toStdString().c_str(), cached->error_message_.c_str()});
                    }
                    else if (!cached->warning_message_.empty())
                    {
                        nodes_.SetWarning(component_id, cached->warning_message_);
                        ImGui::InsertNotification({ImGuiToastType::Warning, DEFAULT_NOTIFICATION_DURATION_MS, "'%s' warning: %s", comp.getName().toStdString().c_str(), cached->warning_message_.c_str()});
                    }
                    else
                        nodes_.SetOk(component_id);

                    break;
                }
                case static_cast<int>(daq::CoreEventId::AttributeChanged):
                {
                    daq::DictPtr<daq::IString, daq::IBaseObject> params = args.getParameters();
                    if (params.hasKey("AttributeName"))
                    {
                        std::string attribute_name = params.get("AttributeName");
                        if (attribute_name == "Active")
                        {
                             std::string component_id = comp.getGlobalId().toStdString();
                             if (signals_.find(component_id) != signals_.end())
                             {
                                 bool active = (bool)params.get("Active");
                                 nodes_.SetActive(component_id, active);
                             }
                             else
                             {
                                 SetNodeActiveRecursively(component_id);
                             }
                        }
                        else if (attribute_name == "Visible" || attribute_name == "Name")
                        {
                            // yolo refresh, a massive overkill
                            RebuildStructure();
                        }
                    }
                    break;
                }
                case static_cast<int>(daq::CoreEventId::PropertyValueChanged):
                    if (args.getParameters().hasKey("Name"))
                        properties_window_.on_property_changed_(comp.getGlobalId().toStdString(), args.getParameters().get("Name").toString());
                    // fallthrough
                case static_cast<int>(daq::CoreEventId::PropertyAdded):
                case static_cast<int>(daq::CoreEventId::PropertyRemoved):
                {
                    properties_window_.FlagComponentsForResync();
                    for (auto& w : cloned_properties_windows_)
                        w->FlagComponentsForResync();
                    break;
                }
                case static_cast<int>(daq::CoreEventId::DataDescriptorChanged):
                {
                    signals_window_.RebuildInvalidSignals();
                    std::string signal_id = comp.getGlobalId().toStdString();
                    if (signals_.count(signal_id) > 0)
                        signals_[signal_id]->needs_resync_ = true;
                    break;
                }
                case static_cast<int>(daq::CoreEventId::SignalConnected):
                case static_cast<int>(daq::CoreEventId::SignalDisconnected):
                {
                    std::string input_port_id = comp.getGlobalId().toStdString();
                    if (auto it = input_ports_.find(input_port_id); it != input_ports_.end())
                    {
                        CachedComponent* input_cached = it->second;
                        if (input_cached->parent_.assigned())
                        {
                            std::string node_id = input_cached->parent_.getGlobalId().toStdString();
                            RebuildNodeConnections(node_id);
                        }
                    }
                    break;
                }
                case static_cast<int>(daq::CoreEventId::ComponentRemoved):
                {
                    daq::DictPtr<daq::IString, daq::IBaseObject> params = args.getParameters();
                    if (params.hasKey("Id"))
                    {
                        std::string removed_local_id = params.get("Id");
                        for (const auto& [id, cached] : all_components_)
                        {
                            if (cached->parent_ == comp && cached->component_.assigned() && cached->component_.getLocalId() == removed_local_id)
                            {
                                if (canCastTo<daq::IDevice>(cached->component_))
                                {
                                    std::string name = cached->component_.getName().toStdString();
                                    ImGui::InsertNotification({ImGuiToastType::Warning, DEFAULT_NOTIFICATION_DURATION_MS, "Device disconnected: %s", name.c_str()});
                                }
                                break;
                            }
                        }
                    }
                    RebuildStructure();
                    break;
                }
                case static_cast<int>(daq::CoreEventId::ComponentAdded):
                case static_cast<int>(daq::CoreEventId::ComponentUpdateEnd):
                {
                    RebuildStructure();
                    break;
                }
            }
        }
        event_id_queue_.clear();
    }

    for (auto& [id, component] : all_components_)
    {
        if (component && component->needs_resync_)
            component->RefreshProperties();
    }

    properties_window_.Render();
    for (auto it = cloned_properties_windows_.begin(); it != cloned_properties_windows_.end(); )
    {
        (*it)->Render();
        if (!(*it)->is_open_)
            it = cloned_properties_windows_.erase(it);
        else
            ++it;
    }

    signals_window_.Render();
    for (auto it = cloned_signals_windows_.begin(); it != cloned_signals_windows_.end(); )
    {
        (*it)->Render();
        if (!(*it)->is_open_)
            it = cloned_signals_windows_.erase(it);
        else
            ++it;
    }
    tree_view_window_.Render(all_components_[instance_.getGlobalId().toStdString()].get(), all_components_);

    if (ImGui::Begin("Nodes", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        nodes_.Update();
        RenderNestedNodePopup();
    }
    ImGui::End();

    for (auto& [id, component] : all_components_)
    {
        if (component)
            component->needs_resync_ = false;
    }
}
