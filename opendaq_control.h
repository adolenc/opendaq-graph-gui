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
};

class OpenDAQHandler
{
public:
    OpenDAQHandler();
    void RetrieveTopology(daq::ComponentPtr component, ImGui::ImGuiNodes& nodes, std::string parent_id = "");

    daq::InstancePtr instance_;
    std::unordered_map<std::string, OpenDAQComponent> folders_{};
    std::unordered_map<std::string, OpenDAQComponent> input_ports_{};
    std::unordered_map<std::string, OpenDAQComponent> signals_{};
};

class OpenDAQNodeInteractionHandler: public ImGui::ImGuiNodesInteractionHandler
{
public:
    OpenDAQNodeInteractionHandler(OpenDAQHandler* opendaq_handler);
    void OnConnectionCreated(const ImGui::ImGuiNodesUid& output_id, const ImGui::ImGuiNodesUid& input_id) override;
    void OnOutputHover(const ImGui::ImGuiNodesUid& id) override;
    void OnSelectionChanged(const std::vector<ImGui::ImGuiNodesUid>& selected_ids) override;
    void RenderPopupMenu(ImGui::ImGuiNodes* nodes, ImVec2 position) override;
    void OnAddButtonClick(const ImGui::ImGuiNodesUid& parent_node_id, std::optional<ImVec2> position) override;
    void OnInputDropped(const ImGui::ImGuiNodesUid& input_uid, std::optional<ImVec2> /*position*/) override;
    void RenderNestedNodePopup(ImGui::ImGuiNodes* nodes);
    void ShowStartupPopup(ImGui::ImGuiNodes* nodes);

    std::vector<daq::ComponentPtr> selected_components_;
    
    daq::ComponentPtr add_button_click_component_;
    std::optional<ImVec2> add_button_drop_position_;

    daq::ComponentPtr dragged_input_port_component_;

    OpenDAQHandler* opendaq_handler_;
    
    daq::ListPtr<daq::IDeviceInfo> available_devices_;
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

