#pragma once
#include <opendaq/opendaq.h>
#include <variant>
#include <string>
#include <optional>


struct CachedProperty;

struct CachedComponent
{
    CachedComponent(daq::ComponentPtr component);
    CachedComponent(const std::vector<daq::ComponentPtr>& component);

    std::string name_;
    std::optional<std::string> warning_message_;
    std::optional<std::string> error_message_;
    std::vector<daq::ComponentPtr> components_; // either one component, or multiple components with the same properties
    std::vector<CachedProperty> properties_;
};

struct CachedProperty
{
    struct DifferingValueType {};
    using ValueType = std::variant<std::string, int64_t, double, bool, struct DifferingValueType>;

    void SetValue(ValueType value);

    daq::PropertyPtr property;
    CachedComponent* owner = nullptr;

    std::string name_;
    bool read_only_;
    ValueType value_; // TODO: could also be nested property, could also be structure (which is a kind-of nested property with read-only children)
    std::optional<double> min_value_;
    std::optional<double> max_value_;
    std::optional<std::string> selection_values_;
};
