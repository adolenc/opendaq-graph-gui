#pragma once
#include <opendaq/opendaq.h>
#include "nodes.h"
#include <variant>
#include <string>
#include <optional>
#include <vector>


struct CachedComponent;

struct CachedProperty
{
    using ValueType = std::variant<std::string, int64_t, double, bool>;

    void SetValue(ValueType value);

    daq::PropertyPtr property_;
    CachedComponent* owner_ = nullptr;

    daq::CoreType type_;
    std::string name_;
    std::string uid_;
    std::string unit_;
    std::string display_name_;
    int depth_{0};
    bool is_read_only_{false};
    bool is_debug_property_{false};
    ValueType value_;
    std::optional<double> min_value_;
    std::optional<double> max_value_;
    std::optional<std::string> selection_values_;
    int selection_values_count_ = 0;

    struct FunctionInfo
    {
        struct Parameter
        {
            std::string name;
            daq::CoreType type;
            ValueType value;
        };
        std::vector<Parameter> parameters;
        daq::CoreType return_type;
        std::string last_execution_result;
    };
    std::optional<FunctionInfo> function_info_;

    void EnsureFunctionInfoCached();
};

struct CachedComponent
{
    CachedComponent(daq::ComponentPtr component);

    void UpdateState();
    void RefreshStatus();
    void RefreshProperties();
    void RefreshStructure();
    void AddProperty(daq::PropertyPtr prop, daq::PropertyObjectPtr property_holder, int depth = 0, const std::string& parent_uid = "");
    void AddDescriptorProperties(daq::DataDescriptorPtr descriptor, std::vector<CachedProperty>& properties, bool is_domain_signal = false);
    CachedProperty& AddAttribute(std::vector<CachedProperty>& properties, const std::string& name, const std::string& display_name, CachedProperty::ValueType value, bool is_read_only = true, bool is_debug_property = false, daq::CoreType type = daq::ctString);

    daq::ComponentPtr component_;
    daq::ComponentPtr parent_; // the parent component in the hierarchy (although some folders are skipped)
    daq::ComponentPtr owner_; // the component that can delete this one

    std::string name_;
    std::string uid_;
    std::string warning_message_;
    std::string error_message_;
    bool is_active_;
    bool is_locked_ = false;
    std::string operation_mode_;
    std::vector<CachedProperty> attributes_;
    std::vector<CachedProperty> properties_;
    std::vector<CachedProperty> signal_descriptor_properties_;
    std::vector<CachedProperty> signal_domain_descriptor_properties_;

    std::vector<ImGui::ImGuiNodesIdentifier> input_ports_;
    std::vector<ImGui::ImGuiNodesIdentifier> output_signals_;
    std::vector<ImGui::ImGuiNodesIdentifier> children_;

    int color_index_ = 0;
    std::optional<ImVec4> signal_color_;

    bool needs_refresh_ = false;
    bool initial_properties_loaded_ = false;
};
