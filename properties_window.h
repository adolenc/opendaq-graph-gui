#pragma once
#include <opendaq/opendaq.h>
#include <vector>


void RenderSelectedComponent(daq::ComponentPtr component, bool show_parents, bool show_attributes);
void RenderComponentPropertiesAndAttributes(const daq::ComponentPtr& component, bool show_attributes);
void DrawPropertiesWindow(const std::vector<daq::ComponentPtr>& selected_components);
