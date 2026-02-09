#pragma once
#include <opendaq/opendaq.h>
#include "nodes.h"
#include "properties_window.h"
#include "component_cache.h"
#include "signals_window.h"
#include "tree_view_window.h"
#include <vector>
#include <optional>
#include <string>
#include <memory>
#include <future>
#include <atomic>
#include <chrono>


class OpenDAQNodeEditor
{
public:
    OpenDAQNodeEditor();
    void Init();
    void InitImGui();
    void RetrieveTopology(daq::ComponentPtr component, std::string parent_id = "", daq::ComponentPtr owner = nullptr);
    void OnConnectionCreated(const ImGui::ImGuiNodesUid& output_id, const ImGui::ImGuiNodesUid& input_id);
    void OnConnectionRemoved(const ImGui::ImGuiNodesUid& input_id);
    void OnOutputHover(const ImGui::ImGuiNodesUid& id);
    void OnInputHover(const ImGui::ImGuiNodesUid& id);
    void OnSelectionChanged(const std::vector<ImGui::ImGuiNodesUid>& selected_ids);
    void RenderPopupMenu(ImGui::ImGuiNodes* nodes, ImVec2 position);
    void OnAddButtonClick(const ImGui::ImGuiNodesUid& parent_node_id, std::optional<ImVec2> position);
    void OnNodeActiveToggle(const ImGui::ImGuiNodesUid& uid);
    void OnNodeDelete(const std::vector<ImGui::ImGuiNodesUid>& uids);
    void OnSignalActiveToggle(const ImGui::ImGuiNodesUid& uid);
    void OnInputDropped(const ImGui::ImGuiNodesUid& input_uid, std::optional<ImVec2> position);
    void OnEmptySpaceClick(ImVec2 position);
    void RenderNestedNodePopup();
    void ShowStartupPopup();
    void RetrieveConnections();
    void RebuildNodeConnections(const std::string& node_id);
    void RebuildStructure();
    void RebuildNodeGeometry();
    void Render();
    void SetNodeActiveRecursively(const std::string& node_id);
    void UpdateSignalsActiveState(CachedComponent* cached);

    void DeleteComponent(CachedComponent* component);
    void RenderFunctionBlockOptions(daq::ComponentPtr parent_component, const std::string& parent_id, std::optional<ImVec2> position);
    void RenderDeviceOptions(daq::ComponentPtr parent_component, const std::string& parent_id, std::optional<ImVec2> position);
    void BuildPopupParentCandidates(const std::string& parent_guid, int depth = 0, int parent_color_index = 0);

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
    std::future<daq::ListPtr<daq::IDeviceInfo>> device_discovery_future_;
    bool is_device_discovery_initialized_ = false;
    bool auto_refresh_devices_ = true;
    std::chrono::steady_clock::time_point last_refresh_time_;

    bool fb_options_cache_valid_ = false;
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> cached_available_fbs_;
    std::string popup_selected_parent_guid_;
    struct PopupParentCandidate
    {
        std::string display_name;
        std::string guid;
        CachedComponent* cached;
        int color_index;
        int depth;
    };
    std::vector<PopupParentCandidate> popup_parent_candidates_;

    ImGui::ImGuiNodes nodes_;

    PropertiesWindow properties_window_;
    SignalsWindow signals_window_;
    std::vector<std::unique_ptr<SignalsWindow>> cloned_signals_windows_;
    std::vector<std::unique_ptr<PropertiesWindow>> cloned_properties_windows_;
    TreeViewWindow tree_view_window_;

    static constexpr ImColor node_color_palette_[] = {
        ImColor(0xffffd670),
        ImColor(0xff70d6ff),
        ImColor(0xff7097ff),
        ImColor(0xff70ffe9),
        ImColor(0xffa670ff),
    };
    static constexpr int node_color_palette_size_ = sizeof(node_color_palette_) / sizeof(node_color_palette_[0]);
    ImColor GetNodeColor(int color_index) const { return node_color_palette_[color_index % node_color_palette_size_]; }

    int next_color_index_ = 1;

    std::mutex event_mutex_;
    std::vector<std::pair<daq::ComponentPtr, daq::CoreEventArgsPtr>> event_id_queue_;
};
