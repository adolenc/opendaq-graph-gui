#include "opendaq_control.h"
#include "imgui_stdlib.h"
#include "implot.h"
#include "imsearch.h"


OpenDAQHandler::OpenDAQHandler()
    : instance_(daq::Instance("."))
{
}

void OpenDAQHandler::RetrieveTopology(daq::ComponentPtr component, ImGui::ImGuiNodes& nodes, std::string parent_id)
{
    if (component == nullptr)
        return;

    if (canCastTo<daq::IFolder>(component) && daq::FolderPtr(castTo<daq::IFolder>(component)).isEmpty())
        return;

    if (component.getName() == "IP" || component.getName() == "Sig")
        return;

    std::vector<ImGui::ImGuiNodesIdentifier> input_ports;
    std::vector<ImGui::ImGuiNodesIdentifier> output_signals;
    if (canCastTo<daq::IFunctionBlock>(component))
    {
        daq::FunctionBlockPtr function_block = castTo<daq::IFunctionBlock>(component);
        for (const daq::InputPortPtr& input_port : function_block.getInputPorts())
        {
            input_ports.push_back({input_port.getName().toStdString(), input_port.getGlobalId().toStdString()});
            input_ports_[input_port.getGlobalId().toStdString()] = {input_port, component};
        }

        for (const daq::SignalPtr& signal : function_block.getSignals())
        {
            signals_[signal.getGlobalId().toStdString()] = {signal, component};
            output_signals.push_back({signal.getName().toStdString(), signal.getGlobalId().toStdString()});
        }
    }
    if (canCastTo<daq::IDevice>(component))
    {
        daq::DevicePtr device = castTo<daq::IDevice>(component);
        for (const daq::SignalPtr& signal : device.getSignals())
        {
            signals_[signal.getGlobalId().toStdString()] = {signal, component};
            output_signals.push_back({signal.getName().toStdString(), signal.getGlobalId().toStdString()});
        }
    }


    std::string new_parent_id = "";
    if (component == instance_ || component.getName() == "IO" || component.getName() == "AI" || component.getName() == "AO" || component.getName() == "Dev" || component.getName() == "FB")
    {
        // just a dummy folder we should skip
        assert(input_ports.empty());
        assert(output_signals.empty());
        new_parent_id = parent_id;
    }
    else
    {
        OpenDAQComponent c;
        c.component_ = component;
        c.parent_ = parent_id.empty() ? nullptr : folders_[parent_id].component_;
        
        if (canCastTo<daq::IDevice>(component))
        {
            c.color_index_ = next_color_index_;
            next_color_index_ = (next_color_index_ + 1) % color_palette_size_;
        }
        else if (!parent_id.empty())
        {
            c.color_index_ = folders_[parent_id].color_index_;
        }

        nodes.AddNode({component.getName().toStdString(), component.getGlobalId().toStdString()}, 
                      color_palette_[c.color_index_],
                      input_ports,
                      output_signals,
                      parent_id);
        new_parent_id = component.getGlobalId().toStdString();
        folders_[component.getGlobalId().toStdString()] = c;
    }

    if (canCastTo<daq::IFolder>(component))
    {
        daq::FolderPtr folder = castTo<daq::IFolder>(component);
        for (const auto& item : folder.getItems())
            RetrieveTopology(item, nodes, new_parent_id);
    }
}

OpenDAQNodeInteractionHandler::OpenDAQNodeInteractionHandler(OpenDAQHandler* opendaq_handler)
    : opendaq_handler_(opendaq_handler)
{
};

void OpenDAQNodeInteractionHandler::RetrieveConnections(ImGui::ImGuiNodes& nodes)
{
    for (const auto& [input_uid, input_component] : opendaq_handler_->input_ports_)
    {
        daq::InputPortPtr input_port = input_component.component_.as<daq::IInputPort>();
        if (input_port.assigned() && input_port.getSignal().assigned())
        {
            daq::SignalPtr connected_signal = input_port.getSignal();
            std::string signal_uid = connected_signal.getGlobalId().toStdString();
            nodes.AddConnection(signal_uid, input_uid);
        }
    }
}

void OpenDAQNodeInteractionHandler::OnConnectionCreated(const ImGui::ImGuiNodesUid& output_id, const ImGui::ImGuiNodesUid& input_id)
{
    daq::SignalPtr signal;
    daq::InputPortPtr input_port;
    if (auto it = opendaq_handler_->signals_.find(output_id); it != opendaq_handler_->signals_.end())
        signal = it->second.component_.as<daq::ISignal>();
    if (auto it = opendaq_handler_->input_ports_.find(input_id); it != opendaq_handler_->input_ports_.end())
        input_port = it->second.component_.as<daq::IInputPort>();

    if (signal.assigned() && input_port.assigned())
        input_port.connect(signal);
}

