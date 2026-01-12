#include "component_cache.h"
#include "utils.h"
#include "ImGuiNotify.hpp"


CachedComponent::CachedComponent(daq::ComponentPtr component)
    : component_(component)
{
    UpdateState();
}

void CachedComponent::UpdateState()
{
    if (!component_.assigned())
        return;

    name_ = component_.getName().toStdString();
    is_active_ = (bool)component_.getActive();

    is_locked_ = false;
    operation_mode_.clear();

    if (canCastTo<daq::IDevice>(component_))
    {
        daq::DevicePtr device = castTo<daq::IDevice>(component_);
        if (auto available_modes = device.getAvailableOperationModes(); available_modes.assigned() && available_modes.getCount() > 0)
        {
            auto current_mode = device.getOperationMode();
            operation_mode_ = OperationModeToString(current_mode);
        }
        is_locked_ = (bool)device.isLocked();
    }

    RefreshStatus();
}

void CachedComponent::AddProperty(daq::PropertyPtr prop, daq::PropertyObjectPtr property_holder, int depth, const std::string& parent_uid)
{
    properties_.push_back(CachedProperty());
    CachedProperty& cached = properties_.back();
    cached.property_ = prop;
    cached.owner_ = this;
    cached.depth_ = depth;
    cached.name_ = prop.getName().toStdString();
    cached.uid_ = parent_uid + cached.name_;
    cached.unit_ = (prop.getUnit().assigned() && prop.getUnit().getSymbol().assigned()) ? static_cast<std::string>(prop.getUnit().getSymbol()) : "";
    cached.display_name_ = cached.name_ + (cached.unit_.empty() ? "" : " [" + cached.unit_ + "]");
    cached.is_read_only_ = prop.getReadOnly();
    if (prop.getMinValue().assigned()) cached.min_value_ = (double)prop.getMinValue();
    if (prop.getMaxValue().assigned()) cached.max_value_ = (double)prop.getMaxValue();
    cached.type_ = prop.getValueType();

    if (auto sv = prop.getSelectionValues(); sv.assigned())
    {
        std::stringstream values;
        daq::ListPtr<daq::IBaseObject> selection_values;
        if (sv.supportsInterface<daq::IList>())
            selection_values = sv;
        else if (sv.supportsInterface<daq::IDict>())
            selection_values = daq::DictPtr<daq::IInteger, daq::IString>(sv).getValueList();
        
        if (selection_values.assigned())
        {
            for (size_t i = 0; i < selection_values.getCount(); i++)
            {
                auto val = selection_values.getItemAt(i);
                if (val.supportsInterface<daq::IInteger>())
                    values << std::to_string((int64_t)val) << '\0';
                else if (val.supportsInterface<daq::IFloat>())
                    values << std::to_string((double)val) << '\0';
                else
                    values << static_cast<std::string>(val) << '\0';
            }
            cached.selection_values_ = values.str();
            cached.selection_values_count_ = (int)selection_values.getCount();
        }
    }

    try
    {
        switch (cached.type_)
        {
            case daq::ctBool:
                cached.value_ = (bool)property_holder.getPropertyValue(cached.name_);
                break;
            case daq::ctInt:
                cached.value_ = (int64_t)property_holder.getPropertyValue(cached.name_);
                break;
            case daq::ctFloat:
                cached.value_ = (double)property_holder.getPropertyValue(cached.name_);
                break;
            case daq::ctString:
                cached.value_ = static_cast<std::string>(property_holder.getPropertyValue(cached.name_));
                break;
            case daq::ctObject:
                {
                    std::string new_parent_uid = cached.uid_ + ".";
                    daq::PropertyObjectPtr parent = property_holder.getPropertyValue(cached.name_);
                    for (const auto& sub_property : parent.getVisibleProperties())
                        AddProperty(sub_property, parent, depth + 1, new_parent_uid);
                }
                break;
            case daq::ctList:
            case daq::ctDict:
            case daq::ctRatio:
            case daq::ctEnumeration:
                cached.is_read_only_ = true;
                cached.type_ = daq::ctString;
                cached.value_ = ValueToString(property_holder.getPropertyValue(cached.name_));
                break;
            case daq::ctStruct:
                {
                    auto struct_value = property_holder.getPropertyValue(cached.name_).asPtr<daq::IStruct>();
                    auto field_names = struct_value.getFieldNames();
                    auto field_values = struct_value.getFieldValues();
                    for (size_t i = 0; i < field_names.getCount(); i++)
                    {
                        CachedProperty struct_field;
                        struct_field.owner_ = this;
                        struct_field.depth_ = depth + 1;
                        struct_field.name_ = field_names.getItemAt(i).toStdString();
                        struct_field.uid_ = cached.uid_ + "." + struct_field.name_;
                        struct_field.display_name_ = struct_field.name_;
                        struct_field.is_read_only_ = true;
                        struct_field.property_ = prop;
                        
                        auto field_value = field_values.getItemAt(i);
                        if (field_value.supportsInterface<daq::IBoolean>())
                        {
                            struct_field.type_ = daq::ctBool;
                            struct_field.value_ = (bool)field_value.asPtr<daq::IBoolean>();
                        }
                        else if (field_value.supportsInterface<daq::IInteger>())
                        {
                            struct_field.type_ = daq::ctInt;
                            struct_field.value_ = (int64_t)field_value.asPtr<daq::IInteger>();
                        }
                        else if (field_value.supportsInterface<daq::IFloat>())
                        {
                            struct_field.type_ = daq::ctFloat;
                            struct_field.value_ = (double)field_value.asPtr<daq::IFloat>();
                        }
                        else
                        {
                            struct_field.type_ = daq::ctString;
                            struct_field.value_ = ValueToString(field_value);
                        }
                        properties_.push_back(struct_field);
                    }
                }
                break;
            default:
                break;
        }
    }
    catch (...) {}
}

