#pragma once
#include <opendaq/opendaq.h>
#include <vector>
#include <string>

class PropertiesWindow
{
public:
    void Render();
    void OnSelectionChanged(const std::vector<daq::ComponentPtr>& selected_components);

    static std::string SampleTypeToString(daq::SampleType sample_type);
    static std::string CoreTypeToString(daq::CoreType core_type);
    static std::string OperationModeToString(daq::OperationModeType mode);
    
private:
    void RenderProperty(daq::PropertyPtr property, daq::PropertyObjectPtr property_holder);
    void RenderDescriptorAttribute(const std::string& name, const daq::BaseObjectPtr& value, int depth);
    void RenderAllDescriptorAttributes(const daq::DataDescriptorPtr& descriptor, const std::string& title);
    void RenderComponentPropertiesAndAttributes(const daq::ComponentPtr& component);
    void RenderSelectedComponent(const daq::ComponentPtr& component);
    
    std::vector<daq::ComponentPtr> selected_components_;
    bool show_parents_ = false;
    bool tabbed_interface_ = false;
    bool show_attributes_ = false;
};
