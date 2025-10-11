#include "opendaq_control.h"
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
            c.color_index_ = next_color_index_++;
        }
        else if (!parent_id.empty())
        {
            c.color_index_ = folders_[parent_id].color_index_;
        }

        nodes_->AddNode({component.getName().toStdString(), component.getGlobalId().toStdString()}, 
                      c.color_index_,
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
            RetrieveTopology(item, new_parent_id);
    }
}

void OpenDAQNodeEditor::RetrieveConnections()
{
    for (const auto& [input_uid, input_component] : input_ports_)
    {
        daq::InputPortPtr input_port = input_component.component_.as<daq::IInputPort>();
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
        signal = it->second.component_.as<daq::ISignal>();
    if (auto it = input_ports_.find(input_id); it != input_ports_.end())
        input_port = it->second.component_.as<daq::IInputPort>();

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

    if (last_id != id)
    {
        // reinitialization with a new signal
        last_id = id;
        daq::SignalPtr signal = signals_[id].component_.as<daq::ISignal>();
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
            
            if (signal_preview.has_domain_signal_)
            {
                ImPlot::SetupAxisLimits(ImAxis_Y1, -5, 5);
                ImPlot::SetupAxisLimits(ImAxis_X1, signal_preview.end_time_seconds_ - 2.0, signal_preview.end_time_seconds_, ImGuiCond_Always);
                ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
                ImPlot::SetNextFillStyle(ImColor(0xff66ffff), 0.3);
                ImPlot::PlotShaded("Uncertain Data", signal_preview.plot_times_seconds_.data(), signal_preview.plot_values_min_.data(), signal_preview.plot_values_max_.data(), (int)signal_preview.points_in_plot_buffer_, 0, signal_preview.pos_in_plot_buffer_);
                ImPlot::SetNextLineStyle(ImColor(0xff66ffff));
                ImPlot::PlotLine("", signal_preview.plot_times_seconds_.data(), signal_preview.plot_values_avg_.data(), (int)signal_preview.points_in_plot_buffer_, 0, signal_preview.pos_in_plot_buffer_);
            }
            else
            {
                static double dummy_ticks[] = {0};
                static const char* dummy_labels[] = {"no value"};
                ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
                ImPlot::SetupAxis(ImAxis_Y1, "", ImPlotAxisFlags_AutoFit);
                ImPlot::SetupAxisTicks(ImAxis_Y1, dummy_ticks, 1, dummy_labels, false);
                ImPlot::PlotLine("", signal_preview.plot_times_seconds_.data(), signal_preview.plot_values_avg_.data(), (int)signal_preview.points_in_plot_buffer_);
            }
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

    daq::InputPortPtr input_port = it->second.component_.as<daq::IInputPort>();
    if (!input_port.assigned())
        return;

    daq::SignalPtr signal = input_port.getSignal();
    if (!signal.assigned())
        return;

    OnOutputHover(signal.getGlobalId().toStdString());
}

void OpenDAQNodeEditor::OnSelectionChanged(const std::vector<ImGui::ImGuiNodesUid>& selected_ids)
{
    selected_components_.clear();
    for (ImGui::ImGuiNodesUid id : selected_ids)
    {
        if (auto it = folders_.find(id); it != folders_.end())
            selected_components_.push_back(it->second.component_);
        if (auto it = input_ports_.find(id); it != input_ports_.end())
            selected_components_.push_back(it->second.component_);
        if (auto it = signals_.find(id); it != signals_.end())
            selected_components_.push_back(it->second.component_);
    }
    
    properties_window_.OnSelectionChanged(selected_components_);
    signals_window_.OnSelectionChanged(selected_components_);
}

void OpenDAQNodeEditor::RenderFunctionBlockOptions(daq::ComponentPtr parent_component, const std::string& parent_id, ImVec2 position)
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
                    input_ports_[input_port.getGlobalId().toStdString()] = {input_port, fb};
                }

                for (const daq::SignalPtr& signal : fb.getSignals())
                {
                    signals_[signal.getGlobalId().toStdString()] = {signal, fb};
                    output_signals.push_back({signal.getName().toStdString(), signal.getGlobalId().toStdString()});
                }

                int color_index = parent_id.empty() ? 0 : folders_[parent_id].color_index_;

                nodes_->AddNode({fb.getName().toStdString(), fb.getGlobalId().toStdString()},
                              color_index,
                              position,
                              input_ports,
                              output_signals,
                              parent_id);

                OpenDAQComponent c;
                c.component_ = fb;
                c.parent_ = parent_component;
                c.color_index_ = color_index;
                folders_[fb.getGlobalId().toStdString()] = c;
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

    if (available_devices_.assigned() && available_devices_.getCount() > 0)
    {
        for (const auto& device_info : available_devices_)
        {
            std::string device_connection_name = device_info.getName();
            std::string device_connection_string = device_info.getConnectionString();
            if (ImGui::MenuItem((device_connection_name + " (" + device_connection_string + ")").c_str()))
            {
                const daq::DevicePtr dev = parent_device.addDevice(device_connection_string);
                int color_index = next_color_index_++;
                nodes_->AddNode({dev.getName().toString(), dev.getGlobalId().toString()}, 
                               color_index, 
                               position,
                               {}, {},
                               parent_id);
                
                OpenDAQComponent c;
                c.component_ = dev;
                c.parent_ = parent_component;
                c.color_index_ = color_index;
                folders_[dev.getGlobalId().toStdString()] = c;
            }
        }
    }

    std::string device_connection_string = "daq.nd://";
    ImGui::InputText("##w", &device_connection_string);
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        const daq::DevicePtr dev = parent_device.addDevice(device_connection_string);
        int color_index = next_color_index_++;
        nodes_->AddNode({dev.getName().toString(), dev.getGlobalId().toString()}, 
                       color_index, 
                       position,
                       {}, {},
                       parent_id);
        
        OpenDAQComponent c;
        c.component_ = dev;
        c.parent_ = parent_component;
        c.color_index_ = color_index;
        folders_[dev.getGlobalId().toStdString()] = c;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Connect"))
    {
        const daq::DevicePtr dev = parent_device.addDevice(device_connection_string);
        int color_index = next_color_index_++;
        nodes_->AddNode({dev.getName().toString(), dev.getGlobalId().toString()}, 
                       color_index, 
                       position,
                       {}, {},
                       parent_id);
        
        OpenDAQComponent c;
        c.component_ = dev;
        c.parent_ = parent_component;
        c.color_index_ = color_index;
        folders_[dev.getGlobalId().toStdString()] = c;
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
        add_button_click_component_ = it->second.component_;
    else
        add_button_click_component_ = nullptr;

    ImGui::OpenPopup("AddNestedNodeMenu");
}

void OpenDAQNodeEditor::RenderNestedNodePopup()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

    if (ImGui::BeginPopup("NodesContextMenu", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        RenderPopupMenu(nodes_, add_button_drop_position_ ? add_button_drop_position_.value() : ImGui::GetMousePos());
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
        
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("AddInputMenu", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        if (ImSearch::BeginSearch())
        {
            ImSearch::SearchBar();

            for (const auto& e : signals_)
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
        dragged_input_port_component_ = it->second.component_;
    else
        dragged_input_port_component_ = nullptr;

    ImGui::OpenPopup("AddInputMenu");
}

void OpenDAQNodeEditor::Render()
{
    properties_window_.Render();
    signals_window_.Render();
}