CachedProperty& CachedComponent::AddAttribute(std::vector<CachedProperty>& properties, const std::string& name, const std::string& display_name, CachedProperty::ValueType value, bool is_read_only, bool is_debug_property, daq::CoreType type)
{
    properties.push_back(CachedProperty());
    CachedProperty& cached = properties.back();
    cached.owner_ = this;
    cached.name_ = name;
    cached.uid_ = name;
    cached.display_name_ = display_name;
    cached.value_ = value;
    cached.is_read_only_ = is_read_only;
    cached.is_debug_property_ = is_debug_property;
    cached.type_ = type;
    return cached;
}

void CachedComponent::AddDescriptorProperties(daq::DataDescriptorPtr descriptor, std::vector<CachedProperty>& properties, bool is_domain_signal)
{
    std::string prefix = is_domain_signal ? "@DSD_" : "@SD_";
    {
        std::string value;
        try { value = SampleTypeToString(descriptor.getSampleType()); } catch (...) { value = "<unavailable>"; }
        AddAttribute(properties, prefix + "SampleType", "Sample Type", value, true, true);
    }
    {
        std::string value = descriptor.getName().assigned() ? descriptor.getName().toStdString() : "None";
        AddAttribute(properties, prefix + "Name", "Name", value, true, false);
    }
    {
        std::string value;
        if (descriptor.getDimensions().assigned())
        {
            auto dimensions = descriptor.getDimensions();
            try
            {
                std::string text = "{";
                for (size_t i = 0; i < dimensions.getCount(); i++)
                {
                    if (i > 0) text += ", ";

                    daq::DimensionPtr dim = dimensions.getItemAt(i);
                    text += (dim.getName().assigned()) ? dim.getName().toStdString() : std::to_string(i);
                    text += (dim.getUnit().assigned() && dim.getUnit().getSymbol().assigned()) ? " [" + dim.getUnit().getSymbol().toStdString() + "]: " : ": ";
                    text += std::to_string((long long)dim.getSize());
                }
                text += "}";
                value = text;
            } catch (...) { value = "<error>"; }
        }
        else value = "None";
        AddAttribute(properties, prefix + "Dimensions", "Dimensions", value, true, is_domain_signal);
    }
    {
        std::string value = descriptor.getOrigin().assigned() ? descriptor.getOrigin().toStdString() : "None";
        AddAttribute(properties, prefix + "Origin", "Origin", value, true, true);
    }
    AddAttribute(properties, prefix + "RawSampleSize", "Raw Sample Size", std::to_string(descriptor.getRawSampleSize()), true, true);
    AddAttribute(properties, prefix + "SampleSize", "Sample Size", std::to_string(descriptor.getSampleSize()), true, true);
    {
        std::string value;
        if (auto tick_res = descriptor.getTickResolution(); tick_res.assigned())
            value = std::to_string((long long)tick_res.getNumerator()) + "/" + std::to_string((long long)tick_res.getDenominator());
        else
            value = "None";
        AddAttribute(properties, prefix + "TickResolution", "Tick Resolution", value, true, !is_domain_signal);
    }
    {
        std::string value;
        if (auto unit = descriptor.getUnit(); unit.assigned())
        {
            std::string symbol = unit.getSymbol().assigned() ? unit.getSymbol().toStdString() : "None";
            std::string quantity = unit.getQuantity().assigned() ? unit.getQuantity().toStdString() : "None";
            value = symbol + " (" + quantity + ")";
        }
        else
            value = "None";
        AddAttribute(properties, prefix + "Unit", "Unit", value, true, false);
    }
    {
        std::string value;
        try {
            if (auto rule = descriptor.getRule(); rule.assigned()) value = ValueToString(rule);
            else value = "None";
        } catch (...) { value = "<unavailable>"; }
        AddAttribute(properties, prefix + "Rule", "Rule", value, true, !is_domain_signal);
    }
    {
        std::string value;
        try
        {
            if (daq::RangePtr range = descriptor.getValueRange(); range.assigned())
            {
                daq::NumberPtr low = range.getLowValue();
                daq::NumberPtr high = range.getHighValue();
                std::string low_str = low.assigned() ? std::to_string((double)low) : "None";
                std::string high_str = high.assigned() ? std::to_string((double)high) : "None";
                value = "[" + low_str + ", " + high_str + "]";
            }
            else value = "None";
        } catch (...) { value = "<unavailable>"; }
        AddAttribute(properties, prefix + "ValueRange", "Value Range", value, true, is_domain_signal);
    }
    {
        std::string value;
        try {
            if (auto scaling = descriptor.getPostScaling(); scaling.assigned())
                value = static_cast<std::string>(scaling.asPtr<daq::IBaseObject>().toString());
            else value = "None";
        } catch (...) { value = "<unavailable>"; }
        AddAttribute(properties, prefix + "PostScaling", "Post Scaling", value, true, true);
    }
    {
        std::string value;
        try {
            if (auto fields = descriptor.getStructFields(); fields.assigned())
            {
                std::string text = "[";
                for (size_t i = 0; i < fields.getCount(); i++)
                {
                    if (i > 0) text += ", ";
                    text += static_cast<std::string>(fields.getItemAt(i).asPtr<daq::IBaseObject>().toString());
                }
                text += "]";
                value = text;
            }
            else value = "None";
        } catch (...) { value = "<unavailable>"; }
        AddAttribute(properties, prefix + "StructFields", "Struct Fields", value, true, true);
    }
    {
        std::string value;
        try {
            if (auto metadata = descriptor.getMetadata(); metadata.assigned())
                value = DictToString(metadata);
            else value = "None";
        } catch (...) { value = "<unavailable>"; }
        AddAttribute(properties, prefix + "Metadata", "Metadata", value, true, true);
    }
}

