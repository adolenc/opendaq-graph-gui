#pragma once
#include <opendaq/opendaq.h>
#include <vector>
#include <string>


class PropertiesWindow
{
public:
    void Draw(const std::vector<daq::ComponentPtr>& selected_components);
    
private:
    void RenderProperty(daq::PropertyPtr property, daq::PropertyObjectPtr property_holder);
    std::string SampleTypeToString(daq::SampleType sample_type);
    std::string CoreTypeToString(daq::CoreType core_type);
    void RenderDescriptorAttribute(const std::string& name, const daq::BaseObjectPtr& value, int depth);
    void RenderAllDescriptorAttributes(const daq::DataDescriptorPtr& descriptor, const std::string& title);
    void RenderComponentPropertiesAndAttributes(const daq::ComponentPtr& component, bool show_attributes);
    void RenderSelectedComponent(daq::ComponentPtr component, bool show_parents, bool show_attributes);
    
    bool show_parents_ = false;
    bool tabbed_interface_ = false;
    bool show_attributes_ = false;
};
