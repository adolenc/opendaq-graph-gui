#pragma once
#include <opendaq/opendaq.h>
#include <vector>
#include <iostream>
#include <optional>
#include <string>


class OpenDAQ
{

public:
    OpenDAQ();
    OpenDAQ(daq::InstancePtr instance);
    std::vector<std::string> GetAvailableDevices();
    struct Topology
    {
        std::string name;
        std::string globalId;
        daq::ComponentPtr component;
        std::vector<std::string> inputs;
        std::vector<std::string> outputs;
        std::vector<Topology> children;

        void print() const
        {
            std::cout << name << " (" << globalId << ")" << std::endl;
            for (const auto& input : inputs)
                std::cout << "  - " << input << " (Input)" << std::endl;
            for (const auto& output : outputs)
                std::cout << "  - " << output << " (Output)" << std::endl;
            // for (const auto& child : children)
            //     child.print();
        }
    };

    Topology RetrieveTopology(daq::ComponentPtr component, int indent = 0);
    std::optional<Topology> RetrieveFolders(daq::ComponentPtr component);
    void ConnectToDevice(const std::string& device_id);
    Topology AddFunctionBlock(const std::string& fb_id);

    daq::InstancePtr instance_;
    std::unordered_map<std::string, Topology> folders_{};
    std::unordered_map<std::string, std::string> connections_{};
    std::unordered_map<std::string, daq::SignalPtr> signals_{};

    std::vector<std::string> RetrieveSignals(daq::ComponentPtr component);
};

OpenDAQ* GetTopologyInstance();

template <class Interface>
bool canCastTo(daq::IBaseObject* baseObject)
{
    return daq::BaseObjectPtr::Borrow(baseObject).asPtrOrNull<Interface>().assigned();
}
template <class Interface>
auto castTo(daq::IBaseObject* baseObject)
{
    return daq::BaseObjectPtr::Borrow(baseObject).as<Interface>();
}

