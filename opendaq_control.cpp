#include "opendaq_control.h"


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
            input_ports.push_back({input_port.getName().toStdString(), input_port.getGlobalId().toStdString()});

        for (const daq::SignalPtr& signal : function_block.getSignals())
            output_signals.push_back({signal.getName().toStdString(), signal.getGlobalId().toStdString()});
    }
    if (canCastTo<daq::IDevice>(component))
    {
        daq::DevicePtr device = castTo<daq::IDevice>(component);
        for (const daq::SignalPtr& signal : device.getSignals())
            output_signals.push_back({signal.getName().toStdString(), signal.getGlobalId().toStdString()});
    }


    std::string new_parent_id = "";
    if (component == instance_ || component.getName() == "IO" || component.getName() == "AI" || component.getName() == "AO" || component.getName() == "Dev" || component.getName() == "FB")
    {
        new_parent_id = parent_id;
    }
    else
    {
        nodes.AddNode({component.getName().toStdString(), component.getGlobalId().toStdString()}, ImColor(0.4f, 0.6f, 0.3f, 1.0f),
                      input_ports,
                      output_signals,
                      parent_id);
        new_parent_id = component.getGlobalId().toStdString();
    }

    if (canCastTo<daq::IFolder>(component))
    {
        daq::FolderPtr folder = castTo<daq::IFolder>(component);
        for (const auto& item : folder.getItems())
            RetrieveTopology(item, nodes, new_parent_id);
    }
}
