#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"
#include "imgui_internal.h"
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>

namespace ImGui
{

enum ImGuiNodesConnectorStateFlag_
{
    ImGuiNodesConnectorStateFlag_Default	= 0,
    ImGuiNodesConnectorStateFlag_Visible	= 1 << 0,
    ImGuiNodesConnectorStateFlag_Hovered	= 1 << 1,
    ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget	= 1 << 2,
    ImGuiNodesConnectorStateFlag_Dragging	= 1 << 3,
    ImGuiNodesConnectorStateFlag_Selected	= 1 << 4,
    ImGuiNodesConnectorStateFlag_Inactive   = 1 << 5
};

enum ImGuiNodesNodeStateFlag_
{
    ImGuiNodesNodeStateFlag_Default              = 0,
    ImGuiNodesNodeStateFlag_Visible              = 1 << 0,
    ImGuiNodesNodeStateFlag_Hovered              = 1 << 1,
    ImGuiNodesNodeStateFlag_MarkedForSelection   = 1 << 2,  // Temporary state during box selection
    ImGuiNodesNodeStateFlag_Selected             = 1 << 3,
    ImGuiNodesNodeStateFlag_Collapsed            = 1 << 4,
    ImGuiNodesNodeStateFlag_Disabled             = 1 << 5,
    ImGuiNodesNodeStateFlag_Warning              = 1 << 6,
    ImGuiNodesNodeStateFlag_Error                = 1 << 7,
    ImGuiNodesNodeStateFlag_Inactive             = 1 << 8
};

enum ImGuiNodesState_
{
    ImGuiNodesState_Default	= 0,
    ImGuiNodesState_HoveringNode,
    ImGuiNodesState_HoveringInput,
    ImGuiNodesState_HoveringOutput,
    ImGuiNodesState_HoveringAddButton,
    ImGuiNodesState_HoveringActiveButton,
    ImGuiNodesState_HoveringTrashButton,
    ImGuiNodesState_HoveringOutputActiveButton,
    ImGuiNodesState_Dragging,
    ImGuiNodesState_DraggingInput,
    ImGuiNodesState_DraggingOutput,
    ImGuiNodesState_DraggingParentConnection,
    ImGuiNodesState_Selecting
};

typedef std::string ImGuiNodesUid;
typedef unsigned int ImGuiNodesConnectorState;
typedef unsigned int ImGuiNodesNodeState;

typedef unsigned int ImGuiNodesState;

// connector text name heights factors
constexpr float ImGuiNodesConnectorDotDiameter = 0.7f; // connectors dot diameter
constexpr float ImGuiNodesConnectorDotPadding = 0.35f; // connectors dot left/right sides padding
constexpr float ImGuiNodesConnectorDistance = 1.5f; // vertical distance between connectors centers

// title text name heights factors
constexpr float ImGuiNodesHSeparation = 1.7f; // extra horizontal separation inbetween IOs
constexpr float ImGuiNodesVSeparation = 1.5f; // total IOs area separation from title and node bottom edge
constexpr float ImGuiNodesTitleHight = 2.0f;

struct ImGuiNodesNode;
struct ImGuiNodesInput;
struct ImGuiNodesOutput;

struct ImGuiNodesIdentifier
{
    ImGuiNodesUid id_;
    std::string name_;

    ImGuiNodesIdentifier(const std::string& name, ImGuiNodesUid id) : name_(name), id_(id) { }
    ImGuiNodesIdentifier(const std::string& name) : ImGuiNodesIdentifier(name, std::to_string(id_counter_++)) { }
    ImGuiNodesIdentifier(const char* name) : ImGuiNodesIdentifier(std::string(name)) { }
private:
    static unsigned int id_counter_;
};

struct ImGuiNodesInput
{
    ImGuiNodesUid uid_;

    ImVec2 pos_;
    ImRect area_input_;
    ImRect area_name_;
    ImGuiNodesConnectorState state_;
    std::string name_;
    ImGuiNodesNode* source_node_;
    ImGuiNodesOutput* source_output_;
    std::optional<ImColor> connection_color_;

    ImGuiNodesInput(const ImGuiNodesIdentifier& name);
    void TranslateInput(ImVec2 delta);
    void Render(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const;
};

struct ImGuiNodesOutput
{
    ImGuiNodesUid uid_;

    ImVec2 pos_;
    ImRect area_output_;
    ImRect area_name_;
    ImRect area_active_button_;
    ImGuiNodesConnectorState state_;
    std::string name_;
    unsigned int connections_count_;

    ImGuiNodesOutput(const ImGuiNodesIdentifier& name);
    void TranslateOutput(ImVec2 delta);
    void Render(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const;
};

struct ImGuiNodesNode
{
    ImGuiNodesUid uid_;
    ImGuiNodesNode* parent_node_;

    ImRect area_node_;
    ImRect area_name_;
    ImRect area_add_button_;
    ImRect area_active_button_;
    ImRect area_trash_button_;
    float title_height_;
    float body_height_;
    ImGuiNodesNodeState state_;
    std::string name_;
    int color_index_;
    std::string warning_message_;
    std::string error_message_;
    std::vector<ImGuiNodesInput> inputs_;
    std::vector<ImGuiNodesOutput> outputs_;

