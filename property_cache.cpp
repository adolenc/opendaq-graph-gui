#include "property_cache.h"
#include "opendaq_control.h"


CachedComponent::CachedComponent(daq::ComponentPtr component)
    : component_(component)
{
    Refresh();
}

static std::string OperationModeToString(daq::OperationModeType mode)
{
    switch (mode)
    {
        case daq::OperationModeType::Idle: return "Idle";
        case daq::OperationModeType::Operation: return "Operation";
        case daq::OperationModeType::SafeOperation: return "Safe Operation";
        default: return "Unknown";
    }
}

static std::string SampleTypeToString(daq::SampleType sample_type)
{
    switch (sample_type)
    {
        case daq::SampleType::Undefined: return "Undefined";
        case daq::SampleType::Float32: return "Float32";
        case daq::SampleType::Float64: return "Float64";
        case daq::SampleType::UInt8: return "UInt8";
        case daq::SampleType::Int8: return "Int8";
        case daq::SampleType::UInt16: return "UInt16";
        case daq::SampleType::Int16: return "Int16";
        case daq::SampleType::UInt32: return "UInt32";
        case daq::SampleType::Int32: return "Int32";
        case daq::SampleType::UInt64: return "UInt64";
        case daq::SampleType::Int64: return "Int64";
        case daq::SampleType::RangeInt64: return "RangeInt64";
        case daq::SampleType::ComplexFloat32: return "ComplexFloat32";
        case daq::SampleType::ComplexFloat64: return "ComplexFloat64";
        case daq::SampleType::Binary: return "Binary";
        case daq::SampleType::String: return "String";
        case daq::SampleType::Struct: return "Struct";
        default: return "Unknown (" + std::to_string(static_cast<int>(sample_type)) + ")";
    }
}

static std::string DataRuleTypeToString(daq::DataRuleType type)
{
    switch (type)
    {
        case daq::DataRuleType::Other: return "Other";
        case daq::DataRuleType::Linear: return "Linear";
        case daq::DataRuleType::Constant: return "Constant";
        case daq::DataRuleType::Explicit: return "Explicit";
        default: return "Unknown";
    }
}

static std::string ValueToString(daq::BaseObjectPtr value)
{
    try
    {
        if (!value.assigned())
            return "None";
        
        if (value.supportsInterface<daq::IDataRule>())
        {
            auto rule = value.asPtr<daq::IDataRule>();
            std::string result = "{type: " + DataRuleTypeToString(rule.getType()) + ", parameters: ";
            if (auto params = rule.getParameters(); params.assigned())
            {
                result += "{";
                auto keys = params.getKeyList();
                for (int i = 0; i < keys.getCount(); i++)
                {
                    if (i > 0) result += ", ";
                    auto key = keys.getItemAt(i);
                    auto val = params.get(key);
                    result += static_cast<std::string>(key) + ": " + ValueToString(val);
                }
                result += "}";
            }
            else
                result += "None";
            result += "}";
            return result;
        }
        else if (value.supportsInterface<daq::IDict>())
        {
            auto dict = value.asPtr<daq::IDict>();
            std::string result = "{";
            auto keys = dict.getKeyList();
            for (int i = 0; i < keys.getCount(); i++)
            {
                if (i > 0) result += ", ";
                auto key = keys.getItemAt(i);
                auto val = dict.get(key);
                result += static_cast<std::string>(key) + ": " + ValueToString(val);
            }
            result += "}";
            return result;
        }
        else if (value.supportsInterface<daq::IPropertyObject>())
        {
            auto prop_obj = value.asPtr<daq::IPropertyObject>();
            std::string result = "{";
            auto props = prop_obj.getVisibleProperties();
            for (int i = 0; i < props.getCount(); i++)
            {
                if (i > 0) result += ", ";
                auto prop = props.getItemAt(i);
                result += prop.getName().toStdString() + ": " + ValueToString(prop_obj.getPropertyValue(prop.getName()));
            }
            result += "}";
            return result;
        }
        else
        {
            return static_cast<std::string>(value.asPtr<daq::IBaseObject>().toString());
        }
    }
    catch (...)
    {
        return "<error>";
    }
}

