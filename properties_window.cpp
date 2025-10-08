#include "properties_window.h"
#include "opendaq_control.h"
#include <string>
#include <sstream>
#include "imgui.h"
#include "imgui_stdlib.h"


void RenderProperty(daq::PropertyPtr property, daq::PropertyObjectPtr property_holder)
{
    std::string prop_name = property.getName().toStdString();
    std::string prop_name_for_display = property.getName().toStdString();
    if (property.getUnit().assigned() && property.getUnit().getSymbol().assigned())
        prop_name_for_display += " [" + static_cast<std::string>(property.getUnit().getSymbol().toString()) + ']';

    if (property.getReadOnly())
        ImGui::BeginDisabled();

    switch (property.getValueType())
    {
        case daq::ctBool:
            {
                bool value = property_holder.getPropertyValue(prop_name);
                if (ImGui::Checkbox(prop_name_for_display.c_str(), &value))
                    property_holder.setPropertyValue(prop_name, value);
                break;
            }
        case daq::ctInt:
            {
                auto sv = property.getSelectionValues();
                if (sv.assigned()) // TODO: check if is a dict instead
                {
                    std::stringstream values;
                    daq::ListPtr<daq::IString> selection_values;
                    if (sv.supportsInterface<daq::IList>())
                        selection_values = sv;
                    else if (sv.supportsInterface<daq::IDict>())
                        selection_values = daq::DictPtr<daq::IInteger, daq::IString>(sv).getValueList();
                    else
                    {
                        ImGui::Text("!Unsupported selection values type for property: %s", prop_name_for_display.c_str());
                        break;
                    }
                    for (int i = 0; i < selection_values.getCount(); i++)
                        values << selection_values.getItemAt(i).toStdString() << '\0';
                    int value = (int64_t)property_holder.getPropertyValue(prop_name);
                    if (ImGui::Combo(prop_name_for_display.c_str(), &value, values.str().c_str(), selection_values.getCount()))
                        property_holder.setPropertyValue(prop_name, value);
                }
                else
                {
                    int value = (int64_t)property_holder.getPropertyValue(prop_name);
                    if (ImGui::InputInt(prop_name_for_display.c_str(), &value))
                        property_holder.setPropertyValue(prop_name, value);
                }
                break;
            }
        case daq::ctFloat:
            {
                double value = property_holder.getPropertyValue(prop_name);
                if (ImGui::InputDouble(prop_name_for_display.c_str(), &value))
                    property_holder.setPropertyValue(prop_name, value);
                break;
            }
        case daq::ctString:
            {
                std::string value = property_holder.getPropertyValue(prop_name);
                if (ImGui::InputText(prop_name_for_display.c_str(), &value))
                    property_holder.setPropertyValue(prop_name, value);
                break;
            }
        case daq::ctProc:
            {
                if (ImGui::Button(prop_name_for_display.c_str()))
                    property_holder.getPropertyValue(prop_name).asPtr<daq::IProcedure>().dispatch();
                break;
            }
        case daq::ctObject:
            {
                ImGui::Text("> %s", prop_name_for_display.c_str());
                ImGui::Indent();
                ImGui::PushID(prop_name.c_str());
                daq::PropertyObjectPtr parent = property_holder.getPropertyValue(prop_name);
                for (const auto& sub_property : parent.getVisibleProperties())
                    RenderProperty(sub_property, parent);
                ImGui::PopID();
                ImGui::Unindent();
                break;
            }
        default:
            {
                std::string n = "!Unsupported prop t" + std::to_string(property.getValueType()) + ": " + prop_name_for_display;
                ImGui::Text("%s", n.c_str());
                break;
            }
    }

    if (property.getReadOnly())
        ImGui::EndDisabled();
}

std::string SampleTypeToString(daq::SampleType sample_type)
{
    switch (sample_type)
    {
        case daq::SampleType::Undefined: return "daq::Undefined";
        case daq::SampleType::Float32: return "daq::Float32";
        case daq::SampleType::Float64: return "daq::Float64";
        case daq::SampleType::UInt8: return "daq::UInt8";
        case daq::SampleType::Int8: return "daq::Int8";
        case daq::SampleType::UInt16: return "daq::UInt16";
        case daq::SampleType::Int16: return "daq::Int16";
        case daq::SampleType::UInt32: return "daq::UInt32";
        case daq::SampleType::Int32: return "daq::Int32";
        case daq::SampleType::UInt64: return "daq::UInt64";
        case daq::SampleType::Int64: return "daq::Int64";
        case daq::SampleType::RangeInt64: return "daq::RangeInt64";
        case daq::SampleType::ComplexFloat32: return "daq::ComplexFloat32";
        case daq::SampleType::ComplexFloat64: return "daq::ComplexFloat64";
        case daq::SampleType::Binary: return "daq::Binary";
        case daq::SampleType::String: return "daq::String";
        case daq::SampleType::Struct: return "daq::Struct";
        default: return "Unknown (" + std::to_string(static_cast<int>(sample_type)) + ")";
    }
}