    ImGuiNodesNode(const ImGuiNodesIdentifier& name, int color_index);
    void TranslateNode(ImVec2 delta, bool selected_only = false);
    void BuildNodeGeometry(ImVec2 inputs_size, ImVec2 outputs_size);
    void Render(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const;
};

struct ImGuiNodes;

class ImGuiNodesInteractionHandler
{
public:
    virtual void OnOutputHover(const ImGuiNodesUid& id) {};
    virtual void OnInputHover(const ImGuiNodesUid& id) {};
    virtual void OnSelectionChanged(const std::vector<ImGuiNodesUid>& selected_ids) {}
    virtual void OnConnectionCreated(const ImGuiNodesUid& output_id, const ImGuiNodesUid& input_id) {}
    virtual void OnConnectionRemoved(const ImGuiNodesUid& input_id) {}
    virtual void RenderPopupMenu(ImGuiNodes* nodes, ImVec2 position) {}
    virtual void OnAddButtonClick(const ImGuiNodesUid& parent_node_id, std::optional<ImVec2> position) {}
    virtual void OnNodeActiveToggle(const ImGuiNodesUid& uid) {}
    virtual void OnNodeTrashClick(const ImGuiNodesUid& uid) {}
    virtual void OnSignalActiveToggle(const ImGuiNodesUid& uid) {}
    virtual void OnInputDropped(const ImGuiNodesUid& input_uid, std::optional<ImVec2> position) {}
    virtual void OnEmptySpaceClick(ImVec2 position) {}
};

struct ImGuiNodes
{
public:
    ImGuiNodes(ImGuiNodesInteractionHandler* interaction_handler = nullptr);
    ~ImGuiNodes();

    void Update();
    void AddNode(const ImGuiNodesIdentifier& name, int color_index,
                 const std::vector<ImGuiNodesIdentifier>& inputs,
                 const std::vector<ImGuiNodesIdentifier>& outputs,
                 ImGuiNodesUid parent_uid = "");
    void AddNode(const ImGuiNodesIdentifier& name, int color_index, ImVec2 pos, 
                 const std::vector<ImGuiNodesIdentifier>& inputs,
                 const std::vector<ImGuiNodesIdentifier>& outputs,
                 ImGuiNodesUid parent_uid = "");
    void BeginBatchAdd();
    void EndBatchAdd();
    void AddConnection(const ImGuiNodesUid& output_uid, const ImGuiNodesUid& input_uid, std::optional<ImColor> color = std::nullopt);
    void SetConnectionColor(const ImGuiNodesUid& input_uid, std::optional<ImColor> color);
    void RemoveConnection(const ImGuiNodesUid& input_uid);
    void SetWarning(const ImGuiNodesUid& uid, const std::string& message);
    void SetError(const ImGuiNodesUid& uid, const std::string& message);
    void SetOk(const ImGuiNodesUid& uid);
    void SetActive(const ImGuiNodesUid& uid, bool active);
    void SetSelectedNodes(const std::vector<ImGuiNodesUid>& selected_ids);
    void MoveSelectedNodesIntoView();
    void ClearNodeConnections(const ImGuiNodesUid& node_uid);
    void Clear();

    static constexpr ImColor text_color_ = ImColor(0xff000000);
    static constexpr ImColor connection_color_ = ImColor(0xffffffff);
    static constexpr ImColor parent_connection_color_ = ImColor(0x20ffffff);
    static constexpr ImColor warning_color_ = ImColor(0xff0060ff);
    static constexpr ImColor error_color_ = ImColor(0xff0719eb);
    static constexpr ImColor color_palette_[] = {
        ImColor(0xffffd670),
        ImColor(0xff70d6ff),
        ImColor(0xff7097ff),
        ImColor(0xff70ffe9),
        ImColor(0xffa670ff),
    };
    static constexpr int color_palette_size_ = sizeof(color_palette_) / sizeof(color_palette_[0]);

private:
    ImVec2 nodes_imgui_window_pos_;
    ImVec2 nodes_imgui_window_size_;

    ImVec2 mouse_;
    ImGuiNodesState state_;
    ImVec2 scroll_;
    float scale_;

    ImRect active_dragging_selection_area_;
    ImVec4 active_dragging_connection_;
    ImRect minimap_rect_;
    float minimap_preview_scale_ = 1.0f;
    ImGuiNodesNode* active_node_ = NULL;
    ImGuiNodesInput* active_input_ = NULL;
    ImGuiNodesOutput* active_output_ = NULL;

    ImGuiNodesInteractionHandler* interaction_handler_ = nullptr;

    struct OutputWithOwner
    {
        ImGuiNodesOutput* output;
        ImGuiNodesNode* node;
    };

    ImVector<ImGuiNodesNode*> nodes_;
    std::unordered_map<ImGuiNodesUid, ImGuiNodesNode*> nodes_by_uid_;
    std::unordered_map<ImGuiNodesUid, ImGuiNodesInput*> inputs_by_uid_;
    std::unordered_map<ImGuiNodesUid, OutputWithOwner> outputs_by_uid_;

    // cache for node positions and colors between deletions
    struct NodeCacheEntry
    {
        ImVec2 pos;
        int color_index;
        bool is_selected;
    };
    std::unordered_map<ImGuiNodesUid, NodeCacheEntry> node_cache_;
    bool batch_add_mode_ = false;

    void ProcessInteractions();
    void ProcessNodes();
    void UpdateCanvasGeometry(ImDrawList* draw_list);
    ImGuiNodesNode* UpdateNodesFromCanvas();
    void RenderConnection(ImVec2 p1, ImVec2 p4, ImColor color, float thickness = 1.5f);
    void RenderMinimap(ImDrawList* draw_list);
    inline bool SortSelectedNodesOrder();
    void ClearAllConnectorSelections();
    ImVec2 UpdateEdgeScrolling();
};

}