static std::string DictToString(daq::DictPtr<daq::IString, daq::IBaseObject> dict)
{
    try
    {
        std::string result = "{";
        auto keys = dict.getKeyList();
        for (int i = 0; i < keys.getCount(); i++)
        {
            if (i > 0) result += ", ";
            auto key = keys.getItemAt(i);
            auto value = dict.get(key);
            result += static_cast<std::string>(key) + ": " + ValueToString(value);
        }
        result += "}";
        return result;
    }
    catch (...)
    {
        return "<error>";
    }
}

void CachedComponent::AddProperty(daq::PropertyPtr prop, daq::PropertyObjectPtr property_holder, int depth, const std::string& parent_uid)
{
    // add it first so that recursion works out nicely
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
                    daq::PropertyObjectPtr parent = property_holder.getPropertyValue(cached.name_);
                    for (const auto& sub_property : parent.getVisibleProperties())
                        AddProperty(sub_property, parent, depth + 1, cached.uid_ + ".");
                }
                break;
            case daq::ctStruct:
                {
                    auto struct_value = property_holder.getPropertyValue(cached.name_).asPtr<daq::IStruct>();
                    auto field_names = struct_value.getFieldNames();
                    auto field_values = struct_value.getFieldValues();
                    for (int i = 0; i < field_names.getCount(); i++)
                    {
                        CachedProperty struct_field;
                        struct_field.owner_ = this;
                        struct_field.depth_ = depth + 1;
                        struct_field.name_ = field_names.getItemAt(i).toStdString();
                        struct_field.uid_ = cached.uid_ + struct_field.name_;
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
            for (int i = 0; i < selection_values.getCount(); i++)
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
            cached.selection_values_count_ = selection_values.getCount();
        }
    }
}