std::string CoreTypeToString(daq::CoreType core_type)
{
    switch (core_type)
    {
        case daq::ctBool: return "daq::Bool";
        case daq::ctInt: return "daq::Int";
        case daq::ctFloat: return "daq::Float";
        case daq::ctString: return "daq::String";
        case daq::ctList: return "daq::List";
        case daq::ctDict: return "daq::Dict";
        case daq::ctRatio: return "daq::Ratio";
        case daq::ctProc: return "daq::Proc";
        case daq::ctObject: return "daq::Object";
        case daq::ctBinaryData: return "daq::BinaryData";
        case daq::ctFunc: return "daq::Func";
        case daq::ctComplexNumber: return "daq::ComplexNumber";
        case daq::ctStruct: return "daq::Struct";
        case daq::ctEnumeration: return "daq::Enumeration";
        case daq::ctUndefined: return "daq::Undefined";
        default: return "Unknown (" + std::to_string(static_cast<int>(core_type)) + ")";
    }
}

void RenderDescriptorAttribute(const std::string& name, const daq::BaseObjectPtr& value, int depth)
{
    if (!value.assigned())
    {
        ImGui::BeginDisabled();
        std::string null_str = "(null)";
        ImGui::InputText(name.c_str(), &null_str);
        ImGui::EndDisabled();
        return;
    }
    
    ImGui::BeginDisabled();
    
    if (value.supportsInterface<daq::IString>())
    {
        std::string str_value = value;
        ImGui::InputText(name.c_str(), &str_value);
    }
    else if (value.supportsInterface<daq::IInteger>())
    {
        int int_value = value;
        ImGui::InputInt(name.c_str(), &int_value);
    }
    else if (value.supportsInterface<daq::IFloat>())
    {
        double double_value = value;
        ImGui::InputDouble(name.c_str(), &double_value);
    }
    else if (value.supportsInterface<daq::IBoolean>())
    {
        bool bool_value = value;
        ImGui::Checkbox(name.c_str(), &bool_value);
    }
    else if (value.supportsInterface<daq::IRatio>())
    {
        daq::RatioPtr ratio_val = value;
        std::string ratio_str = std::to_string((long long)ratio_val.getNumerator())
                              + "/"
                              + std::to_string((long long)ratio_val.getDenominator());
        ImGui::InputText(name.c_str(), &ratio_str);
    }
    else if (value.supportsInterface<daq::IUnit>())
    {
        daq::UnitPtr unit_val = value;
        std::string symbol = unit_val.getSymbol().assigned() ? unit_val.getSymbol().toStdString() : "None";
        std::string quantity = unit_val.getQuantity().assigned() ? unit_val.getQuantity().toStdString() : "None";
        std::string unit_str = symbol + " (" + quantity + ")";
        ImGui::InputText(name.c_str(), &unit_str);
    }
    else if (value.supportsInterface<daq::IList>())
    {
        ImGui::EndDisabled();
        auto list_val = value.asPtr<daq::IList>();
        if (ImGui::TreeNodeEx((name + " (List)").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (int i = 0; i < list_val.getCount(); i++)
            {
                auto item = list_val.getItemAt(i);
                RenderDescriptorAttribute("[" + std::to_string(i) + "]", item, depth + 1);
            }
            ImGui::TreePop();
        }
        return;
    }
    else if (value.supportsInterface<daq::IDict>())
    {
        ImGui::EndDisabled();
        auto dict_val = value.asPtr<daq::IDict>();
        if (ImGui::TreeNodeEx((name + " (Dict)").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (const auto& key : dict_val.getKeyList())
            {
                auto dict_value = dict_val.get(key);
                std::string key_str = key.supportsInterface<daq::IString>() ? 
                    key.asPtr<daq::IString>().toStdString() : 
                    static_cast<std::string>(key.toString());
                RenderDescriptorAttribute(key_str, dict_value, depth + 1);
            }
            ImGui::TreePop();
        }
        return;
    }
    else
    {
        try
        {
            std::string str_rep = value;
            ImGui::InputText(name.c_str(), &str_rep);
        }
        catch (...)
        {
            std::string error_str = "<unable to convert>";
            ImGui::InputText(name.c_str(), &error_str);
        }
    }
    
    ImGui::EndDisabled();
}

void RenderAllDescriptorAttributes(const daq::DataDescriptorPtr& descriptor, const std::string& title)
{
    if (!descriptor.assigned())
    {
        ImGui::Text("No %s available", title.c_str());
        return;
    }

    ImGui::BeginDisabled();
        
        std::string text;
        try
        {
            auto core_type = descriptor.getSampleType();
            text = SampleTypeToString(core_type);
        } catch (...)
        {
            text = "<unavailable>";
        }
        ImGui::InputText("Core Type", &text);

        if (descriptor.getDimensions().assigned())
        {
            auto dimensions = descriptor.getDimensions();
            try
            {
                text = "[";
                for (int i = 0; i < dimensions.getCount(); i++)
                {
                    if (i > 0) text += ", ";
                    text += std::to_string((long long)dimensions.getItemAt(i).asPtr<daq::IInteger>());
                }
                text += "]";
            } catch (...)
            {
                text = "<error>";
            }
        }
        else
            text = "None";
        ImGui::InputText("Dimensions", &text);

        text = descriptor.getName().assigned() ? descriptor.getName().toStdString() : "None";
        ImGui::InputText("Name", &text);

        text = descriptor.getOrigin().assigned() ? descriptor.getOrigin().toStdString() : "None";
        ImGui::InputText("Origin", &text);

        text = "";
        if (descriptor.getPostScaling().assigned())
            RenderDescriptorAttribute("Post Scaling", descriptor.getPostScaling(), 0);
        else
            ImGui::InputText("Post Scaling", &text);

        text = std::to_string(descriptor.getRawSampleSize());
        ImGui::InputText("Raw Sample Size", &text);

        text = "";
        if (descriptor.getReferenceDomainInfo().assigned())
            RenderDescriptorAttribute("Reference Domain Info", descriptor.getReferenceDomainInfo(), 0);
        else
            ImGui::InputText("Reference Domain Info", &text);

        if (descriptor.getRule().assigned())
            RenderDescriptorAttribute("Rule", descriptor.getRule(), 0);
        else
            ImGui::InputText("Rule", &text);

        text = std::to_string(descriptor.getSampleSize());
        ImGui::InputText("Sample Size", &text);

        try {
            text = SampleTypeToString(descriptor.getSampleType());
        } catch (...) {
            text = "<unavailable>";
        }
        ImGui::InputText("Sample Type", &text);

        text = "";
        if (descriptor.getStructFields().assigned())
            RenderDescriptorAttribute("Struct Fields", descriptor.getStructFields(), 0);
        else
            ImGui::InputText("Struct Fields", &text);

        if (auto tick_res = descriptor.getTickResolution(); tick_res.assigned())
            text = std::to_string((long long)tick_res.getNumerator()) + "/" + std::to_string((long long)tick_res.getDenominator());
        else
            text = "None";
        ImGui::InputText("Tick Resolution", &text);

        if (auto unit = descriptor.getUnit(); unit.assigned())
        {
            std::string symbol = unit.getSymbol().assigned() ? unit.getSymbol().toStdString() : "None";
            std::string quantity = unit.getQuantity().assigned() ? unit.getQuantity().toStdString() : "None";
            text = symbol + " (" + quantity + ")";
        }
        else
            text = "None";
        ImGui::InputText("Unit", &text);

        if (descriptor.getValueRange().assigned())
            RenderDescriptorAttribute("Value Range", descriptor.getValueRange(), 0);
        else
        {
            text = "None";
            ImGui::InputText("Value Range", &text);
        }

        ImGui::EndDisabled();

        auto metadata = descriptor.getMetadata();
        if (metadata.assigned() && metadata.getCount() > 0)
        {
            if (ImGui::TreeNodeEx("Metadata", ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (const auto& key : metadata.getKeyList())
                {
                    auto value = metadata.get(key);
                    std::string key_str = key.supportsInterface<daq::IString>() ? 
                        key.asPtr<daq::IString>().toStdString() : 
                        static_cast<std::string>(key.toString());
                    RenderDescriptorAttribute(key_str, value, 1);
                }
                ImGui::TreePop();
            }
        }
}

void RenderComponentPropertiesAndAttributes(const daq::ComponentPtr& component, bool show_attributes)
{
    daq::PropertyObjectPtr property_holder = castTo<daq::IPropertyObject>(component);
    for (const auto& property : property_holder.getVisibleProperties())
    {
        RenderProperty(property, property_holder);
    }

    if (canCastTo<daq::ISignal>(component))
    {
        daq::SignalPtr signal = castTo<daq::ISignal>(component);
        
        if (ImGui::BeginTabBar("SignalDescriptors"))
        {
            if (signal.getDescriptor().assigned())
            {
                if (ImGui::BeginTabItem("Signal Descriptor"))
                {
                    RenderAllDescriptorAttributes(signal.getDescriptor(), "Signal Descriptor");
                    ImGui::EndTabItem();
                }
            }
            
            auto domain_signal = signal.getDomainSignal();
            if (domain_signal.assigned() && domain_signal.getDescriptor().assigned())
            {
                if (ImGui::BeginTabItem("Domain Signal Descriptor"))
                {
                    RenderAllDescriptorAttributes(domain_signal.getDescriptor(), "Domain Signal Descriptor");
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }

    if (!show_attributes)
        return;

    {
        std::string value = component.getName();
        if (ImGui::InputText("Name", &value))
            component.setName(value);
    }
    {
        std::string value = component.getDescription();
        if (ImGui::InputText("Description", &value))
            component.setDescription(value);
    }
    {
        bool value = component.getActive();
        if (ImGui::Checkbox("Active", &value))
            component.setActive(value);
    }
    {
        bool value = component.getVisible();
        if (ImGui::Checkbox("Visible", &value))
            component.setVisible(value);
    }
    {
        // tags
        daq::ListPtr<daq::IString> tags = component.getTags().getList();
        std::stringstream tags_value;
        tags_value << "[";
        for (int i = 0; i < tags.getCount(); i++)
        {
            if (i != 0)
                tags_value << ", ";
            tags_value << tags.getItemAt(i).toStdString();
        }
        tags_value << "]";
        ImGui::BeginDisabled();
        std::string tags_str = tags_value.str();
        ImGui::InputText("Tags", &tags_str);
        ImGui::EndDisabled();
    }
    {
        std::string value = "unknown";
        try
        {
            if (component.supportsInterface<daq::IFunctionBlock>())
            {
                if (auto fb_type = component.asPtr<daq::IFunctionBlock>().getFunctionBlockType(); fb_type.assigned())
                    value = fb_type.getId().toStdString();
            }
            else if (component.supportsInterface<daq::IDevice>())
            {
                if (auto device_info = component.asPtr<daq::IDevice>().getInfo(); device_info.assigned())
                {
                    if (auto device_type = device_info.getDeviceType(); device_type.assigned())
                        value = device_type.getId().toStdString();
                }
            }
        }
        catch (...) {}

        ImGui::BeginDisabled();
        ImGui::InputText("Type ID", &value);
        ImGui::EndDisabled();
    }
    {
        std::string value = component.getLocalId();
        ImGui::BeginDisabled();
        ImGui::InputText("Local ID", &value);
        ImGui::EndDisabled();
    }
    {
        std::string value = component.getGlobalId();
        ImGui::BeginDisabled();
        ImGui::InputText("Global ID", &value);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone | ImGuiHoveredFlags_AllowWhenDisabled) && ImGui::BeginTooltip())
        {
            ImGui::Text("%s", value.c_str());
            ImGui::EndTooltip();
        }
    }

    if (canCastTo<daq::ISignal>(component))
    {
        daq::SignalPtr signal = castTo<daq::ISignal>(component);
        
        {
            bool value = signal.getPublic();
            if (ImGui::Checkbox("Public", &value))
                signal.setPublic(value);
        }
        {
            std::string value = signal.getDomainSignal().assigned()
                              ? signal.getDomainSignal().getGlobalId().toStdString()
                              : "";
            ImGui::BeginDisabled();
            ImGui::InputText("Domain Signal ID", &value);
            ImGui::EndDisabled();
        }
        {
            bool value = signal.getStreamed();
            ImGui::BeginDisabled();
            ImGui::Checkbox("Streamed", &value);
            ImGui::EndDisabled();
        }
        {
            std::string last_value_str = "N/A";
            try {
                if (signal.getLastValue().assigned()) {
                    auto last_val = signal.getLastValue();
                    if (last_val.supportsInterface<daq::IString>())
                        last_value_str = last_val.asPtr<daq::IString>().toStdString();
                    else if (last_val.supportsInterface<daq::IInteger>())
                        last_value_str = std::to_string((long long)last_val.asPtr<daq::IInteger>());
                    else if (last_val.supportsInterface<daq::IFloat>())
                        last_value_str = std::to_string((double)last_val.asPtr<daq::IFloat>());
                    else
                        last_value_str = static_cast<std::string>(last_val.toString());
                }
            } catch (...) {
                last_value_str = "<error reading value>";
            }
            ImGui::BeginDisabled();
            ImGui::InputText("Last Value", &last_value_str);
            ImGui::EndDisabled();
        }
        {
            // Status - from Python line 166-169
            std::string status_str = "OK";  // Default assumption
            try
            {
                auto status_container = signal.getStatusContainer();
                if (status_container.assigned())
                {
                    auto statuses = status_container.getStatuses();
                    if (statuses.assigned() && statuses.getCount() > 0)
                        status_str = "Multiple statuses available";
                }
            } catch (...)
            {
                status_str = "<unavailable>";
            }
            ImGui::BeginDisabled();
            ImGui::InputText("Status", &status_str);
            ImGui::EndDisabled();
        }
    }

    if (canCastTo<daq::IInputPort>(component))
    {
        auto input_port = castTo<daq::IInputPort>(component);
        daq::InputPortPtr input_port_ptr(input_port);
        
        {
            std::string value = input_port_ptr.getSignal().assigned()
                ? input_port_ptr.getSignal().getGlobalId().toStdString()
                : "";
            ImGui::BeginDisabled();
            ImGui::InputText("Signal ID", &value);
            ImGui::EndDisabled();
        }
        {
            bool value = input_port_ptr.getRequiresSignal();
            ImGui::BeginDisabled();
            ImGui::Checkbox("Requires Signal", &value);
            ImGui::EndDisabled();
        }
        {
            std::string status_str = "OK";  // Default assumption
            try {
                auto status_container = input_port_ptr.getStatusContainer();
                if (status_container.assigned()) {
                    auto statuses = status_container.getStatuses();
                    if (statuses.assigned() && statuses.getCount() > 0) {
                        status_str = "Multiple statuses available"; // Simplified for now
                    }
                }
            } catch (...) {
                status_str = "<unavailable>";
            }
            ImGui::BeginDisabled();
            ImGui::InputText("Status", &status_str);
            ImGui::EndDisabled();
        }
    }
}

void RenderSelectedComponent(daq::ComponentPtr component, bool show_parents, bool show_attributes)
{
    if (!canCastTo<daq::IPropertyObject>(component))
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextPadding, ImVec2(5.0f, 5.0f));
    while (component.assigned())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::SeparatorText(("[" + component.getName().toStdString() + "]").c_str());
        ImGui::PopStyleColor();
        RenderComponentPropertiesAndAttributes(component, show_attributes);
        if (!show_parents)
            break;
        component = component.getParent();
    }
    ImGui::PopStyleVar(2);
}

void DrawPropertiesWindow(const std::vector<daq::ComponentPtr>& selected_components)
{
    ImGui::Begin("Property editor", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_MenuBar);
    {
        static bool show_parents = false;
        static bool tabbed_interface = false;
        static bool show_attributes = false;
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Settings"))
            {
                ImGui::Checkbox("Show parents", &show_parents);
                ImGui::Checkbox("Show attributes", &show_attributes);
                ImGui::Checkbox("Use tabs for multiple selected components", &tabbed_interface);

                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (selected_components.empty())
        {
            ImGui::Text("No component selected");
        }
        else if (selected_components.size() == 1)
        {
            daq::ComponentPtr component = selected_components[0];
            if (component == nullptr || !component.assigned())
                return;

            RenderSelectedComponent(component, show_parents, show_attributes);
        }
        else if (tabbed_interface)
        {
            if (ImGui::BeginTabBar("Selected components"))
            {
                int uid = 0;
                for (const auto& component : selected_components)
                {
                    if (component == nullptr || !component.assigned())
                        continue;

                    if (ImGui::BeginTabItem((component.getName().toStdString() + "##" + std::to_string(uid++)).c_str()))
                    {
                        RenderSelectedComponent(component, show_parents, show_attributes);
                        ImGui::EndTabItem();
                    }
                }
                ImGui::EndTabBar();
            }
        }
        else // render side by side
        {
            int uid = 0;
            for (const auto& component : selected_components)
            {
                if (component == nullptr || !component.assigned())
                    continue;

                ImGui::BeginChild((component.getName().toStdString() + "##" + std::to_string(uid++)).c_str(), ImVec2(0, 0), ImGuiChildFlags_None | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);
                RenderSelectedComponent(component, show_parents, show_attributes);
                ImGui::EndChild();

                ImGui::SameLine();
            }
        }
    }
    ImGui::End();
}
