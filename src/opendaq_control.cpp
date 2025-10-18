#include "opendaq_control.h"
#include "utils.h"
#include "imgui_stdlib.h"
#include "implot.h"
#include "imsearch.h"
#include "signal.h"


OpenDAQNodeEditor::OpenDAQNodeEditor()
    : instance_(daq::Instance("."))
{
}

void OpenDAQNodeEditor::RetrieveTopology(daq::ComponentPtr component, std::string parent_id)
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
    cached->RefreshStructure();

    if (canCastTo<daq::IFunctionBlock>(component))
    {
        daq::FunctionBlockPtr function_block = castTo<daq::IFunctionBlock>(component);
        for (const daq::InputPortPtr& input_port : function_block.getInputPorts())
        {
            std::string input_id = input_port.getGlobalId().toStdString();
            auto input_cached = std::make_unique<CachedComponent>(input_port);
            input_ports_[input_id] = input_cached.get();
            input_cached->parent_ = component;
            all_components_[input_id] = std::move(input_cached);
        }

        for (const daq::SignalPtr& signal : function_block.getSignals())
        {
            std::string signal_id = signal.getGlobalId().toStdString();
            auto signal_cached = std::make_unique<CachedComponent>(signal);
            signals_[signal_id] = signal_cached.get();
            signal_cached->parent_ = component;
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

        nodes_->AddNode({component.getName().toStdString(), component_id}, 
                        cached->color_index_,
                        cached->input_ports_,
                        cached->output_signals_,
                        parent_id);
        new_parent_id = component_id;
        folders_[component_id] = cached;
    }

    if (canCastTo<daq::IFolder>(component))
    {
        daq::FolderPtr folder = castTo<daq::IFolder>(component);
        for (const auto& item : folder.getItems())
            RetrieveTopology(item, new_parent_id);
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
            nodes_->AddConnection(signal_uid, input_uid);
        }
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
        signal_preview = OpenDAQSignal(signal, 2.0, 1000);
    }

    signal_preview.Update();

    if (ImGui::BeginTooltip())
    {
        ImGui::Text("%s [%s]", signal_preview.signal_name_.c_str(), signal_preview.signal_unit_.c_str());
        
        static ImPlotAxisFlags flags = ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_ShowEdgeLabels;
        if (ImPlot::BeginPlot("##SignalPreview", ImVec2(800,300), ImPlotFlags_NoLegend))
        {
            ImPlot::SetupAxes(nullptr, nullptr, flags, flags);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -5, 5);
            ImPlot::SetupAxisLimits(ImAxis_X1, signal_preview.end_time_seconds_ - 2.0, signal_preview.end_time_seconds_, ImGuiCond_Always);
            ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
            if (signal_preview.signal_type_ == SignalType::DomainAndValue)
            {
                ImPlot::SetNextFillStyle(ImColor(0xff66ffff), 0.3);
                ImPlot::PlotShaded("Uncertain Data", signal_preview.plot_times_seconds_.data(), signal_preview.plot_values_min_.data(), signal_preview.plot_values_max_.data(), (int)signal_preview.points_in_plot_buffer_, 0, signal_preview.pos_in_plot_buffer_);
            }
            else
            {
                static double dummy_ticks[] = {0};
                static const char* dummy_labels[] = {"no value"};
                ImPlot::SetupAxis(ImAxis_Y1, "", ImPlotAxisFlags_AutoFit);
                ImPlot::SetupAxisTicks(ImAxis_Y1, dummy_ticks, 1, dummy_labels, false);
            }
            ImPlot::SetNextLineStyle(ImColor(0xff66ffff));
            ImPlot::PlotLine("", signal_preview.plot_times_seconds_.data(), signal_preview.plot_values_avg_.data(), (int)signal_preview.points_in_plot_buffer_, 0, signal_preview.pos_in_plot_buffer_);
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
    std::vector<CachedComponent*> selected_cached_components_;
    for (ImGui::ImGuiNodesUid id : selected_ids)
    {
        if (auto it = folders_.find(id); it != folders_.end())
            selected_cached_components_.push_back(it->second);
        if (auto it = input_ports_.find(id); it != input_ports_.end())
            selected_cached_components_.push_back(it->second);
        if (auto it = signals_.find(id); it != signals_.end())
            selected_cached_components_.push_back(it->second);
    }
    
    properties_window_.OnSelectionChanged(selected_cached_components_);
    signals_window_.OnSelectionChanged(selected_cached_components_);
}