void CachedComponent::Refresh()
{
    needs_refresh_ = false;

    assert(component_.assigned());

    properties_.clear();
    attributes_.clear();
    signal_descriptor_properties_.clear();
    signal_domain_descriptor_properties_.clear();

    name_ = component_.getName().toStdString();
    error_message_ = "";
    warning_message_ = "";
    if (auto status_container = component_.getStatusContainer(); status_container.assigned())
    {
        if (auto statuses = status_container.getStatuses(); statuses.assigned())
        {
            for (const auto& key : statuses.getKeyList())
            {
                auto status_value = statuses.get(key);
                if (!status_value.supportsInterface<daq::IEnumeration>())
                    continue;

                auto enum_value = status_value.asPtr<daq::IEnumeration>();
                int severity = enum_value.getIntValue();
                if (severity == 0) // ok
                    continue;

                std::string display_text = enum_value.getValue().toStdString();
                if (auto msg_str = status_container.getStatusMessage(key); msg_str.assigned())
                    display_text += ": " + msg_str.toStdString();

                if (severity >= 2) 
                    error_message_ = display_text;
                else
                    warning_message_ = display_text;
            }
        }
    }

    if (canCastTo<daq::IDevice>(component_))
    {
        daq::DevicePtr device = castTo<daq::IDevice>(component_);
        if (auto available_modes = device.getAvailableOperationModes(); available_modes.assigned() && available_modes.getCount() > 0)
        {
            CachedProperty cached;
            cached.owner_ = this;
            cached.name_ = "@OperationMode";
            cached.uid_ = "@OperationMode";
            cached.display_name_ = "Operation Mode";
            cached.is_read_only_ = false;
            cached.type_ = daq::ctInt;

            auto current_mode = device.getOperationMode();
            int current_index = 0;
            std::stringstream modes_str;
            for (size_t i = 0; i < available_modes.getCount(); i++)
            {
                auto mode_type = static_cast<daq::OperationModeType>((int)available_modes.getItemAt(i));
                modes_str << OperationModeToString(mode_type) << '\0';
                if (mode_type == current_mode)
                    cached.value_ = (int64_t)i;
            }
            cached.selection_values_ = modes_str.str();
            cached.selection_values_count_ = available_modes.getCount();
            cached.is_detail_ = false;
            attributes_.push_back(cached);
        }
    }

    {
        CachedProperty cached;
        cached.owner_ = this;
        cached.name_ = "@Name";
        cached.uid_ = "@Name";
        cached.display_name_ = "Name";
        cached.is_read_only_ = false;
        cached.type_ = daq::ctString;
        cached.value_ = component_.getName().toStdString();
        cached.is_detail_ = false;
        attributes_.push_back(cached);
    }
    {
        CachedProperty cached;
        cached.owner_ = this;
        cached.name_ = "@Description";
        cached.uid_ = "@Description";
        cached.display_name_ = "Description";
        cached.is_read_only_ = false;
        cached.type_ = daq::ctString;
        cached.value_ = component_.getDescription().toStdString();
        cached.is_detail_ = false;
        attributes_.push_back(cached);
    }
    {
        CachedProperty cached;
        cached.owner_ = this;
        cached.name_ = "@Active";
        cached.uid_ = "@Active";
        cached.display_name_ = "Active";
        cached.is_read_only_ = false;
        cached.type_ = daq::ctBool;
        cached.value_ = (bool)component_.getActive();
        cached.is_detail_ = true;
        attributes_.push_back(cached);
    }
    {
        CachedProperty cached;
        cached.owner_ = this;
        cached.name_ = "@Visible";
        cached.uid_ = "@Visible";
        cached.display_name_ = "Visible";
        cached.is_read_only_ = false;
        cached.type_ = daq::ctBool;
        cached.value_ = (bool)component_.getVisible();
        cached.is_detail_ = true;
        attributes_.push_back(cached);
    }
    {
        CachedProperty cached;
        cached.owner_ = this;
        cached.name_ = "@LocalID";
        cached.uid_ = "@LocalID";
        cached.display_name_ = "Local ID";
        cached.is_read_only_ = true;
        cached.type_ = daq::ctString;
        cached.value_ = component_.getLocalId().toStdString();
        cached.is_detail_ = true;
        attributes_.push_back(cached);
    }
    {
        CachedProperty cached;
        cached.owner_ = this;
        cached.name_ = "@GlobalID";
        cached.uid_ = "@GlobalID";
        cached.display_name_ = "Global ID";
        cached.is_read_only_ = true;
        cached.type_ = daq::ctString;
        cached.value_ = component_.getGlobalId().toStdString();
        cached.is_detail_ = true;
        attributes_.push_back(cached);
    }
    {
        CachedProperty cached;
        cached.owner_ = this;
        cached.name_ = "@Tags";
        cached.uid_ = "@Tags";
        cached.display_name_ = "Tags";
        cached.is_read_only_ = true;
        cached.type_ = daq::ctString;
        daq::ListPtr<daq::IString> tags = component_.getTags().getList();
        std::stringstream tags_value;
        tags_value << "[";
        for (int i = 0; i < tags.getCount(); i++)
        {
            if (i != 0)
                tags_value << ", ";
            tags_value << tags.getItemAt(i).toStdString();
        }
        tags_value << "]";
        cached.value_ = tags_value.str();
        cached.is_detail_ = true;
        attributes_.push_back(cached);
    }
    {
        CachedProperty cached;
        cached.owner_ = this;
        cached.name_ = "@TypeID";
        cached.uid_ = "@TypeID";
        cached.display_name_ = "Type ID";
        cached.is_read_only_ = true;
        cached.type_ = daq::ctString;
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
        cached.value_ = value;
        cached.is_detail_ = true;
        attributes_.push_back(cached);
    }

    if (canCastTo<daq::ISignal>(component_))
    {
        daq::SignalPtr signal = castTo<daq::ISignal>(component_);
        
        {
            CachedProperty cached;
            cached.owner_ = this;
            cached.name_ = "@Public";
            cached.uid_ = "@Public";
            cached.display_name_ = "Public";
            cached.is_read_only_ = false;
            cached.type_ = daq::ctBool;
            cached.value_ = (bool)signal.getPublic();
            cached.is_detail_ = true;
            attributes_.push_back(cached);
        }
        {
            CachedProperty cached;
            cached.owner_ = this;
            cached.name_ = "@DomainSignalID";
            cached.uid_ = "@DomainSignalID";
            cached.display_name_ = "Domain Signal ID";
            cached.is_read_only_ = true;
            cached.type_ = daq::ctString;
            cached.value_ = signal.getDomainSignal().assigned()
                          ? signal.getDomainSignal().getGlobalId().toStdString()
                          : std::string("");
            cached.is_detail_ = true;
            attributes_.push_back(cached);
        }
        {
            CachedProperty cached;
            cached.owner_ = this;
            cached.name_ = "@Streamed";
            cached.uid_ = "@Streamed";
            cached.display_name_ = "Streamed";
            cached.is_read_only_ = true;
            cached.type_ = daq::ctBool;
            cached.value_ = (bool)signal.getStreamed();
            cached.is_detail_ = true;
            attributes_.push_back(cached);
        }
        {
            CachedProperty cached;
            cached.owner_ = this;
            cached.name_ = "@LastValue";
            cached.uid_ = "@LastValue";
            cached.display_name_ = "Last Value";
            cached.is_read_only_ = true;
            cached.type_ = daq::ctString;
            try
            {
                if (signal.getLastValue().assigned())
                {
                    auto last_val = signal.getLastValue();
                    if (last_val.supportsInterface<daq::IString>())
                        cached.value_ = last_val.asPtr<daq::IString>().toStdString();
                    else if (last_val.supportsInterface<daq::IInteger>())
                        cached.value_ = std::to_string((long long)last_val.asPtr<daq::IInteger>());
                    else if (last_val.supportsInterface<daq::IFloat>())
                        cached.value_ = std::to_string((double)last_val.asPtr<daq::IFloat>());
                    else
                        cached.value_ = "N/A";
                }
            } catch (...) { }
            attributes_.push_back(cached);
        }
        {
            CachedProperty cached;
            cached.owner_ = this;
            cached.name_ = "@Status";
            cached.uid_ = "@Status";
            cached.display_name_ = "Status";
            cached.is_read_only_ = true;
            cached.type_ = daq::ctString;
            cached.value_ = "OK";
            try
            {
                auto status_container = signal.getStatusContainer();
                if (status_container.assigned())
                {
                    auto statuses = status_container.getStatuses();
                    if (statuses.assigned() && statuses.getCount() > 0)
                        cached.value_ = "Multiple statuses available";
                }
            } catch (...)
            {
                cached.value_ = "<unavailable>";
            }
            cached.is_detail_ = true;
            attributes_.push_back(cached);
        }
        
        if (auto descriptor = signal.getDescriptor(); descriptor.assigned())
        {
            {
                CachedProperty cached;
                cached.owner_ = this;
                cached.name_ = "@SD_SampleType";
                cached.uid_ = "@SD_SampleType";
                cached.display_name_ = "Sample Type";
                cached.is_read_only_ = true;
                cached.type_ = daq::ctString;
                try
                {
                    cached.value_ = SampleTypeToString(descriptor.getSampleType());
                }
                catch (...)
                {
                    cached.value_ = std::string("<unavailable>");
                }
                signal_descriptor_properties_.push_back(cached);
            }
            {
                CachedProperty cached;
                cached.owner_ = this;
                cached.name_ = "@SD_Name";
                cached.uid_ = "@SD_Name";
                cached.display_name_ = "Name";
                cached.is_read_only_ = true;
                cached.type_ = daq::ctString;
                cached.value_ = descriptor.getName().assigned() ? descriptor.getName().toStdString() : "None";
                signal_descriptor_properties_.push_back(cached);
            }
            {
                CachedProperty cached;
                cached.owner_ = this;
                cached.name_ = "@SD_Dimensions";
                cached.uid_ = "@SD_Dimensions";
                cached.display_name_ = "Dimensions";
                cached.is_read_only_ = true;
                cached.type_ = daq::ctString;
                if (descriptor.getDimensions().assigned())
                {
                    auto dimensions = descriptor.getDimensions();
                    try
                    {
                        std::string text = "[";
                        for (int i = 0; i < dimensions.getCount(); i++)
                        {
                            if (i > 0) text += ", ";
                            text += std::to_string((long long)dimensions.getItemAt(i).asPtr<daq::IInteger>());
                        }
                        text += "]";
                        cached.value_ = text;
                    } catch (...)
                    {
                        cached.value_ = std::string("<error>");
                    }
                }
                else
                    cached.value_ = std::string("None");
                signal_descriptor_properties_.push_back(cached);
            }
            {
                CachedProperty cached;
                cached.owner_ = this;
                cached.name_ = "@SD_Origin";
                cached.uid_ = "@SD_Origin";
                cached.display_name_ = "Origin";
                cached.is_read_only_ = true;
                cached.type_ = daq::ctString;
                cached.value_ = descriptor.getOrigin().assigned() ? descriptor.getOrigin().toStdString() : "None";
                signal_descriptor_properties_.push_back(cached);
            }
            {
                CachedProperty cached;
                cached.owner_ = this;
                cached.name_ = "@SD_RawSampleSize";
                cached.uid_ = "@SD_RawSampleSize";
                cached.display_name_ = "Raw Sample Size";
                cached.is_read_only_ = true;
                cached.type_ = daq::ctString;
                cached.value_ = std::to_string(descriptor.getRawSampleSize());
                signal_descriptor_properties_.push_back(cached);
            }
            {
                CachedProperty cached;
                cached.owner_ = this;
                cached.name_ = "@SD_SampleSize";
                cached.uid_ = "@SD_SampleSize";
                cached.display_name_ = "Sample Size";
                cached.is_read_only_ = true;
                cached.type_ = daq::ctString;
                cached.value_ = std::to_string(descriptor.getSampleSize());
                signal_descriptor_properties_.push_back(cached);
            }
            {
                CachedProperty cached;
                cached.owner_ = this;
                cached.name_ = "@SD_TickResolution";
                cached.uid_ = "@SD_TickResolution";
                cached.display_name_ = "Tick Resolution";
                cached.is_read_only_ = true;
                cached.type_ = daq::ctString;
                if (auto tick_res = descriptor.getTickResolution(); tick_res.assigned())
                    cached.value_ = std::to_string((long long)tick_res.getNumerator()) + "/" + std::to_string((long long)tick_res.getDenominator());
                else
                    cached.value_ = std::string("None");
                signal_descriptor_properties_.push_back(cached);
            }
            {
                CachedProperty cached;
                cached.owner_ = this;
                cached.name_ = "@SD_Unit";
                cached.uid_ = "@SD_Unit";
                cached.display_name_ = "Unit";
                cached.is_read_only_ = true;
                cached.type_ = daq::ctString;
                if (auto unit = descriptor.getUnit(); unit.assigned())
                {
                    std::string symbol = unit.getSymbol().assigned() ? unit.getSymbol().toStdString() : "None";
                    std::string quantity = unit.getQuantity().assigned() ? unit.getQuantity().toStdString() : "None";
                    cached.value_ = symbol + " (" + quantity + ")";
                }
                else
                    cached.value_ = std::string("None");
                signal_descriptor_properties_.push_back(cached);
            }
            {
                CachedProperty cached;
                cached.owner_ = this;
                cached.name_ = "@SD_Rule";
                cached.uid_ = "@SD_Rule";
                cached.display_name_ = "Rule";
                cached.is_read_only_ = true;
                cached.type_ = daq::ctString;
                try
                {
                    if (auto rule = descriptor.getRule(); rule.assigned())
                        cached.value_ = ValueToString(rule);
                    else
                        cached.value_ = std::string("None");
                }
                catch (...)
                {
                    cached.value_ = std::string("<unavailable>");
                }
                signal_descriptor_properties_.push_back(cached);
            }
            {
                CachedProperty cached;
                cached.owner_ = this;
                cached.name_ = "@SD_ValueRange";
                cached.uid_ = "@SD_ValueRange";
                cached.display_name_ = "Value Range";
                cached.is_read_only_ = true;
                cached.type_ = daq::ctString;
                try
                {
                    if (auto range = descriptor.getValueRange(); range.assigned())
                    {
                        auto low = range.getLowValue();
                        auto high = range.getHighValue();
                        std::string low_str = low.assigned() ? std::to_string((double)low.asPtr<daq::IFloat>()) : "None";
                        std::string high_str = high.assigned() ? std::to_string((double)high.asPtr<daq::IFloat>()) : "None";
                        cached.value_ = "[" + low_str + ", " + high_str + "]";
                    }
                    else
                        cached.value_ = std::string("None");
                }
                catch (...)
                {
                    cached.value_ = std::string("<unavailable>");
                }
                signal_descriptor_properties_.push_back(cached);
            }
            {
                CachedProperty cached;
                cached.owner_ = this;
                cached.name_ = "@SD_PostScaling";
                cached.uid_ = "@SD_PostScaling";
                cached.display_name_ = "Post Scaling";
                cached.is_read_only_ = true;
                cached.type_ = daq::ctString;
                try
                {
                    if (auto scaling = descriptor.getPostScaling(); scaling.assigned())
                        cached.value_ = static_cast<std::string>(scaling.asPtr<daq::IBaseObject>().toString());
                    else
                        cached.value_ = std::string("None");
                }
                catch (...)
                {
                    cached.value_ = std::string("<unavailable>");
                }
                signal_descriptor_properties_.push_back(cached);
            }
            {
                CachedProperty cached;
                cached.owner_ = this;
                cached.name_ = "@SD_StructFields";
                cached.uid_ = "@SD_StructFields";
                cached.display_name_ = "Struct Fields";
                cached.is_read_only_ = true;
                cached.type_ = daq::ctString;
                try
                {
                    if (auto fields = descriptor.getStructFields(); fields.assigned())
                    {
                        std::string text = "[";
                        for (int i = 0; i < fields.getCount(); i++)
                        {
                            if (i > 0) text += ", ";
                            text += static_cast<std::string>(fields.getItemAt(i).asPtr<daq::IBaseObject>().toString());
                        }
                        text += "]";
                        cached.value_ = text;
                    }
                    else
                        cached.value_ = std::string("None");
                }
                catch (...)
                {
                    cached.value_ = std::string("<unavailable>");
                }
                signal_descriptor_properties_.push_back(cached);
            }
            {
                CachedProperty cached;
                cached.owner_ = this;
                cached.name_ = "@SD_Metadata";
                cached.uid_ = "@SD_Metadata";
                cached.display_name_ = "Metadata";
                cached.is_read_only_ = true;
                cached.type_ = daq::ctString;
                try
                {
                    if (auto metadata = descriptor.getMetadata(); metadata.assigned())
                        cached.value_ = DictToString(metadata);
                    else
                        cached.value_ = std::string("None");
                }
                catch (...)
                {
                    cached.value_ = std::string("<unavailable>");
                }
                signal_descriptor_properties_.push_back(cached);
            }
        }

        if (auto domain_signal = signal.getDomainSignal(); domain_signal.assigned())
        {
            if (auto descriptor = domain_signal.getDescriptor(); descriptor.assigned())
            {
                {
                    CachedProperty cached;
                    cached.owner_ = this;
                    cached.name_ = "@DSD_SampleType";
                    cached.uid_ = "@DSD_SampleType";
                    cached.display_name_ = "Sample Type";
                    cached.is_read_only_ = true;
                    cached.type_ = daq::ctString;
                    try
                    {
                        cached.value_ = SampleTypeToString(descriptor.getSampleType());
                    }
                    catch (...)
                    {
                        cached.value_ = std::string("<unavailable>");
                    }
                    signal_domain_descriptor_properties_.push_back(cached);
                }
                {
                    CachedProperty cached;
                    cached.owner_ = this;
                    cached.name_ = "@DSD_Name";
                    cached.uid_ = "@DSD_Name";
                    cached.display_name_ = "Name";
                    cached.is_read_only_ = true;
                    cached.type_ = daq::ctString;
                    cached.value_ = descriptor.getName().assigned() ? descriptor.getName().toStdString() : "None";
                    signal_domain_descriptor_properties_.push_back(cached);
                }
                {
                    CachedProperty cached;
                    cached.owner_ = this;
                    cached.name_ = "@DSD_Dimensions";
                    cached.uid_ = "@DSD_Dimensions";
                    cached.display_name_ = "Dimensions";
                    cached.is_read_only_ = true;
                    cached.type_ = daq::ctString;
                    if (descriptor.getDimensions().assigned())
                    {
                        auto dimensions = descriptor.getDimensions();
                        try
                        {
                            std::string text = "[";
                            for (int i = 0; i < dimensions.getCount(); i++)
                            {
                                if (i > 0) text += ", ";
                                text += std::to_string((long long)dimensions.getItemAt(i).asPtr<daq::IInteger>());
                            }
                            text += "]";
                            cached.value_ = text;
                        }
                        catch (...)
                        {
                            cached.value_ = std::string("<error>");
                        }
                    }
                    else
                        cached.value_ = std::string("None");
                    signal_domain_descriptor_properties_.push_back(cached);
                }
                {
                    CachedProperty cached;
                    cached.owner_ = this;
                    cached.name_ = "@DSD_Origin";
                    cached.uid_ = "@DSD_Origin";
                    cached.display_name_ = "Origin";
                    cached.is_read_only_ = true;
                    cached.type_ = daq::ctString;
                    cached.value_ = descriptor.getOrigin().assigned() ? descriptor.getOrigin().toStdString() : "None";
                    signal_domain_descriptor_properties_.push_back(cached);
                }
                {
                    CachedProperty cached;
                    cached.owner_ = this;
                    cached.name_ = "@DSD_RawSampleSize";
                    cached.uid_ = "@DSD_RawSampleSize";
                    cached.display_name_ = "Raw Sample Size";
                    cached.is_read_only_ = true;
                    cached.type_ = daq::ctString;
                    cached.value_ = std::to_string(descriptor.getRawSampleSize());
                    signal_domain_descriptor_properties_.push_back(cached);
                }
                {
                    CachedProperty cached;
                    cached.owner_ = this;
                    cached.name_ = "@DSD_SampleSize";
                    cached.uid_ = "@DSD_SampleSize";
                    cached.display_name_ = "Sample Size";
                    cached.is_read_only_ = true;
                    cached.type_ = daq::ctString;
                    cached.value_ = std::to_string(descriptor.getSampleSize());
                    signal_domain_descriptor_properties_.push_back(cached);
                }
                {
                    CachedProperty cached;
                    cached.owner_ = this;
                    cached.name_ = "@DSD_TickResolution";
                    cached.uid_ = "@DSD_TickResolution";
                    cached.display_name_ = "Tick Resolution";
                    cached.is_read_only_ = true;
                    cached.type_ = daq::ctString;
                    if (auto tick_res = descriptor.getTickResolution(); tick_res.assigned())
                        cached.value_ = std::to_string((long long)tick_res.getNumerator()) + "/" + std::to_string((long long)tick_res.getDenominator());
                    else
                        cached.value_ = std::string("None");
                    signal_domain_descriptor_properties_.push_back(cached);
                }
                {
                    CachedProperty cached;
                    cached.owner_ = this;
                    cached.name_ = "@DSD_Unit";
                    cached.uid_ = "@DSD_Unit";
                    cached.display_name_ = "Unit";
                    cached.is_read_only_ = true;
                    cached.type_ = daq::ctString;
                    if (auto unit = descriptor.getUnit(); unit.assigned())
                    {
                        std::string symbol = unit.getSymbol().assigned() ? unit.getSymbol().toStdString() : "None";
                        std::string quantity = unit.getQuantity().assigned() ? unit.getQuantity().toStdString() : "None";
                        cached.value_ = symbol + " (" + quantity + ")";
                    }
                    else
                        cached.value_ = std::string("None");
                    signal_domain_descriptor_properties_.push_back(cached);
                }
                {
                    CachedProperty cached;
                    cached.owner_ = this;
                    cached.name_ = "@DSD_Rule";
                    cached.uid_ = "@DSD_Rule";
                    cached.display_name_ = "Rule";
                    cached.is_read_only_ = true;
                    cached.type_ = daq::ctString;
                    try
                    {
                        if (auto rule = descriptor.getRule(); rule.assigned())
                            cached.value_ = ValueToString(rule);
                        else
                            cached.value_ = std::string("None");
                    }
                    catch (...)
                    {
                        cached.value_ = std::string("<unavailable>");
                    }
                    signal_domain_descriptor_properties_.push_back(cached);
                }
                {
                    CachedProperty cached;
                    cached.owner_ = this;
                    cached.name_ = "@DSD_ValueRange";
                    cached.uid_ = "@DSD_ValueRange";
                    cached.display_name_ = "Value Range";
                    cached.is_read_only_ = true;
                    cached.type_ = daq::ctString;
                    try
                    {
                        if (auto range = descriptor.getValueRange(); range.assigned())
                        {
                            auto low = range.getLowValue();
                            auto high = range.getHighValue();
                            std::string low_str = low.assigned() ? std::to_string((double)low.asPtr<daq::IFloat>()) : "None";
                            std::string high_str = high.assigned() ? std::to_string((double)high.asPtr<daq::IFloat>()) : "None";
                            cached.value_ = "[" + low_str + ", " + high_str + "]";
                        }
                        else
                            cached.value_ = std::string("None");
                    }
                    catch (...)
                    {
                        cached.value_ = std::string("<unavailable>");
                    }
                    signal_domain_descriptor_properties_.push_back(cached);
                }
                {
                    CachedProperty cached;
                    cached.owner_ = this;
                    cached.name_ = "@DSD_PostScaling";
                    cached.uid_ = "@DSD_PostScaling";
                    cached.display_name_ = "Post Scaling";
                    cached.is_read_only_ = true;
                    cached.type_ = daq::ctString;
                    try
                    {
                        if (auto scaling = descriptor.getPostScaling(); scaling.assigned())
                            cached.value_ = static_cast<std::string>(scaling.asPtr<daq::IBaseObject>().toString());
                        else
                            cached.value_ = std::string("None");
                    }
                    catch (...)
                    {
                        cached.value_ = std::string("<unavailable>");
                    }
                    signal_domain_descriptor_properties_.push_back(cached);
                }
                {
                    CachedProperty cached;
                    cached.owner_ = this;
                    cached.name_ = "@DSD_StructFields";
                    cached.uid_ = "@DSD_StructFields";
                    cached.display_name_ = "Struct Fields";
                    cached.is_read_only_ = true;
                    cached.type_ = daq::ctString;
                    try
                    {
                        if (auto fields = descriptor.getStructFields(); fields.assigned())
                        {
                            std::string text = "[";
                            for (int i = 0; i < fields.getCount(); i++)
                            {
                                if (i > 0) text += ", ";
                                text += static_cast<std::string>(fields.getItemAt(i).asPtr<daq::IBaseObject>().toString());
                            }
                            text += "]";
                            cached.value_ = text;
                        }
                        else
                            cached.value_ = std::string("None");
                    }
                    catch (...)
                    {
                        cached.value_ = std::string("<unavailable>");
                    }
                    signal_domain_descriptor_properties_.push_back(cached);
                }
                {
                    CachedProperty cached;
                    cached.owner_ = this;
                    cached.name_ = "@DSD_Metadata";
                    cached.uid_ = "@DSD_Metadata";
                    cached.display_name_ = "Metadata";
                    cached.is_read_only_ = true;
                    cached.type_ = daq::ctString;
                    try
                    {
                        if (auto metadata = descriptor.getMetadata(); metadata.assigned())
                            cached.value_ = DictToString(metadata);
                        else
                            cached.value_ = std::string("None");
                    }
                    catch (...)
                    {
                        cached.value_ = std::string("<unavailable>");
                    }
                    signal_domain_descriptor_properties_.push_back(cached);
                }
            }
        }
    }

    if (canCastTo<daq::IInputPort>(component_))
    {
        daq::InputPortPtr input_port = castTo<daq::IInputPort>(component_);
        
        {
            CachedProperty cached;
            cached.owner_ = this;
            cached.name_ = "@SignalID";
            cached.uid_ = "@SignalID";
            cached.display_name_ = "Signal ID";
            cached.is_read_only_ = true;
            cached.type_ = daq::ctString;
            cached.value_ = input_port.getSignal().assigned()
                          ? input_port.getSignal().getGlobalId().toStdString()
                          : std::string("");
            cached.is_detail_ = true;
            attributes_.push_back(cached);
        }
        {
            CachedProperty cached;
            cached.owner_ = this;
            cached.name_ = "@RequiresSignal";
            cached.uid_ = "@RequiresSignal";
            cached.display_name_ = "Requires Signal";
            cached.is_read_only_ = true;
            cached.type_ = daq::ctBool;
            cached.value_ = (bool)input_port.getRequiresSignal();
            cached.is_detail_ = true;
            attributes_.push_back(cached);
        }
        {
            CachedProperty cached;
            cached.owner_ = this;
            cached.name_ = "@Status";
            cached.uid_ = "@Status";
            cached.display_name_ = "Status";
            cached.is_read_only_ = true;
            cached.type_ = daq::ctString;
            cached.value_ = "OK";
            try
            {
                auto status_container = input_port.getStatusContainer();
                if (status_container.assigned())
                {
                    auto statuses = status_container.getStatuses();
                    if (statuses.assigned() && statuses.getCount() > 0)
                        cached.value_ = "Multiple statuses available";
                }
            }
            catch (...)
            {
                cached.value_ = "<unavailable>";
            }
            cached.is_detail_ = true;
            attributes_.push_back(cached);
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
            owner_->needs_refresh_ = true;
            return;
        }

        assert(canCastTo<daq::IPropertyObject>(component));
        daq::PropertyObjectPtr property_holder = castTo<daq::IPropertyObject>(component);
        switch (type_)
        {
            case daq::ctBool:
                property_holder.setPropertyValue(name_, std::get<bool>(value)); break;
            case daq::ctInt:
                property_holder.setPropertyValue(name_, std::get<int64_t>(value)); break;
            case daq::ctFloat:
                property_holder.setPropertyValue(name_, std::get<double>(value)); break;
            case daq::ctString:
                property_holder.setPropertyValue(name_, std::get<std::string>(value)); break;
            case daq::ctProc:
                property_holder.getPropertyValue(name_).asPtr<daq::IProcedure>().dispatch(); break;
            default:
                assert(false && "unsupported property type");
                return;
        }

        owner_->needs_refresh_ = true;
    }
    catch (...)
    {
    }
}