void OpenDAQNodeInteractionHandler::OnOutputHover(const ImGui::ImGuiNodesUid& id)
{
    static daq::StreamReaderPtr reader;
    static daq::RatioPtr tick_resolution;
    static std::string last_id{""};

    static size_t MAX_PLOT_POINTS = 900;
    static std::vector<double> plot_values_avg(MAX_PLOT_POINTS);
    static std::vector<double> plot_values_min(MAX_PLOT_POINTS);
    static std::vector<double> plot_values_max(MAX_PLOT_POINTS);
    static std::vector<double> plot_times(MAX_PLOT_POINTS);
    static double end_time = 0;
    static size_t pos_in_plot_buffer = 0;
    static size_t points_in_plot_buffer = 0;
    static float step = 1.0f;
    static float seconds_shown = 2.0f;

    if (id == "")
    {
        // user stopped hovering
        reader = nullptr;
        last_id = "";
        end_time = 0;
        pos_in_plot_buffer = 0;
        points_in_plot_buffer = 0;
        return;
    }

    static std::string signal_name{""};
    static std::string signal_unit{""};
    static bool has_domain_signal;
    static int64_t start_time{-1};
    
    if (last_id != id)
    {
        // reinitialization with a new signal
        last_id = id;
        daq::SignalPtr signal = opendaq_handler_->signals_[id].component_.as<daq::ISignal>();

        signal_name = signal.getName().toStdString();
        if (signal.getDescriptor().assigned() && signal.getDescriptor().getUnit().assigned() && signal.getDescriptor().getUnit().getSymbol().assigned())
            signal_unit = signal.getDescriptor().getUnit().getSymbol().toStdString();
        else
            signal_unit = "";

        if (!signal.getDescriptor().assigned())
        {
            reader = nullptr;
            end_time = 0;
            pos_in_plot_buffer = 0;
            points_in_plot_buffer = 0;
            return;
        }
        
        has_domain_signal = signal.getDomainSignal().assigned();
        tick_resolution = has_domain_signal ? signal.getDomainSignal().getDescriptor().getTickResolution() : signal.getDescriptor().getTickResolution();
        float samples_per_second;
        try { samples_per_second = std::max<daq::Int>(1, daq::reader::getSampleRate(has_domain_signal ? signal.getDomainSignal().getDescriptor() : signal.getDescriptor())); } catch (...) { samples_per_second = 1; }
        step = std::floor(std::max(1.0f, (float)samples_per_second * seconds_shown / (float)MAX_PLOT_POINTS));
        
        reader = daq::StreamReaderBuilder()
            .setSignal(signal)
            .setValueReadType(has_domain_signal ? daq::SampleType::Float64 : daq::SampleType::Int64)
            .setDomainReadType(daq::SampleType::Int64)
            .setSkipEvents(true)
            .build();
        
        start_time = -1;
    }

    static size_t READ_BUFFER_SIZE = 1024 * 10;
    static std::vector<double> read_values(READ_BUFFER_SIZE);
    static std::vector<int64_t> read_times(READ_BUFFER_SIZE);
    if (reader != nullptr && reader.assigned())
    {
        while (true)
        {
            daq::SizeT read_count = READ_BUFFER_SIZE;
            if (has_domain_signal)
                reader.readWithDomain(read_values.data(), read_times.data(), &read_count);
            else
                reader.read(read_values.data(), &read_count);
            if (read_count == 0)
                break;
            
            if (start_time == -1)
                start_time = read_times[0];

            for (size_t i = 0, read_pos = 0; i + step < read_count; i += step)
            {
                plot_times[pos_in_plot_buffer] = (read_times[read_pos] - start_time) * tick_resolution.getNumerator() / (double)tick_resolution.getDenominator();
                plot_values_avg[pos_in_plot_buffer] = 0;
                plot_values_min[pos_in_plot_buffer] = 1e30;
                plot_values_max[pos_in_plot_buffer] = -1e30;
                for (size_t j = 0; j < step; ++j, ++read_pos)
                {
                    plot_values_avg[pos_in_plot_buffer] += read_values[read_pos];
                    plot_values_min[pos_in_plot_buffer] = std::min(read_values[read_pos], plot_values_min[pos_in_plot_buffer]);
                    plot_values_max[pos_in_plot_buffer] = std::max(read_values[read_pos], plot_values_max[pos_in_plot_buffer]);
                }
                plot_values_avg[pos_in_plot_buffer] = plot_values_avg[pos_in_plot_buffer] / step;

                end_time = plot_times[pos_in_plot_buffer];
                pos_in_plot_buffer += 1; if (pos_in_plot_buffer >= MAX_PLOT_POINTS) pos_in_plot_buffer = 0;
                points_in_plot_buffer = std::min(points_in_plot_buffer + 1, MAX_PLOT_POINTS);
            }
        }
    }

    if (ImGui::BeginTooltip())
    {
        ImGui::Text("%s [%s]", signal_name.c_str(), signal_unit.c_str());
        
        static ImPlotAxisFlags flags = ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_ShowEdgeLabels;
        if (ImPlot::BeginPlot("##SignalPreview", ImVec2(800,300), ImPlotFlags_NoLegend))
        {
            ImPlot::SetupAxes(nullptr, nullptr, flags, flags);
            
            if (has_domain_signal)
            {
                ImPlot::SetupAxisLimits(ImAxis_Y1, -5, 5);
                ImPlot::SetupAxisLimits(ImAxis_X1, end_time - seconds_shown, end_time, ImGuiCond_Always);
                ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.3f);
                ImPlot::PlotShaded("Uncertain Data", plot_times.data(), plot_values_min.data(), plot_values_max.data(), (int)points_in_plot_buffer, 0, pos_in_plot_buffer);
                ImPlot::PlotLine("", plot_times.data(), plot_values_avg.data(), (int)points_in_plot_buffer, 0, pos_in_plot_buffer);
                ImPlot::PopStyleVar();
            }
            else
            {
                static double dummy_ticks[] = {0};
                static const char* dummy_labels[] = {"no value"};
                ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
                ImPlot::SetupAxis(ImAxis_Y1, "", ImPlotAxisFlags_AutoFit);
                ImPlot::SetupAxisTicks(ImAxis_Y1, dummy_ticks, 1, dummy_labels, false);
                ImPlot::PlotLine("", plot_times.data(), plot_values_avg.data(), (int)points_in_plot_buffer);
            }
            ImPlot::EndPlot();
        }
        ImGui::EndTooltip();
    }
}