void CachedComponent::RefreshStructure()
{
    input_ports_.clear();
    output_signals_.clear();

    if (!component_.assigned())
        return;

    if (canCastTo<daq::IFunctionBlock>(component_))
    {
        daq::FunctionBlockPtr function_block = castTo<daq::IFunctionBlock>(component_);
        for (const daq::InputPortPtr& input_port : function_block.getInputPorts())
            input_ports_.push_back({input_port.getName().toStdString(), input_port.getGlobalId().toStdString()});

        for (const daq::SignalPtr& signal : function_block.getSignals())
            output_signals_.push_back({signal.getName().toStdString(), signal.getGlobalId().toStdString()});
    }
    else if (canCastTo<daq::IDevice>(component_))
    {
        daq::DevicePtr device = castTo<daq::IDevice>(component_);
        for (const daq::SignalPtr& signal : device.getSignals())
            output_signals_.push_back({signal.getName().toStdString(), signal.getGlobalId().toStdString()});
    }
}

void CachedComponent::RefreshStatus()
{
    error_message_ = "";
    warning_message_ = "";
    if (auto status_container = component_.getStatusContainer(); status_container.assigned())
    {
        if (!status_container.getStatuses().hasKey("ComponentStatus"))
            return;

        std::string severity = status_container.getStatuses().get("ComponentStatus").getValue().toStdString();
        if (severity == "Ok")
            return;

        std::string display_text = severity;
        if (auto status_message = status_container.getStatusMessage("ComponentStatus"); status_message.assigned())
            display_text += ": " + status_message.toStdString();

        if (severity == "Error") 
            error_message_ = display_text;
        else // Warning
            warning_message_ = display_text;
    }
}

