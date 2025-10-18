#pragma once
#include <opendaq/opendaq.h>
#include <string>


template <class Interface>
inline bool canCastTo(daq::IBaseObject* baseObject)
{
    return daq::BaseObjectPtr::Borrow(baseObject).asPtrOrNull<Interface>().assigned();
}
template <class Interface>
inline auto castTo(daq::IBaseObject* baseObject)
{
    return daq::BaseObjectPtr::Borrow(baseObject).as<Interface>();
}

inline std::string OperationModeToString(daq::OperationModeType mode)
{
    switch (mode)
    {
        case daq::OperationModeType::Idle: return "Idle";
        case daq::OperationModeType::Operation: return "Operation";
        case daq::OperationModeType::SafeOperation: return "Safe Operation";
        default: return "Unknown";
    }
}

inline std::string SampleTypeToString(daq::SampleType sample_type)
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

inline std::string DataRuleTypeToString(daq::DataRuleType type)
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

inline std::string ValueToString(daq::BaseObjectPtr value)
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

inline std::string DictToString(daq::DictPtr<daq::IString, daq::IBaseObject> dict)
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