void OpenDAQNodeInteractionHandler::OnInputHover(const ImGui::ImGuiNodesUid& id)
{
    if (id == "")
    {
        OnOutputHover("");
        return;
    }

    auto it = opendaq_handler_->input_ports_.find(id);
    if (it == opendaq_handler_->input_ports_.end())
        return;

    daq::InputPortPtr input_port = it->second.component_.as<daq::IInputPort>();
    if (!input_port.assigned())
        return;

    daq::SignalPtr signal = input_port.getSignal();
    if (!signal.assigned())
        return;

    OnOutputHover(signal.getGlobalId().toStdString());
}

void OpenDAQNodeInteractionHandler::OnSelectionChanged(const std::vector<ImGui::ImGuiNodesUid>& selected_ids)
{
    selected_components_.clear();
    for (ImGui::ImGuiNodesUid id : selected_ids)
    {
        if (auto it = opendaq_handler_->folders_.find(id); it != opendaq_handler_->folders_.end())
            selected_components_.push_back(it->second.component_);
        if (auto it = opendaq_handler_->input_ports_.find(id); it != opendaq_handler_->input_ports_.end())
            selected_components_.push_back(it->second.component_);
        if (auto it = opendaq_handler_->signals_.find(id); it != opendaq_handler_->signals_.end())
            selected_components_.push_back(it->second.component_);
    }
}

