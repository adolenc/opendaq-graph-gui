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
    ImGuiNodesConnectorStateFlag_Consider	= 1 << 2,
    ImGuiNodesConnectorStateFlag_Dragging	= 1 << 3,
    ImGuiNodesConnectorStateFlag_Selected	= 1 << 4
};

enum ImGuiNodesNodeStateFlag_
{
    ImGuiNodesNodeStateFlag_Default			= 0,
    ImGuiNodesNodeStateFlag_Visible			= 1 << 0,
    ImGuiNodesNodeStateFlag_Hovered			= 1 << 1,
    ImGuiNodesNodeStateFlag_MarkedForSelection	= 1 << 2,  // Temporary state during box selection
    ImGuiNodesNodeStateFlag_Selected		= 1 << 3,
    ImGuiNodesNodeStateFlag_Collapsed		= 1 << 4,
    ImGuiNodesNodeStateFlag_Disabled		= 1 << 5
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

struct ImGuiNodesInput
{
    ImVec2 pos_;
    ImRect area_input_;
    ImRect area_name_;
    ImGuiNodesConnectorState state_;
    std::string name_;
    ImGuiNodesNode* source_node_;
    ImGuiNodesOutput* source_output_;

    ImGuiNodesInput(const std::string& name);
    void TranslateInput(ImVec2 delta);
    void Render(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const;
};

struct ImGuiNodesOutput
{
    ImVec2 pos_;
    ImRect area_output_;
    ImRect area_name_;
    ImGuiNodesConnectorState state_;
    std::string name_;
    unsigned int connections_count_;

    ImGuiNodesOutput(const std::string& name);
    void TranslateOutput(ImVec2 delta);
    void Render(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const;
};

struct ImGuiNodesNode
{
    ImRect area_node_;
    ImRect area_name_;
    float title_height_;
    float body_height_;
    ImGuiNodesNodeState state_;
    std::string name_;
    ImColor color_;
    std::vector<ImGuiNodesInput> inputs_;
    std::vector<ImGuiNodesOutput> outputs_;

    ImGuiNodesNode(const std::string& name, ImColor color);
    void TranslateNode(ImVec2 delta, bool selected_only = false);
    void BuildNodeGeometry(ImVec2 inputs_size, ImVec2 outputs_size);
    void Render(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const;
};

struct ImGuiNodes
{
public:
    ImGuiNodes();
    ~ImGuiNodes();

    void Update();
    void ProcessNodes();
    void ProcessContextMenu();

private:
    ImVec2 mouse_;
    ImRect area_;
    ImVec2 pos_;
    ImVec2 size_;
    ImVec2 scroll_;
    float scale_;

    ImGuiNodesState state_;

    ImVec4 active_dragging_connection_;
    ImGuiNodesNode* active_node_ = NULL;
    ImGuiNodesInput* active_input_ = NULL;
    ImGuiNodesOutput* active_output_ = NULL;

    ImVector<ImGuiNodesNode*> nodes_;

    void UpdateCanvasGeometry(ImDrawList* draw_list);
    ImGuiNodesNode* UpdateNodesFromCanvas();
    ImGuiNodesNode* CreateNode(const std::string& name, ImColor color, ImVec2 pos, 
                               const std::vector<std::string>& inputs, const std::vector<std::string>& outputs);
    void RenderConnection(ImVec2 p1, ImVec2 p4, ImColor color);
    inline bool SortSelectedNodesOrder();
    void ClearAllConnectorSelections();
};

}