void OpenDAQNodeEditor::RenderFunctionBlockOptions(daq::ComponentPtr parent_component, const std::string& parent_id, ImVec2 position)
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
        catch (...)
        {
            cached_available_fbs_ = nullptr;
        }
        fb_options_cache_valid_ = true;
    }

    if (!cached_available_fbs_.assigned() || cached_available_fbs_.getCount() == 0)
    {
        ImGui::Text("No function blocks available");
        return;
    }

    for (const auto [fb_id, desc] : cached_available_fbs_)
    {
        if (ImGui::MenuItem(fb_id.toStdString().c_str()))
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
                fb_cached->color_index_ = parent_id.empty() ? 0 : folders_[parent_id]->color_index_;
                fb_cached->RefreshStructure();
                
                for (const daq::InputPortPtr& input_port : fb.getInputPorts())
                {
                    std::string input_id = input_port.getGlobalId().toStdString();
                    auto input_cached = std::make_unique<CachedComponent>(input_port);
                    input_cached->parent_ = fb;
                    input_ports_[input_id] = input_cached.get();
                    all_components_[input_id] = std::move(input_cached);
                }

                for (const daq::SignalPtr& signal : fb.getSignals())
                {
                    std::string signal_id = signal.getGlobalId().toStdString();
                    auto signal_cached = std::make_unique<CachedComponent>(signal);
                    signal_cached->parent_ = fb;
                    signals_[signal_id] = signal_cached.get();
                    all_components_[signal_id] = std::move(signal_cached);
                }

                nodes_->AddNode({fb.getName().toStdString(), fb_id_str},
                                fb_cached->color_index_,
                                position,
                                fb_cached->input_ports_,
                                fb_cached->output_signals_,
                                parent_id);

                folders_[fb_id_str] = fb_cached.get();
                all_components_[fb_id_str] = std::move(fb_cached);
            }
        }

        if (ImGui::BeginItemTooltip())
        {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc.getDescription().toStdString().c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }
}

void OpenDAQNodeEditor::RenderDeviceOptions(daq::ComponentPtr parent_component, const std::string& parent_id, ImVec2 position)
{
    if (!canCastTo<daq::IDevice>(parent_component))
        return;

    daq::DevicePtr parent_device = castTo<daq::IDevice>(parent_component);

    if (ImGui::Button(available_devices_.assigned() && available_devices_.getCount() > 0 ? "Refresh devices" : "Discover devices"))
    {
        try
        {
            available_devices_ = parent_device.getAvailableDevices();
        }
        catch (...)
        {
            available_devices_ = nullptr;
        }
    }

    auto add_device = [&](const std::string& device_connection_string)
        {
            const daq::DevicePtr dev = parent_device.addDevice(device_connection_string);
            std::string dev_id = dev.getGlobalId().toStdString();

            auto dev_cached = std::make_unique<CachedComponent>(dev);
            dev_cached->parent_ = parent_component;
            dev_cached->color_index_ = next_color_index_++;
            dev_cached->RefreshStructure();
            
            nodes_->AddNode({dev.getName().toString(), dev_id}, 
                            dev_cached->color_index_, 
                            position,
                            dev_cached->input_ports_,
                            dev_cached->output_signals_,
                            parent_id);

            folders_[dev_id] = dev_cached.get();
            all_components_[dev_id] = std::move(dev_cached);
        };

    if (available_devices_.assigned() && available_devices_.getCount() > 0)
    {
        for (const auto& device_info : available_devices_)
        {
            std::string device_connection_name = device_info.getName();
            std::string device_connection_string = device_info.getConnectionString();
            if (ImGui::MenuItem((device_connection_name + " (" + device_connection_string + ")").c_str()))
            {
                add_device(device_connection_string);
                ImGui::CloseCurrentPopup();
            }
        }
    }

    std::string device_connection_string = "daq.nd://";
    ImGui::InputText("##w", &device_connection_string);
    if (ImGui::IsItemDeactivatedAfterEdit() || (ImGui::SameLine(), ImGui::Button("Connect")))
    {
        add_device(device_connection_string);
        ImGui::CloseCurrentPopup();
    }
}

void OpenDAQNodeEditor::RenderPopupMenu(ImGui::ImGuiNodes* nodes, ImVec2 position)
{
    ImGui::SeparatorText("Add a function block");
    RenderFunctionBlockOptions(instance_, "", position);

    ImGui::SeparatorText("Connect to device");
    RenderDeviceOptions(instance_, "", position);
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
            fb_options_cache_valid_ = false;
        
        RenderPopupMenu(nodes_, add_button_drop_position_ ? add_button_drop_position_.value() : ImGui::GetMousePos());
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
            bool supports_fbs = canCastTo<daq::IInstance>(add_button_click_component_) || 
                                canCastTo<daq::IDevice>(add_button_click_component_) || 
                                canCastTo<daq::IFunctionBlock>(add_button_click_component_);
            bool supports_devices = canCastTo<daq::IDevice>(add_button_click_component_);

            if (supports_fbs)
            {
                ImGui::SeparatorText("Add function block");
                RenderFunctionBlockOptions(add_button_click_component_, 
                                          add_button_click_component_.getGlobalId().toStdString(),
                                          add_button_drop_position_ ? add_button_drop_position_.value() : ImVec2(0, 0));
            }

            if (supports_devices)
            {
                ImGui::SeparatorText("Connect to device");
                RenderDeviceOptions(add_button_click_component_, 
                                  add_button_click_component_.getGlobalId().toStdString(),
                                  add_button_drop_position_ ? add_button_drop_position_.value() : ImVec2(0, 0));
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
    if (show_startup_popup_)
    {
        show_startup_popup_ = false;
        ImGui::OpenPopup("Startup");
    }
    
    ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Startup", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::Text("Welcome to openDAQ GUI!");
        ImGui::Text("Connect to a device or add function blocks to get started.");
        ImGui::Separator();

        RenderPopupMenu(nodes_, ImVec2(0,0));
        ImGui::Separator();

        if (ImGui::Button("Dismiss"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void OpenDAQNodeEditor::OnEmptySpaceClick(ImVec2 position)
{
    add_button_drop_position_ = position;
    ImGui::OpenPopup("NodesContextMenu");
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
    properties_window_.Render();
    signals_window_.Render();
}