void OpenDAQNodeInteractionHandler::RenderFunctionBlockOptions(ImGui::ImGuiNodes* nodes, daq::ComponentPtr parent_component, const std::string& parent_id, ImVec2 position)
{
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> available_fbs;
    
    try
    {
        if (canCastTo<daq::IInstance>(parent_component))
        {
            daq::InstancePtr instance = castTo<daq::IInstance>(parent_component);
            available_fbs = instance.getAvailableFunctionBlockTypes();
        }
        else if (canCastTo<daq::IDevice>(parent_component))
        {
            daq::DevicePtr device = castTo<daq::IDevice>(parent_component);
            available_fbs = device.getAvailableFunctionBlockTypes();
        }
        else if (canCastTo<daq::IFunctionBlock>(parent_component))
        {
            daq::FunctionBlockPtr function_block = castTo<daq::IFunctionBlock>(parent_component);
            available_fbs = function_block.getAvailableFunctionBlockTypes();
        }
        else
            return;
    }
    catch (...)
    {
        return;
    }

    if (!available_fbs.assigned() || available_fbs.getCount() == 0)
    {
        ImGui::Text("No function blocks available");
        return;
    }

    for (const auto [fb_id, desc] : available_fbs)
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
                std::vector<ImGui::ImGuiNodesIdentifier> input_ports;
                std::vector<ImGui::ImGuiNodesIdentifier> output_signals;

                for (const daq::InputPortPtr& input_port : fb.getInputPorts())
                {
                    input_ports.push_back({input_port.getName().toStdString(), input_port.getGlobalId().toStdString()});
                    opendaq_handler_->input_ports_[input_port.getGlobalId().toStdString()] = {input_port, fb};
                }

                for (const daq::SignalPtr& signal : fb.getSignals())
                {
                    opendaq_handler_->signals_[signal.getGlobalId().toStdString()] = {signal, fb};
                    output_signals.push_back({signal.getName().toStdString(), signal.getGlobalId().toStdString()});
                }

                int color_index = parent_id.empty() ? 0 : opendaq_handler_->folders_[parent_id].color_index_;

                nodes->AddNode({fb.getName().toStdString(), fb.getGlobalId().toStdString()},
                              opendaq_handler_->color_palette_[color_index],
                              position,
                              input_ports,
                              output_signals,
                              parent_id);

                OpenDAQComponent c;
                c.component_ = fb;
                c.parent_ = parent_component;
                c.color_index_ = color_index;
                opendaq_handler_->folders_[fb.getGlobalId().toStdString()] = c;
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

void OpenDAQNodeInteractionHandler::RenderDeviceOptions(ImGui::ImGuiNodes* nodes, daq::ComponentPtr parent_component, const std::string& parent_id, ImVec2 position)
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

    if (available_devices_.assigned() && available_devices_.getCount() > 0)
    {
        for (const auto& device_info : available_devices_)
        {
            std::string device_connection_name = device_info.getName();
            std::string device_connection_string = device_info.getConnectionString();
            if (ImGui::MenuItem((device_connection_name + " (" + device_connection_string + ")").c_str()))
            {
                const daq::DevicePtr dev = parent_device.addDevice(device_connection_string);
                int color_index = opendaq_handler_->next_color_index_;
                opendaq_handler_->next_color_index_ = (opendaq_handler_->next_color_index_ + 1) % opendaq_handler_->color_palette_size_;
                nodes->AddNode({dev.getName().toString(), dev.getGlobalId().toString()}, 
                               opendaq_handler_->color_palette_[color_index], 
                               position,
                               {}, {},
                               parent_id);
                
                OpenDAQComponent c;
                c.component_ = dev;
                c.parent_ = parent_component;
                c.color_index_ = color_index;
                opendaq_handler_->folders_[dev.getGlobalId().toStdString()] = c;
            }
        }
    }

    std::string device_connection_string = "daq.nd://";
    ImGui::InputText("##w", &device_connection_string);
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        const daq::DevicePtr dev = parent_device.addDevice(device_connection_string);
        int color_index = opendaq_handler_->next_color_index_;
        opendaq_handler_->next_color_index_ = (opendaq_handler_->next_color_index_ + 1) % opendaq_handler_->color_palette_size_;
        nodes->AddNode({dev.getName().toString(), dev.getGlobalId().toString()}, 
                       opendaq_handler_->color_palette_[color_index], 
                       position,
                       {}, {},
                       parent_id);
        
        OpenDAQComponent c;
        c.component_ = dev;
        c.parent_ = parent_component;
        c.color_index_ = color_index;
        opendaq_handler_->folders_[dev.getGlobalId().toStdString()] = c;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Connect"))
    {
        const daq::DevicePtr dev = parent_device.addDevice(device_connection_string);
        int color_index = opendaq_handler_->next_color_index_;
        opendaq_handler_->next_color_index_ = (opendaq_handler_->next_color_index_ + 1) % opendaq_handler_->color_palette_size_;
        nodes->AddNode({dev.getName().toString(), dev.getGlobalId().toString()}, 
                       opendaq_handler_->color_palette_[color_index], 
                       position,
                       {}, {},
                       parent_id);
        
        OpenDAQComponent c;
        c.component_ = dev;
        c.parent_ = parent_component;
        c.color_index_ = color_index;
        opendaq_handler_->folders_[dev.getGlobalId().toStdString()] = c;
        ImGui::CloseCurrentPopup();
    }
}

