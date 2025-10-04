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
    int color_index_ = 0;
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

    int next_color_index_ = 1; // 0 is used by the instance
    static constexpr ImColor color_palette_[] = {
        ImColor(0.4f, 0.6f, 0.9f, 1.0f),  // Blue
        ImColor(0.9f, 0.5f, 0.4f, 1.0f),  // Orange
        ImColor(0.5f, 0.8f, 0.5f, 1.0f),  // Green
        ImColor(0.9f, 0.6f, 0.8f, 1.0f),  // Pink
        ImColor(0.8f, 0.8f, 0.4f, 1.0f),  // Yellow
        ImColor(0.6f, 0.4f, 0.9f, 1.0f),  // Purple
        ImColor(0.4f, 0.8f, 0.9f, 1.0f),  // Cyan
        ImColor(0.9f, 0.7f, 0.4f, 1.0f),  // Amber
    };
    static constexpr int color_palette_size_ = sizeof(color_palette_) / sizeof(color_palette_[0]);
};

class OpenDAQNodeInteractionHandler: public ImGui::ImGuiNodesInteractionHandler
{
public:
    OpenDAQNodeInteractionHandler(OpenDAQHandler* opendaq_handler);
    void OnConnectionCreated(const ImGui::ImGuiNodesUid& output_id, const ImGui::ImGuiNodesUid& input_id) override;
    void OnOutputHover(const ImGui::ImGuiNodesUid& id) override;
    void OnInputHover(const ImGui::ImGuiNodesUid& id) override;
    void OnSelectionChanged(const std::vector<ImGui::ImGuiNodesUid>& selected_ids) override;
    void RenderPopupMenu(ImGui::ImGuiNodes* nodes, ImVec2 position) override;
    void OnAddButtonClick(const ImGui::ImGuiNodesUid& parent_node_id, std::optional<ImVec2> position) override;
    void OnInputDropped(const ImGui::ImGuiNodesUid& input_uid, std::optional<ImVec2> /*position*/) override;
    void OnEmptySpaceClick(ImVec2 position) override;
    void RenderNestedNodePopup(ImGui::ImGuiNodes* nodes);
    void ShowStartupPopup(ImGui::ImGuiNodes* nodes);

    void RenderFunctionBlockOptions(ImGui::ImGuiNodes* nodes, daq::ComponentPtr parent_component, const std::string& parent_id, ImVec2 position);
    void RenderDeviceOptions(ImGui::ImGuiNodes* nodes, daq::ComponentPtr parent_component, const std::string& parent_id, ImVec2 position);

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

