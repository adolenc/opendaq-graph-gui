#pragma once
#include <opendaq/opendaq.h>

class TreeViewWindow
{
public:
    void Render(const daq::ComponentPtr& component);

private:
    void RenderTreeNode(const daq::ComponentPtr& component);
};
