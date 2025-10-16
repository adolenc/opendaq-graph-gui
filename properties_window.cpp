#include "properties_window.h"
#include "property_cache.h"
#include "opendaq_control.h"
#include <string>
#include <sstream>
#include "imgui.h"
#include "imgui_stdlib.h"


void PropertiesWindow::RenderCachedProperty(CachedProperty& cached_prop)
{
    if (cached_prop.is_read_only_)
        ImGui::BeginDisabled();

    // ImGui::Indent(cached_prop.depth_ * 10.0f);

    switch (cached_prop.type_)
    {
        case daq::ctBool:
            assert(std::holds_alternative<bool>(cached_prop.value_));
            {
                bool value = std::get<bool>(cached_prop.value_);
                if (ImGui::Checkbox(cached_prop.display_name_.c_str(), &value))
                    cached_prop.SetValue(value);
            }
            break;
        case daq::ctInt:
            assert(std::holds_alternative<int64_t>(cached_prop.value_));
            {
                if (cached_prop.selection_values_count_ > 0)
                {
                    assert(cached_prop.selection_values_);
                    int value = std::get<int64_t>(cached_prop.value_);
                    if (ImGui::Combo(cached_prop.display_name_.c_str(), &value, cached_prop.selection_values_->c_str(), cached_prop.selection_values_count_))
                        cached_prop.SetValue((int64_t)value);
                }
                else
                {
                    int value = std::get<int64_t>(cached_prop.value_);
                    if (ImGui::InputInt(cached_prop.display_name_.c_str(), &value))
                        cached_prop.SetValue((int64_t)value);
                }
            }
            break;
        case daq::ctFloat:
            assert(std::holds_alternative<double>(cached_prop.value_));
            {
                double value = std::get<double>(cached_prop.value_);
                if (ImGui::InputDouble(cached_prop.display_name_.c_str(), &value))
                    cached_prop.SetValue(value);
            }
            break;
        case daq::ctString:
            assert(std::holds_alternative<std::string>(cached_prop.value_));
            {
                std::string value = std::get<std::string>(cached_prop.value_);
                if (ImGui::InputText(cached_prop.display_name_.c_str(), &value))
                    cached_prop.SetValue(value);
                if (cached_prop.name_ == "@GlobalID" && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone | ImGuiHoveredFlags_AllowWhenDisabled) && ImGui::BeginTooltip())
                {
                    ImGui::Text("%s", value.c_str());
                    ImGui::EndTooltip();
                }
            }
            break;
        case daq::ctProc:
            if (ImGui::Button(cached_prop.display_name_.c_str()))
                cached_prop.SetValue({});
            break;
        case daq::ctObject:
            ImGui::Text("!Unsupported: %s", cached_prop.display_name_.c_str());
            break;
        default:
            {
                std::string n = "!Unsupported prop t" + std::to_string((int)cached_prop.type_) + ": " + cached_prop.display_name_;
                ImGui::Text("%s", n.c_str());
                break;
            }
    }

    // ImGui::Unindent();

    if (cached_prop.is_read_only_)
        ImGui::EndDisabled();
}

void PropertiesWindow::RenderProperty(daq::PropertyPtr property, daq::PropertyObjectPtr property_holder)
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
                if (property.getReadOnly())
                    ImGui::EndDisabled();
                ImGui::Indent();
                ImGui::PushID(prop_name.c_str());
                daq::PropertyObjectPtr parent = property_holder.getPropertyValue(prop_name);
                for (const auto& sub_property : parent.getVisibleProperties())
                    RenderProperty(sub_property, parent);
                ImGui::PopID();
                ImGui::Unindent();
                if (property.getReadOnly())
                    ImGui::BeginDisabled();
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

std::string PropertiesWindow::SampleTypeToString(daq::SampleType sample_type)
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

std::string PropertiesWindow::CoreTypeToString(daq::CoreType core_type)
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

std::string PropertiesWindow::OperationModeToString(daq::OperationModeType mode)
{
    switch (mode)
    {
        case daq::OperationModeType::Unknown: return "Unknown";
        case daq::OperationModeType::Idle: return "Idle";
        case daq::OperationModeType::Operation: return "Operation";
        case daq::OperationModeType::SafeOperation: return "Safe Operation";
        default: return "Unknown";
    }
}

void PropertiesWindow::RenderComponentStatus(const daq::ComponentPtr& component)
{
    if (!component.assigned())
        return;

    try
    {
        auto status_container = component.getStatusContainer();
        if (!status_container.assigned())
            return;

        auto statuses = status_container.getStatuses();
        if (!statuses.assigned() || statuses.getCount() == 0)
            return;

        for (const auto& key : statuses.getKeyList())
        {
            std::string status_name = key.asPtr<daq::IString>().toStdString();
            auto status_value = statuses.get(key);

            if (!status_value.supportsInterface<daq::IEnumeration>())
                continue;

            auto enum_value = status_value.asPtr<daq::IEnumeration>();
            std::string display_text = enum_value.getValue().toStdString();
            int int_value = enum_value.getIntValue();
            if (int_value == 0) // ok
                continue;

            ImVec4 color;
            if (int_value >= 2)  // error
                color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            else  // warning
                color = ImVec4(1.0f, 0.7f, 0.2f, 1.0f);

            try
            {
                auto msg_str = status_container.getStatusMessage(key);
                if (msg_str.assigned())
                    display_text += ": " + msg_str.toStdString();
            }
            catch (...) {}

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%s", display_text.c_str());
            ImGui::PopStyleColor();
        }
    }
    catch (...) { }
}

