#pragma once
#include <opendaq/opendaq.h>
#include "nodes.h"
#include <vector>
#include <optional>
#include <string>


struct OpenDAQComponent
{
    daq::ComponentPtr component_;
    daq::ComponentPtr parent_;
    std::vector<ImGui::ImGuiNodesIdentifier> input_ports_;
    std::vector<ImGui::ImGuiNodesIdentifier> output_signals_;

    OpenDAQComponent(daq::ComponentPtr component) {};
};

class OpenDAQHandler
{
public:
    OpenDAQHandler();
    void RetrieveTopology(daq::ComponentPtr component, ImGui::ImGuiNodes& nodes, std::string parent_id = "");

    daq::InstancePtr instance_;
    std::unordered_map<std::string, OpenDAQComponent> folders_{};
};

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

