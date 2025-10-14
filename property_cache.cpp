#include "property_cache.h"
#include "opendaq_control.h"


CachedComponent::CachedComponent(daq::ComponentPtr component)
    : components_({component})
{
    Refresh();
}

CachedComponent::CachedComponent(const std::vector<daq::ComponentPtr>& components)
    : components_(components)
{
    Refresh();
}

void CachedComponent::Refresh()
{
    needs_refresh_ = false;

    if (components_.empty())
        return;
    
    properties_.clear();
    
    daq::PropertyObjectPtr property_holder = castTo<daq::IPropertyObject>(components_[0]);
    for (const auto& prop : property_holder.getVisibleProperties())
    {
        CachedProperty cached;
        cached.property_ = prop;
        cached.owner_ = this;
        cached.name_ = prop.getName().toStdString();
        cached.read_only_ = prop.getReadOnly();
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
                {
                    cached.value_ = (int64_t)property_holder.getPropertyValue(cached.name_);
                    break;
                }
                case daq::ctFloat:
                    cached.value_ = (double)property_holder.getPropertyValue(cached.name_);
                    break;
                case daq::ctString:
                    cached.value_ = static_cast<std::string>(property_holder.getPropertyValue(cached.name_));
                    break;
                case daq::ctProc:
                case daq::ctObject:
                    break;
                default:
                    break;
            }
        }
        catch (...) {}

        if (auto sv = prop.getSelectionValues(); sv.assigned())
        {
            std::stringstream values;
            daq::ListPtr<daq::IString> selection_values;
            if (sv.supportsInterface<daq::IList>())
                selection_values = sv;
            else if (sv.supportsInterface<daq::IDict>())
                selection_values = daq::DictPtr<daq::IInteger, daq::IString>(sv).getValueList();
            
            if (selection_values.assigned())
            {
                for (int i = 0; i < selection_values.getCount(); i++)
                    values << selection_values.getItemAt(i).toStdString() << '\0';
                cached.selection_values_ = values.str();
                cached.selection_values_count_ = selection_values.getCount();
            }
        }

        properties_.push_back(cached);
    }
}

void CachedProperty::SetValue(ValueType value)
{
    for (daq::ComponentPtr component : owner_->components_)
    {
        if (!component.assigned())
            continue;

        try
        {
            if (!canCastTo<daq::IPropertyObject>(component))
                continue;

            daq::PropertyObjectPtr property_holder = castTo<daq::IPropertyObject>(component);

            if (type_ == daq::ctProc)
                property_holder.getPropertyValue(name_).asPtr<daq::IProcedure>().dispatch();
            else if (std::holds_alternative<bool>(value))
                property_holder.setPropertyValue(name_, std::get<bool>(value));
            else if (std::holds_alternative<int64_t>(value))
                property_holder.setPropertyValue(name_, std::get<int64_t>(value));
            else if (std::holds_alternative<double>(value))
                property_holder.setPropertyValue(name_, std::get<double>(value));
            else if (std::holds_alternative<std::string>(value))
                property_holder.setPropertyValue(name_, std::get<std::string>(value));

            owner_->needs_refresh_ = true;
        }
        catch (...)
        {
        }
    }
}