void CachedComponent::RefreshProperties()
{
    assert(component_.assigned());

    properties_.clear();
    attributes_.clear();
    signal_descriptor_properties_.clear();
    signal_domain_descriptor_properties_.clear();
    initial_properties_loaded_ = true;

    UpdateState();

    if (canCastTo<daq::IDevice>(component_))
    {
        daq::DevicePtr device = castTo<daq::IDevice>(component_);
        if (auto available_modes = device.getAvailableOperationModes(); available_modes.assigned() && available_modes.getCount() > 0)
        {
            auto& cached = AddAttribute(attributes_, "@OperationMode", "Operation Mode", (int64_t)0, false, false, daq::ctInt);
            auto current_mode = device.getOperationMode();
            std::stringstream modes_str;
            for (size_t i = 0; i < available_modes.getCount(); i++)
            {
                auto mode_type = static_cast<daq::OperationModeType>((int)available_modes.getItemAt(i));
                modes_str << OperationModeToString(mode_type) << '\0';
                if (mode_type == current_mode)
                    cached.value_ = (int64_t)i;
            }
            cached.selection_values_ = modes_str.str();
            cached.selection_values_count_ = (int)available_modes.getCount();
        }

        AddAttribute(attributes_, "@Locked", "Locked", is_locked_, false, false, daq::ctBool);
    }

    AddAttribute(attributes_, "@Name", "Name", component_.getName().toStdString(), false, false);
    AddAttribute(attributes_, "@Description", "Description", component_.getDescription().toStdString(), false, true);
    AddAttribute(attributes_, "@Active", "Active", (bool)component_.getActive(), false, true, daq::ctBool);
    AddAttribute(attributes_, "@Visible", "Visible", (bool)component_.getVisible(), false, true, daq::ctBool);
    AddAttribute(attributes_, "@LocalID", "Local ID", component_.getLocalId().toStdString(), true, true);
    AddAttribute(attributes_, "@GlobalID", "Global ID", component_.getGlobalId().toStdString(), true, true);

    if (canCastTo<daq::IRecorder>(component_))
    {
        daq::RecorderPtr recorder = castTo<daq::IRecorder>(component_);
        AddAttribute(attributes_, "@Recording", "Recording", (bool)recorder.getIsRecording(), false, false, daq::ctBool);
    }

    if (signal_color_.has_value())
        AddAttribute(attributes_, "@SignalColor", "Signal Color", (int64_t)ImGui::ColorConvertFloat4ToU32(signal_color_.value()), false, false, daq::ctInt);

    {
        daq::ListPtr<daq::IString> tags = component_.getTags().getList();
        std::stringstream tags_value;
        tags_value << "[";
        for (size_t i = 0; i < tags.getCount(); i++)
        {
            if (i != 0)
                tags_value << ", ";
            tags_value << tags.getItemAt(i).toStdString();
        }
        tags_value << "]";
        AddAttribute(attributes_, "@Tags", "Tags", tags_value.str(), true, true);
    }
    {
        std::string value = "unknown";
        try
        {
            if (component_.supportsInterface<daq::IFunctionBlock>())
            {
                if (auto fb_type = component_.asPtr<daq::IFunctionBlock>().getFunctionBlockType(); fb_type.assigned())
                    value = fb_type.getId().toStdString();
            }
            else if (component_.supportsInterface<daq::IDevice>())
            {
                if (auto device_info = component_.asPtr<daq::IDevice>().getInfo(); device_info.assigned())
                {
                    if (auto device_type = device_info.getDeviceType(); device_type.assigned())
                        value = device_type.getId().toStdString();
                }
            }
        }
        catch (...) {}
        AddAttribute(attributes_, "@TypeID", "Type ID", value, true, true);
    }

    if (canCastTo<daq::ISignal>(component_))
    {
        daq::SignalPtr signal = castTo<daq::ISignal>(component_);
        AddAttribute(attributes_, "@Public", "Public", (bool)signal.getPublic(), false, true, daq::ctBool);
        std::string domain_signal_id = signal.getDomainSignal().assigned()
                          ? signal.getDomainSignal().getGlobalId().toStdString()
                          : std::string("");
        AddAttribute(attributes_, "@DomainSignalID", "Domain Signal ID", domain_signal_id, true, true);
        AddAttribute(attributes_, "@Streamed", "Streamed", (bool)signal.getStreamed(), true, true, daq::ctBool);

        {
            std::string value = "N/A";
            try
            {
                if (signal.getLastValue().assigned())
                {
                    auto last_val = signal.getLastValue();
                    if (last_val.supportsInterface<daq::IString>())
                        value = last_val.asPtr<daq::IString>().toStdString();
                    else if (last_val.supportsInterface<daq::IInteger>())
                        value = std::to_string((long long)last_val.asPtr<daq::IInteger>());
                    else if (last_val.supportsInterface<daq::IFloat>())
                        value = std::to_string((double)last_val.asPtr<daq::IFloat>());
                }
            } catch (...) { }
            AddAttribute(attributes_, "@LastValue", "Last Value", value, true, true);
        }
        {
            std::string value = "OK";
            try
            {
                auto status_container = signal.getStatusContainer();
                if (status_container.assigned())
                {
                    auto statuses = status_container.getStatuses();
                    if (statuses.assigned() && statuses.getCount() > 0)
                        value = "Multiple statuses available";
                }
            } catch (...) { value = "<unavailable>"; }
            AddAttribute(attributes_, "@Status", "Status", value, true, true);
        }

        if (auto descriptor = signal.getDescriptor(); descriptor.assigned())
            AddDescriptorProperties(descriptor, signal_descriptor_properties_);

        if (auto domain_signal = signal.getDomainSignal(); domain_signal.assigned())
        {
            if (auto descriptor = domain_signal.getDescriptor(); descriptor.assigned())
                AddDescriptorProperties(descriptor, signal_domain_descriptor_properties_, true);
        }
    }

    if (canCastTo<daq::IInputPort>(component_))
    {
        daq::InputPortPtr input_port = castTo<daq::IInputPort>(component_);

        std::string signal_id = input_port.getSignal().assigned()
                      ? input_port.getSignal().getGlobalId().toStdString()
                      : std::string("");
        AddAttribute(attributes_, "@SignalID", "Signal ID", signal_id, true, true);
        AddAttribute(attributes_, "@RequiresSignal", "Requires Signal", (bool)input_port.getRequiresSignal(), true, true, daq::ctBool);
        {
            std::string value = "OK";
            try
            {
                auto status_container = input_port.getStatusContainer();
                if (status_container.assigned())
                {
                    auto statuses = status_container.getStatuses();
                    if (statuses.assigned() && statuses.getCount() > 0)
                        value = "Multiple statuses available";
                }
            }
            catch (...) { value = "<unavailable>"; }
            AddAttribute(attributes_, "@Status", "Status", value, true, true);
        }
    }

    daq::PropertyObjectPtr property_holder = castTo<daq::IPropertyObject>(component_);
    for (const auto& prop : property_holder.getVisibleProperties())
        AddProperty(prop, property_holder);
}