void PropertiesWindow::RenderDescriptorAttribute(const std::string& name, const daq::BaseObjectPtr& value, int depth)
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

void PropertiesWindow::RenderAllDescriptorAttributes(const daq::DataDescriptorPtr& descriptor, const std::string& title)
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

void PropertiesWindow::RenderCachedComponent(CachedComponent& cached_component)
{
    ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextPadding, ImVec2(5.0f, 5.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::SeparatorText(("[" + cached_component.name_ + "]").c_str());
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    if (!cached_component.error_message_.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::TextWrapped("%s", cached_component.error_message_.c_str());
        ImGui::PopStyleColor();
    }
    else if (!cached_component.warning_message_.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.2f, 1.0f));
        ImGui::TextWrapped("%s", cached_component.warning_message_.c_str());
        ImGui::PopStyleColor();
    }

    if (show_attributes_)
    {
        for (auto& cached_attr : cached_component.detail_attributes_)
            RenderCachedProperty(cached_attr);
    }

    for (auto& cached_attr : cached_component.main_attributes_)
        RenderCachedProperty(cached_attr);

    for (auto& cached_prop : cached_component.properties_)
        RenderCachedProperty(cached_prop);
    
    if (cached_component.needs_refresh_)
        cached_component.Refresh();
}

void PropertiesWindow::RenderComponentPropertiesAndAttributes(const daq::ComponentPtr& component)
{
    RenderComponentStatus(component);

    if (canCastTo<daq::IDevice>(component))
    {
        daq::DevicePtr device = castTo<daq::IDevice>(component);
        try
        {
            auto available_modes = device.getAvailableOperationModes();
            if (!available_modes.assigned() || available_modes.getCount() == 0)
                return;

            auto current_mode = device.getOperationMode();
            std::vector<daq::OperationModeType> mode_types;
            int current_index = 0;
            std::stringstream modes_str;
            for (size_t i = 0; i < available_modes.getCount(); i++)
            {
                auto mode_type = static_cast<daq::OperationModeType>((int)available_modes.getItemAt(i));
                mode_types.push_back(mode_type);
                modes_str << OperationModeToString(mode_type) << '\0';
                if (mode_type == current_mode)
                    current_index = i;
            }

            if (ImGui::Combo("OperationMode", &current_index, modes_str.str().c_str(), mode_types.size()))
                device.setOperationMode(mode_types[current_index]);
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
        }
        catch (...)
        {
        }
    }

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

    if (!show_attributes_)
        return;

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

void PropertiesWindow::RenderSelectedComponent(const daq::ComponentPtr& component)
{
    if (!canCastTo<daq::IPropertyObject>(component))
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextPadding, ImVec2(5.0f, 5.0f));
    daq::ComponentPtr parent_component = component;
    while (parent_component.assigned())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::SeparatorText(("[" + parent_component.getName().toStdString() + "]").c_str());
        ImGui::PopStyleColor();
        RenderComponentPropertiesAndAttributes(parent_component);
        if (!show_parents_)
            break;
        parent_component = parent_component.getParent();
    }
    ImGui::PopStyleVar(2);
}

void PropertiesWindow::OnSelectionChanged(const std::vector<daq::ComponentPtr>& selected_components)
{
    if (freeze_selection_)
        return;

    selected_components_ = selected_components;
    
    cached_components_.clear();
    for (const auto& component : selected_components_)
    {
        if (component.assigned())
            cached_components_.push_back(std::make_unique<CachedComponent>(component));
    }
}

void PropertiesWindow::Render()
{
    ImGui::Begin("Property editor", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_MenuBar);
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Settings"))
            {
                ImGui::Checkbox("Freeze selection", &freeze_selection_);
                ImGui::Checkbox("Show parents", &show_parents_);
                ImGui::Checkbox("Show attributes", &show_attributes_);
                ImGui::Checkbox("Use tabs for multiple selected components", &tabbed_interface_);

                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (cached_components_.empty())
        {
            ImGui::Text("No component selected");
        }
        else if (cached_components_.size() == 1)
        {
            RenderCachedComponent(*cached_components_[0]);
        }
        else if (tabbed_interface_)
        {
            if (ImGui::BeginTabBar("Selected components"))
            {
                int uid = 0;
                for (auto& cached_component : cached_components_)
                {
                    if (ImGui::BeginTabItem((cached_component->name_ + "##" + std::to_string(uid++)).c_str()))
                    {
                        RenderCachedComponent(*cached_component);
                        ImGui::EndTabItem();
                    }
                }
                ImGui::EndTabBar();
            }
        }
        else
        {
            int uid = 0;
            for (auto& cached_component : cached_components_)
            {
                ImGui::BeginChild((cached_component->name_ + "##" + std::to_string(uid++)).c_str(), ImVec2(0, 0), ImGuiChildFlags_None | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);

                RenderCachedComponent(*cached_component);

                ImGui::EndChild();
                ImGui::SameLine();
            }
        }
    }
    ImGui::End();
}
