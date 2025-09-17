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
    struct Property
    {
        std::string id;
        std::string name;
        std::optional<std::string> unit;
        bool read_only;
        bool visible;
        daq::CoreType type;
        daq::BaseObjectPtr value;
        std::optional<std::string> suggested_values;
        std::optional<daq::BaseObjectPtr> selection_values;
        std::optional<double> min;
        std::optional<double> max;

        Property(daq::PropertyObjectPtr owner, daq::PropertyPtr prop)
        {
            id = prop.getSerializeId();
            name = prop.getName().toStdString();
            if (prop.getUnit() != nullptr && prop.getUnit().assigned())
                unit = prop.getUnit().getSymbol().toStdString();
            read_only = prop.getReadOnly();
            visible = prop.getVisible();
            type = prop.getValueType();
            value = owner.getPropertyValue(name);
            if (prop.getSelectionValues() != nullptr)
                selection_values = prop.getSelectionValues();
            // suggested_values = prop.getSuggestedValues();
            if (prop.getMinValue() != nullptr) min = static_cast<double>(prop.getMinValue());
            if (prop.getMaxValue() != nullptr) max = static_cast<double>(prop.getMaxValue());
        }
        bool IsCompatibleWith(const Property& other) const
        {
            if (name != other.name) return false;
            if (type != other.type) return false;
            if (selection_values && other.selection_values && selection_values != other.selection_values) return false;

            return true;
        }
    };
    struct Topology
    {
        std::string name;
        std::string globalId;
        daq::ComponentPtr component;
        std::vector<std::string> inputs;
        std::vector<std::string> outputs;
        std::vector<Property> properties;
        std::vector<Topology> children;

        void print() const
        {
            std::cout << name << " (" << globalId << ")" << std::endl;
            for (const auto& input : inputs)
                std::cout << "  - " << input << " (Input)" << std::endl;
            for (const auto& output : outputs)
                std::cout << "  - " << output << " (Output)" << std::endl;
            // for (const auto& property : properties)
            // {
            //     std::cout << "  - " << property.name << " (" << property.value->toString().toStdString() << ')' << std::endl;
            // }
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

