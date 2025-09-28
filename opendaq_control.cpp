#include "opendaq_control.h"
#include "imgui_stdlib.h"
#include "implot.h"


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

        nodes.AddNode({component.getName().toStdString(), component.getGlobalId().toStdString()}, ImColor(0.4f, 0.6f, 0.3f, 1.0f),
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
    static int64_t start_time{-1};
    static daq::TailReaderPtr reader;
    static daq::RatioPtr tick_resolution;
    static std::string last_id{""};

    if (id == "")
    {
        reader = nullptr;
        start_time = -1;
        last_id = "";
        return;
    }

    static int samples_in_2_seconds{0};
    static std::vector<double> values;
    static std::vector<double> times;
    static std::vector<ImS64> values_int;
    static std::vector<ImS64> times_int;
    static std::string signal_name{""};
    static std::string signal_unit{""};
    static bool has_domain_signal;
    if (last_id != id)
    {
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
            start_time = -1;
            return;
        }
        has_domain_signal = signal.getDomainSignal().assigned();
        tick_resolution = has_domain_signal ? signal.getDomainSignal().getDescriptor().getTickResolution() : signal.getDescriptor().getTickResolution();
        try { samples_in_2_seconds = std::max<daq::Int>(2, daq::reader::getSampleRate(has_domain_signal ? signal.getDomainSignal().getDescriptor() : signal.getDescriptor()) * 2); } catch (...) { samples_in_2_seconds = 2; }
        values.resize(samples_in_2_seconds);
        times_int.resize(samples_in_2_seconds);
        times.resize(samples_in_2_seconds);
        values_int.resize(samples_in_2_seconds);
        if (!has_domain_signal)
        {
            for (int i = 0; i < samples_in_2_seconds; i++)
            {
                values_int[i] = 0;
                values[i] = 0;
            }
        }
        reader = daq::TailReaderBuilder()
            .setSignal(signal)
            .setHistorySize(samples_in_2_seconds)
            .setValueReadType(has_domain_signal ? daq::SampleType::Float64 : daq::SampleType::Int64)
            .setDomainReadType(daq::SampleType::Int64)
            .setSkipEvents(true)
            .build();
        start_time = -1;
    }

    if (ImGui::BeginTooltip())
    {
        daq::SizeT count = samples_in_2_seconds;
        if (reader != nullptr && reader.assigned())
        {
            if (has_domain_signal)
                reader.readWithDomain(values.data(), times_int.data(), &count);
            else
                reader.read(times_int.data(), &count);
        }
        else
            count = 0;

        ImGui::Text("%s [%s]", signal_name.c_str(), signal_unit.c_str());
        if (count)
        {
            if (start_time == -1)
                start_time = times_int[0];
            for (int i = 0; i < count; i++)
            {
                if (!has_domain_signal)
                    times[i] = double(tick_resolution.getNumerator() * times_int[i] / static_cast<double>(tick_resolution.getDenominator()));
                else
                {
                    times[i] = static_cast<double>(times_int[i] - start_time);
                    times[i] = double(tick_resolution.getNumerator() * times[i] / static_cast<double>(tick_resolution.getDenominator()));
                }
            }

            static ImPlotAxisFlags flags = ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_ShowEdgeLabels;
            if (ImPlot::BeginPlot("##SignalPreview", ImVec2(800,300), ImPlotFlags_NoLegend))
            {
                ImPlot::SetupAxes(nullptr, nullptr, flags, flags);
                ImPlot::SetupAxisLimits(ImAxis_Y1,-5,5);
                ImPlot::SetupAxisLimits(ImAxis_X1,times[0], times[count-1], ImGuiCond_Always);
                if (has_domain_signal)
                {
                    ImPlot::PlotLine("", times.data(), values.data(), (int)count);
                }
                else
                {
                    static double dummy_ticks[] = {0};
                    static const char* dummy_labels[] = {"no value"};
                    ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
                    // ImPlot::SetupAxisLimits(ImAxis_X1,times_int[0], times_int[count-1], ImGuiCond_Always);
                    ImPlot::SetupAxis(ImAxis_Y1, "", ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupAxisTicks(ImAxis_Y1, dummy_ticks, 1, dummy_labels, false);
                    ImPlot::PlotLine("", times.data(), values.data(), (int)count);
                }
                ImPlot::EndPlot();
            }
        }
        else
        {
            if (ImPlot::BeginPlot("##Scrolling", ImVec2(800,300), ImPlotFlags_NoLegend))
                ImPlot::EndPlot();
        }
        ImGui::EndTooltip();
    }
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

void OpenDAQNodeInteractionHandler::RenderPopupMenu(ImGui::ImGuiNodes* nodes, ImVec2 position)
{
    ImGui::SeparatorText("Add a function block");
    for (const auto [fb_id, desc]: opendaq_handler_->instance_.getAvailableFunctionBlockTypes())
    {
        if (ImGui::MenuItem(fb_id.toStdString().c_str()))
        {
            daq::FunctionBlockPtr fb = opendaq_handler_->instance_.addFunctionBlock(fb_id);
            nodes->AddNode({fb.getName().toStdString(), fb.getGlobalId().toStdString()}, ImColor(100, 100, 200), position,
                           {}, {},
                           "");
            opendaq_handler_->folders_[fb.getGlobalId().toStdString()] = {fb, {}};
        }
        if (ImGui::BeginItemTooltip())
        {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc.getDescription().toStdString().c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    ImGui::SeparatorText("Connect to device");
    
    if (ImGui::Button(available_devices_.assigned() && available_devices_.getCount() > 0 ? "Refresh devices" : "Discover devices"))
        available_devices_ = opendaq_handler_->instance_.getAvailableDevices();
    
    if (available_devices_.assigned() && available_devices_.getCount() > 0)
    {
        for (const auto& device_info : available_devices_)
        {
            std::string device_connection_name = device_info.getName();
            std::string device_connection_string = device_info.getConnectionString();
            if (ImGui::MenuItem((device_connection_name + " (" + device_connection_string + ")").c_str()))
            {
                opendaq_handler_->instance_.addDevice(device_connection_string);
                nodes->AddNode(device_connection_string.c_str(), ImColor(0, 100, 200), position,
                               {}, {},
                               "");
            }
        }
    }
    
    std::string device_connection_string = "daq.nd://";
    ImGui::InputText("##w", &device_connection_string);
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        opendaq_handler_->instance_.addDevice(device_connection_string);
        nodes->AddNode(device_connection_string.c_str(), ImColor(0, 100, 200), position,
                       {}, {},
                       "");
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Connect"))
    {
        opendaq_handler_->instance_.addDevice(device_connection_string);
        nodes->AddNode(device_connection_string.c_str(), ImColor(0, 100, 200), position,
                       {}, {},
                       "");
        ImGui::CloseCurrentPopup();
    }
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

    if (ImGui::BeginPopup("AddNestedNodeMenu", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        ImGui::SeparatorText("Add nested function block");
        if (add_button_click_component_ == nullptr || !canCastTo<daq::IFunctionBlock>(add_button_click_component_))
        {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "This component doesn't support nested function blocks");
            ImGui::EndPopup();
            ImGui::PopStyleVar();
            return;
        }

        try
        {
            daq::FunctionBlockPtr parent_fb = castTo<daq::IFunctionBlock>(add_button_click_component_);
            bool has_any = false;
            auto available_nested_fbs = parent_fb.getAvailableFunctionBlockTypes();
            for (const auto [fb_id, desc]: available_nested_fbs)
            {
                if (ImGui::MenuItem(fb_id.toStdString().c_str()))
                {
                    daq::FunctionBlockPtr nested_fb = parent_fb.addFunctionBlock(fb_id);
                    
                    std::vector<ImGui::ImGuiNodesIdentifier> input_ports;
                    std::vector<ImGui::ImGuiNodesIdentifier> output_signals;
                    
                    for (const daq::InputPortPtr& input_port : nested_fb.getInputPorts())
                    {
                        input_ports.push_back({input_port.getName().toStdString(), input_port.getGlobalId().toStdString()});
                        opendaq_handler_->input_ports_[input_port.getGlobalId().toStdString()] = {input_port, nested_fb};
                    }

                    for (const daq::SignalPtr& signal : nested_fb.getSignals())
                    {
                        opendaq_handler_->signals_[signal.getGlobalId().toStdString()] = {signal, nested_fb};
                        output_signals.push_back({signal.getName().toStdString(), signal.getGlobalId().toStdString()});
                    }
                    
                    if (add_button_drop_position_)
                    {
                        nodes->AddNode({nested_fb.getName().toStdString(), nested_fb.getGlobalId().toStdString()}, 
                                      ImColor(0.3f, 0.5f, 0.8f, 1.0f),
                                      add_button_drop_position_.value(),
                                      input_ports,
                                      output_signals,
                                      add_button_click_component_.getGlobalId().toStdString());
                    }
                    else
                    {
                        nodes->AddNode({nested_fb.getName().toStdString(), nested_fb.getGlobalId().toStdString()}, 
                                      ImColor(0.3f, 0.5f, 0.8f, 1.0f),
                                      input_ports,
                                      output_signals,
                                      add_button_click_component_.getGlobalId().toStdString());
                    }
                    
                    OpenDAQComponent c;
                    c.component_ = nested_fb;
                    opendaq_handler_->folders_[nested_fb.getGlobalId().toStdString()] = c;
                    add_button_click_component_ = nullptr;
                }
                
                if (ImGui::BeginItemTooltip())
                {
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                    ImGui::TextUnformatted(desc.getDescription().toStdString().c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
            }
            if (available_nested_fbs.getCount() == 0)
                ImGui::Text("No nested function blocks available");
        }
        catch (...)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: Cannot add nested function blocks to this component");
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
