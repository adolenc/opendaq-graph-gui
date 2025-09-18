#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"
#include "imgui_internal.h"
#include <vector>
#include <string>
#include <string>

namespace ImGui
{

enum ImGuiNodesConnectorStateFlag_
{
    ImGuiNodesConnectorStateFlag_Default	= 0,
    ImGuiNodesConnectorStateFlag_Visible	= 1 << 0,
    ImGuiNodesConnectorStateFlag_Hovered	= 1 << 1,
    ImGuiNodesConnectorStateFlag_Consider	= 1 << 2,
    ImGuiNodesConnectorStateFlag_Draging	= 1 << 3
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
    ImGuiNodesState_Draging,
    ImGuiNodesState_DragingInput,
    ImGuiNodesState_DragingOutput,
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
    ImGuiNodesNode* target_;
    ImGuiNodesOutput* output_;

    void TranslateInput(ImVec2 delta);

    void DrawInput(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const;

    ImGuiNodesInput(const std::string& name);
};

struct ImGuiNodesOutput
{
    ImVec2 pos_;
    ImRect area_output_;
    ImRect area_name_;
    ImGuiNodesConnectorState state_;
    std::string name_;
    unsigned int connections_;

    void TranslateOutput(ImVec2 delta);

    void DrawOutput(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const;

    ImGuiNodesOutput(const std::string& name);
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

    void TranslateNode(ImVec2 delta, bool selected_only = false);

    void BuildNodeGeometry(ImVec2 inputs_size, ImVec2 outputs_size);

    void DrawNode(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const;

    ImGuiNodesNode(const std::string& name, ImColor color);
};



struct ImGuiNodes
{
private:
    ImVec2 mouse_;
    ImVec2 pos_;
    ImVec2 size_;
    ImVec2 scroll_;
    ImVec4 connection_;
    float scale_;

    ImGuiNodesState state_;

    ImRect area_;
    ImGuiNodesNode* element_node_ = NULL;
    ImGuiNodesInput* element_input_ = NULL;
    ImGuiNodesOutput* element_output_ = NULL;

    ImVector<ImGuiNodesNode*> nodes_;

private:
    void UpdateCanvasGeometry(ImDrawList* draw_list);
    ImGuiNodesNode* UpdateNodesFromCanvas();
    ImGuiNodesNode* CreateNode(const std::string& name, ImColor color, ImVec2 pos, 
                               const std::vector<std::string>& inputs, const std::vector<std::string>& outputs);

    void DrawConnection(ImVec2 p1, ImVec2 p4, ImColor color);

    bool ConnectionMatrix(ImGuiNodesNode* input_node, ImGuiNodesNode* output_node, ImGuiNodesInput* input, ImGuiNodesOutput* output);

    inline bool SortSelectedNodesOrder();
public:
    void Update();
    void ProcessNodes();
    void ProcessContextMenu();

    ImGuiNodes();

    ~ImGuiNodes();
};

}
