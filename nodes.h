#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"
#include "imgui_internal.h"
#include <vector>
#include <string>

namespace ImGui
{

enum ImGuiNodesConnectorStateFlag_
{
    ImGuiNodesConnectorStateFlag_Default	= 0,
    ImGuiNodesConnectorStateFlag_Visible	= 1 << 0,
    ImGuiNodesConnectorStateFlag_Hovered	= 1 << 1,
    ImGuiNodesConnectorStateFlag_ConsideredAsDropTarget	= 1 << 2,
    ImGuiNodesConnectorStateFlag_Dragging	= 1 << 3,
    ImGuiNodesConnectorStateFlag_Selected	= 1 << 4
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
    ImGuiNodesNodeStateFlag_Warning              = 1 << 6
};

enum ImGuiNodesState_
{
    ImGuiNodesState_Default	= 0,
    ImGuiNodesState_HoveringNode,
    ImGuiNodesState_HoveringInput,
    ImGuiNodesState_HoveringOutput,
    ImGuiNodesState_Dragging,
    ImGuiNodesState_DraggingInput,
    ImGuiNodesState_DraggingOutput,
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
    float title_height_;
    float body_height_;
    ImGuiNodesNodeState state_;
    std::string name_;
    ImColor color_;
    std::string warning_message_;
    std::vector<ImGuiNodesInput> inputs_;
    std::vector<ImGuiNodesOutput> outputs_;

    ImGuiNodesNode(const ImGuiNodesIdentifier& name, ImColor color);
    void TranslateNode(ImVec2 delta, bool selected_only = false);
    void BuildNodeGeometry(ImVec2 inputs_size, ImVec2 outputs_size);
    void Render(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const;
};

struct ImGuiNodes;

class ImGuiNodesInteractionHandler
{
public:
    virtual void OnOutputHover(const ImGuiNodesUid& id) {};
    virtual void OnSelectionChanged(const std::vector<ImGuiNodesUid>& selected_ids) {}
    virtual void OnConnectionCreated(const ImGuiNodesUid& output_id, const ImGuiNodesUid& input_id) {}
    virtual void RenderPopupMenu(ImGuiNodes* nodes, ImVec2 position) {}
};

struct ImGuiNodes
{
public:
    ImGuiNodes(ImGuiNodesInteractionHandler* interaction_handler = nullptr);
    ~ImGuiNodes();

    void Update();
    void AddNode(const ImGuiNodesIdentifier& name, ImColor color,
                 const std::vector<ImGuiNodesIdentifier>& inputs,
                 const std::vector<ImGuiNodesIdentifier>& outputs,
                 ImGuiNodesUid parent_uid = "");
    void AddNode(const ImGuiNodesIdentifier& name, ImColor color, ImVec2 pos, 
                 const std::vector<ImGuiNodesIdentifier>& inputs,
                 const std::vector<ImGuiNodesIdentifier>& outputs,
                 ImGuiNodesUid parent_uid = "");
    void SetWarning(const ImGuiNodesUid& uid, const std::string& message);
    void SetOk(const ImGuiNodesUid& uid);

private:
    ImVec2 nodes_imgui_window_pos_;
    ImVec2 nodes_imgui_window_size_;

    ImVec2 mouse_;
    ImGuiNodesState state_;
    ImVec2 scroll_;
    float scale_;

    ImRect active_dragging_selection_area_;
    ImVec4 active_dragging_connection_;
    ImGuiNodesNode* active_node_ = NULL;
    ImGuiNodesInput* active_input_ = NULL;
    ImGuiNodesOutput* active_output_ = NULL;

    ImGuiNodesInteractionHandler* interaction_handler_ = nullptr;

    ImVector<ImGuiNodesNode*> nodes_;

    void ProcessInteractions();
    void ProcessNodes();
    void ProcessContextMenu();
    void UpdateCanvasGeometry(ImDrawList* draw_list);
    ImGuiNodesNode* UpdateNodesFromCanvas();
    void RenderConnection(ImVec2 p1, ImVec2 p4, ImColor color, float thickness = 1.5f);
    inline bool SortSelectedNodesOrder();
    void ClearAllConnectorSelections();
};

}
