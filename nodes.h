#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"
#include "imgui_internal.h"

namespace ImGui
{

enum ImGuiNodesNodeType_
{
    ImGuiNodesNodeType_None = 0,
    ImGuiNodesNodeType_Generic,
    ImGuiNodesNodeType_Generator,
    ImGuiNodesNodeType_Test
};

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
    ImGuiNodesNodeStateFlag_Marked			= 1 << 2,
    ImGuiNodesNodeStateFlag_Selected		= 1 << 3,
    ImGuiNodesNodeStateFlag_Collapsed		= 1 << 4,
    ImGuiNodesNodeStateFlag_Disabled		= 1 << 5,
    ImGuiNodesNodeStateFlag_Processing		= 1 << 6 
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

typedef unsigned int ImGuiNodesNodeType;

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
    const char* name_;
    ImGuiNodesNode* target_;
    ImGuiNodesOutput* output_;

    void TranslateInput(ImVec2 delta);

    void DrawInput(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const;

    ImGuiNodesInput(const char* name);
};

struct ImGuiNodesOutput
{
    ImVec2 pos_;
    ImRect area_output_;
    ImRect area_name_;
    ImGuiNodesConnectorState state_;
    const char* name_;
    unsigned int connections_;

    void TranslateOutput(ImVec2 delta);

    void DrawOutput(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const;

    ImGuiNodesOutput(const char* name);
};

struct ImGuiNodesNode
{
    ImRect area_node_;
    ImRect area_name_;
    float title_height_;
    float body_height_;
    ImGuiNodesNodeState state_;
    ImGuiNodesNodeType type_;
    const char* name_;
    ImColor color_;
    ImVector<ImGuiNodesInput> inputs_;
    ImVector<ImGuiNodesOutput> outputs_;

    void TranslateNode(ImVec2 delta, bool selected_only = false);

    void BuildNodeGeometry(ImVec2 inputs_size, ImVec2 outputs_size);

    void DrawNode(ImDrawList* draw_list, ImVec2 offset, float scale, ImGuiNodesState state) const;

    ImGuiNodesNode(const char* name, ImGuiNodesNodeType type, ImColor color);
};

//ImGuiNodesConnectionDesc size round up to 32 bytes to be cache boundaries friendly
constexpr int ImGuiNodesNamesMaxLen = 32;

struct ImGuiNodesConnectionDesc
{
    char name_[ImGuiNodesNamesMaxLen];
};

//TODO: ImVector me
struct ImGuiNodesNodeDesc
{
    char name_[ImGuiNodesNamesMaxLen];
    ImGuiNodesNodeType type_;
    ImColor color_;
    ImVector<ImGuiNodesConnectionDesc> inputs_;
    ImVector<ImGuiNodesConnectionDesc> outputs_;
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
    ImGuiNodesNode* processing_node_ = NULL;

    ImVector<ImGuiNodesNode*> nodes_;
    ImVector<ImGuiNodesNodeDesc> nodes_desc_;

private:
    void UpdateCanvasGeometry(ImDrawList* draw_list);
    ImGuiNodesNode* UpdateNodesFromCanvas();
    ImGuiNodesNode* CreateNodeFromDesc(ImGuiNodesNodeDesc* desc, ImVec2 pos);

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
