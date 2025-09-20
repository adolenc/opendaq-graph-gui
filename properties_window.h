#pragma once
#include <opendaq/opendaq.h>
#include <vector>


void RenderSelectedComponent(daq::ComponentPtr component, bool show_parents);
void RenderComponentProperties(const daq::ComponentPtr& component);
void DrawPropertiesWindow(const std::vector<daq::ComponentPtr>& selected_components);
