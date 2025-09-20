#include "opendaq_control.h"
#include <iostream>

OpenDAQ* topology_instance_ = nullptr;

OpenDAQ* GetTopologyInstance()
{
    if (topology_instance_ == nullptr)
        topology_instance_ = new OpenDAQ();
    return topology_instance_;
}

OpenDAQ::OpenDAQ()
    : OpenDAQ(daq::Instance("."))
{

}

OpenDAQ::OpenDAQ(daq::InstancePtr instance)
    : instance_(instance)
{
}

std::vector<std::string> OpenDAQ::GetAvailableDevices()
{
    std::vector<std::string> devices;
    for (const auto& deviceInfo : instance_.getAvailableDevices())
        devices.push_back(deviceInfo.getConnectionString());
    return devices;
}

void OpenDAQ::ConnectToDevice(const std::string& device_id)
{
    instance_.addDevice(device_id);
    RetrieveFolders(instance_);

    std::cout << " SIGNALS " << std::endl;
    for (const auto& [k, value] : signals_)
    {
        std::cout << k << std::endl;
    }
}

OpenDAQ::Topology OpenDAQ::RetrieveTopology(daq::ComponentPtr component, int indent)
{
    if (component == nullptr)
        return {};

    if (canCastTo<daq::IFolder>(component) && daq::FolderPtr(castTo<daq::IFolder>(component)).isEmpty())
        return {};

    // TODO: in these cases you should actually descend and find the input ports and output signals
    // if (canCastTo<daq::IFolder>(component) && (component.getName() == "IP" || component.getName() == "Sig" || component.getName() == "Srv"))
    //     return {};
    // if (canCastTo<daq::IInputPort>(component) || canCastTo<daq::ISignal>(component) || canCastTo<daq::IServer>(component))
    //     return {};

    std::cout << std::string(indent*2, ' ') << "> " << component.getName();
    if (canCastTo<daq::ISignal>(component))
        std::cout << " (Signal)";
    if (canCastTo<daq::IInputPort>(component))
        std::cout << " (Input port)";
    if (canCastTo<daq::IChannel>(component))
        std::cout << " (Channel)";
    if (canCastTo<daq::IFunctionBlock>(component))
        std::cout << " (Function block)";
    if (canCastTo<daq::IDevice>(component))
        std::cout << " (Device)";
    std::cout << component.getGlobalId().toStdString();
    std::cout << std::endl;


    if (canCastTo<daq::IFolder>(component))
    {
        daq::FolderPtr folder = castTo<daq::IFolder>(component);
        for (const auto& item : folder.getItems())
            RetrieveTopology(item, indent + 1);
    }
    return {};
}

std::vector<std::string> RetrieveInputPorts(daq::ComponentPtr component)
{
    if (component == nullptr)
        return {};

    std::vector<std::string> inputs;
    if (canCastTo<daq::IFolder>(component))
    {
        daq::FolderPtr folder = castTo<daq::IFolder>(component);
        for (const auto& item : folder.getItems())
        {
            if (canCastTo<daq::IInputPort>(item))
                inputs.push_back(item.getLocalId().toStdString());
        }
    }
    return inputs;
}

std::vector<std::string> OpenDAQ::RetrieveSignals(daq::ComponentPtr component)
{
    if (component == nullptr)
        return {};

    std::vector<std::string> signals;
    if (canCastTo<daq::IFolder>(component))
    {
        daq::FolderPtr folder = castTo<daq::IFolder>(component);
        for (const auto& item : folder.getItems())
        {
            if (canCastTo<daq::ISignal>(item))
            {
                signals.push_back(item.getLocalId().toStdString());
                std::cout << item.getLocalId().toStdString()  << std::endl;
                signals_[item.getLocalId().toStdString()] = castTo<daq::ISignal>(item);
                std::cout << item.getLocalId().toStdString()  << std::endl;
            }
        }
    }
    return signals;
}

std::optional<OpenDAQ::Topology> OpenDAQ::RetrieveFolders(daq::ComponentPtr component)
{
    if (component == nullptr)
        return {};

    if (canCastTo<daq::IFolder>(component) && daq::FolderPtr(castTo<daq::IFolder>(component)).isEmpty())
        return {};

    if (component.getName() == "IP" || component.getName() == "Sig")
        return {};

    Topology topology;
    topology.name = component.getName().toStdString();
    topology.globalId = component.getLocalId().toStdString();
    // topology.component = castTo<daq::IComponent>(component);
    // topology.component = daq::ComponentPtr::Borrow(component);
    topology.component = component;
    // topology.component = component.asPtr<daq::IComponent>(true);
    // topology.componentWeak = daq::WeakRefPtr<daq::IComponent>(component);


    // if (canCastTo<daq::IPropertyObject>(component))
    // {
    //     daq::PropertyObjectPtr property_holder = castTo<daq::IPropertyObject>(component);
    //     for (const auto& prop : property_holder.getVisibleProperties())
    //     {
    //         Property property(property_holder, prop);
    //         std::cout << '|' << property.name << " (" << property.value << ')' << std::endl;
    //     }
    // }

    if (canCastTo<daq::IFolder>(component))
    {
        daq::FolderPtr folder = castTo<daq::IFolder>(component);
        for (const auto& item : folder.getItems())
        {
            if (item.getName() == "IP")
                topology.inputs = RetrieveInputPorts(item);
            else if (item.getName() == "Sig")
                topology.outputs = RetrieveSignals(item);
            else
            {
                if (RetrieveFolders(item))
                    connections_[topology.globalId] = item.getLocalId().toStdString();
            }
        }
    }

    std::cout << topology.name << " (" << topology.globalId << ")" << std::endl;
    folders_[topology.globalId] = topology;
    return topology;
}