void OpenDAQNodeInteractionHandler::RenderPopupMenu(ImGui::ImGuiNodes* nodes, ImVec2 position)
{
    ImGui::SeparatorText("Add a function block");
    RenderFunctionBlockOptions(nodes, opendaq_handler_->instance_, "", position);

    ImGui::SeparatorText("Connect to device");
    RenderDeviceOptions(nodes, opendaq_handler_->instance_, "", position);
}

void OpenDAQNodeInteractionHandler::OnAddButtonClick(const ImGui::ImGuiNodesUid& parent_node_id, std::optional<ImVec2> position)
{
    add_button_drop_position_ = position;
    if (auto it = opendaq_handler_->folders_.find(parent_node_id); it != opendaq_handler_->folders_.end())
        add_button_click_component_ = it->second.component_;
    else
        add_button_click_component_ = nullptr;

    ImGui::OpenPopup("AddNestedNodeMenu");
}

void OpenDAQNodeInteractionHandler::RenderNestedNodePopup(ImGui::ImGuiNodes* nodes)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

    if (ImGui::BeginPopup("NodesContextMenu", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        RenderPopupMenu(nodes, add_button_drop_position_ ? add_button_drop_position_.value() : ImGui::GetMousePos());
        ImGui::EndPopup();
        ImGui::PopStyleVar();
        return;
    }

    if (ImGui::BeginPopup("AddNestedNodeMenu", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        if (add_button_click_component_ == nullptr)
        {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No component selected");
            ImGui::EndPopup();
            ImGui::PopStyleVar();
            return;
        }

        bool supports_fbs = canCastTo<daq::IInstance>(add_button_click_component_) || 
                            canCastTo<daq::IDevice>(add_button_click_component_) || 
                            canCastTo<daq::IFunctionBlock>(add_button_click_component_);
        bool supports_devices = canCastTo<daq::IDevice>(add_button_click_component_);

        if (supports_fbs)
        {
            ImGui::SeparatorText("Add function block");
            RenderFunctionBlockOptions(nodes, 
                                      add_button_click_component_, 
                                      add_button_click_component_.getGlobalId().toStdString(),
                                      add_button_drop_position_ ? add_button_drop_position_.value() : ImVec2(0, 0));
        }

        if (supports_devices)
        {
            ImGui::SeparatorText("Connect to device");
            RenderDeviceOptions(nodes, 
                              add_button_click_component_, 
                              add_button_click_component_.getGlobalId().toStdString(),
                              add_button_drop_position_ ? add_button_drop_position_.value() : ImVec2(0, 0));
        }

        if (!supports_fbs && !supports_devices)
        {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "This component doesn't support nested components");
        }
        
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("AddInputMenu", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        if (ImSearch::BeginSearch())
        {
            ImSearch::SearchBar();

            for (const auto& e : opendaq_handler_->signals_)
            {
                const std::string& id = e.first;
                const OpenDAQComponent& sig = e.second;

                std::string entry_name = sig.component_.getName().toStdString() + " (" + id + ")";
                ImSearch::SearchableItem(entry_name.c_str(), [=](const char*)
                    {
                        if (ImGui::Selectable(entry_name.c_str()))
                        {
                            daq::InputPortPtr input_port = castTo<daq::IInputPort>(dragged_input_port_component_);
                            daq::SignalPtr signal = castTo<daq::ISignal>(sig.component_);

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

void OpenDAQNodeInteractionHandler::ShowStartupPopup(ImGui::ImGuiNodes* nodes)
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

        RenderPopupMenu(nodes, ImVec2(0,0));
        ImGui::Separator();

        if (ImGui::Button("Dismiss"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void OpenDAQNodeInteractionHandler::OnEmptySpaceClick(ImVec2 position)
{
    add_button_drop_position_ = position;
    ImGui::OpenPopup("NodesContextMenu");
}

void OpenDAQNodeInteractionHandler::OnInputDropped(const ImGui::ImGuiNodesUid& input_uid, std::optional<ImVec2> /*position*/)
{
    if (auto it = opendaq_handler_->input_ports_.find(input_uid); it != opendaq_handler_->input_ports_.end())
        dragged_input_port_component_ = it->second.component_;
    else
        dragged_input_port_component_ = nullptr;

    ImGui::OpenPopup("AddInputMenu");
}
