#pragma once
#include <opendaq/opendaq.h>
#include "nodes.h"
#include "properties_window.h"
#include "component_cache.h"
#include "signals_window.h"
#include <vector>
#include <optional>
#include <string>
#include <memory>


class OpenDAQNodeEditor : public ImGui::ImGuiNodesInteractionHandler
{
public:
    OpenDAQNodeEditor();
    void RetrieveTopology(daq::ComponentPtr component, std::string parent_id = "");
    void OnConnectionCreated(const ImGui::ImGuiNodesUid& output_id, const ImGui::ImGuiNodesUid& input_id) override;
    void OnOutputHover(const ImGui::ImGuiNodesUid& id) override;
    void OnInputHover(const ImGui::ImGuiNodesUid& id) override;
    void OnSelectionChanged(const std::vector<ImGui::ImGuiNodesUid>& selected_ids) override;
    void RenderPopupMenu(ImGui::ImGuiNodes* nodes, ImVec2 position) override;
    void OnAddButtonClick(const ImGui::ImGuiNodesUid& parent_node_id, std::optional<ImVec2> position) override;
    void OnInputDropped(const ImGui::ImGuiNodesUid& input_uid, std::optional<ImVec2> /*position*/) override;
    void OnEmptySpaceClick(ImVec2 position) override;
    void RenderNestedNodePopup();
    void ShowStartupPopup();
    void RetrieveConnections();
    void Render();

    void RenderFunctionBlockOptions(daq::ComponentPtr parent_component, const std::string& parent_id, ImVec2 position);
    void RenderDeviceOptions(daq::ComponentPtr parent_component, const std::string& parent_id, ImVec2 position);

    daq::InstancePtr instance_;
    std::unordered_map<std::string, std::unique_ptr<CachedComponent>> all_components_;
    std::unordered_map<std::string, CachedComponent*> folders_;
    std::unordered_map<std::string, CachedComponent*> input_ports_;
    std::unordered_map<std::string, CachedComponent*> signals_;

    std::vector<std::string> selected_ids_;
    
    daq::ComponentPtr add_button_click_component_;
    std::optional<ImVec2> add_button_drop_position_;

    daq::ComponentPtr dragged_input_port_component_;

    daq::ListPtr<daq::IDeviceInfo> available_devices_;

    bool fb_options_cache_valid_ = false;
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> cached_available_fbs_;

    ImGui::ImGuiNodes* nodes_ = nullptr;

    PropertiesWindow properties_window_;
    SignalsWindow signals_window_;

    int next_color_index_ = 1;
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