void CachedProperty::SetValue(ValueType value)
{
    daq::ComponentPtr component = owner_->component_;
    if (!component.assigned())
        return;

    try
    {
        // attribute changes have special non-generic logic
        if (name_.size() > 1 && name_[0] == '@')
        {
            if (name_ == "@OperationMode")
            {
                assert(canCastTo<daq::IDevice>(component));
                daq::DevicePtr device = castTo<daq::IDevice>(component);
                device.setOperationMode(device.getAvailableOperationModes().getItemAt(std::get<int64_t>(value)));
            }
            else if (name_ == "@Recording")
            {
                assert(canCastTo<daq::IRecorder>(component));
                daq::RecorderPtr recorder = castTo<daq::IRecorder>(component);
                if (std::get<bool>(value))
                    recorder.startRecording();
                else
                    recorder.stopRecording();
            }
            else if (name_ == "@Locked")
            {
                assert(canCastTo<daq::IDevice>(component));
                daq::DevicePtr device = castTo<daq::IDevice>(component);
                if (std::get<bool>(value))
                    device.lock();
                else
                    device.unlock();
            }
            else if (name_ == "@Name")
            {
                component.setName(std::get<std::string>(value));
            }
            else if (name_ == "@Description")
            {
                component.setDescription(std::get<std::string>(value));
            }
            else if (name_ == "@Active")
            {
                component.setActive(std::get<bool>(value));
            }
            else if (name_ == "@Visible")
            {
                component.setVisible(std::get<bool>(value));
            }
            else if (name_ == "@Public")
            {
                assert(canCastTo<daq::ISignal>(component));
                daq::SignalPtr signal = castTo<daq::ISignal>(component);
                signal.setPublic(std::get<bool>(value));
            }
            else if (name_ == "@SignalColor")
            {
                owner_->signal_color_ = ImGui::ColorConvertU32ToFloat4((ImU32)std::get<int64_t>(value));
            }
            owner_->needs_refresh_ = true;
            return;
        }

        assert(canCastTo<daq::IPropertyObject>(component));
        daq::PropertyObjectPtr property_holder = castTo<daq::IPropertyObject>(component);
        switch (type_)
        {
            case daq::ctBool:
                property_holder.setPropertyValue(uid_, std::get<bool>(value)); break;
            case daq::ctInt:
                property_holder.setPropertyValue(uid_, std::get<int64_t>(value)); break;
            case daq::ctFloat:
                property_holder.setPropertyValue(uid_, std::get<double>(value)); break;
            case daq::ctString:
                property_holder.setPropertyValue(uid_, std::get<std::string>(value)); break;
            case daq::ctProc:
                property_holder.getPropertyValue(uid_).asPtr<daq::IProcedure>().dispatch(); break;
            case daq::ctFunc:
                property_holder.getPropertyValue(uid_).asPtr<daq::IFunction>().dispatch(); break;
            default:
                assert(false && "unsupported property type");
                return;
        }

        owner_->needs_refresh_ = true;
    }
    catch (const std::exception& e)
    {
        ImGui::InsertNotification({ImGuiToastType::Error, DEFAULT_NOTIFICATION_DURATION_MS, "Failed to set property value: %s", e.what()});
    }
    catch (...)
    {
        ImGui::InsertNotification({ImGuiToastType::Error, DEFAULT_NOTIFICATION_DURATION_MS, "Failed to set property value: Unknown error"});
    }
}

void CachedProperty::EnsureFunctionInfoCached()
{
    if (function_info_.has_value())
        return;

    if (!property_.assigned())
        return;

    function_info_ = FunctionInfo();
    auto callable_info = property_.getCallableInfo();
    if (!callable_info.assigned())
        return;

    function_info_->return_type = callable_info.getReturnType();
    auto args = callable_info.getArguments();
    if (!args.assigned())
        return;
    for (const daq::ArgumentInfoPtr& arg : args)
    {
        ValueType default_val;
        switch (arg.getType())
        {
            case daq::ctBool:   default_val = false; break;
            case daq::ctInt:    default_val = (int64_t)0; break;
            case daq::ctFloat:  default_val = 0.0; break;
            case daq::ctString: default_val = std::string(""); break;
            default:            default_val = std::string(""); break;
        }
        function_info_->parameters.push_back({arg.getName(), arg.getType(), default_val});
    }
}
