#include "properties_window.h"
#include "utils.h"
#include "IconsFontAwesome6.h"
#include <string>
#include "imgui.h"
#include "imgui_stdlib.h"
#include <map>
#include <sstream>


SharedCachedComponent::SharedCachedComponent(const std::vector<CachedComponent*>& components, const std::string& group_name)
{
    source_components_ = components;
    if (components.empty())
        return;

    if (group_name.empty())
    {
        if (components.size() == 1)
            name_ = components[0]->name_;
        else
            name_ = "Unknown";
    }
    else
        name_ = group_name;
    name_ += ((components.size() > 1) ? " (" + std::to_string(components.size()) + ")" : "");

    CachedComponent* base = components[0];

    auto merge_list = [&](std::vector<CachedProperty> CachedComponent::* list, std::vector<SharedCachedProperty>& target_list)
    {
        std::vector<CachedProperty>& base_list = base->*list;
        for (CachedProperty& base_prop : base_list)
        {
            bool found_in_all_components = true;
            bool same_value_in_all_components = true;

            std::optional<double> final_min = base_prop.min_value_;
            std::optional<double> final_max = base_prop.max_value_;

            std::vector<CachedProperty*> targets;
            targets.push_back(&base_prop);

            for (size_t i = 1; i < components.size(); ++i)
            {
                bool matching_property_found = false;
                std::vector<CachedProperty>& other_list = components[i]->*list;
                for (CachedProperty& prop : other_list)
                {
                    if (!(prop.name_ == base_prop.name_ && prop.type_ == base_prop.type_))
                        continue;

                    matching_property_found = true;
                    if (prop.value_ != base_prop.value_)
                        same_value_in_all_components = false;

                    if (prop.min_value_.has_value())
                    {
                        if (!final_min.has_value() || prop.min_value_.value() > final_min.value())
                            final_min = prop.min_value_;
                    }
                    if (prop.max_value_.has_value())
                    {
                        if (!final_max.has_value() || prop.max_value_.value() < final_max.value())
                            final_max = prop.max_value_;
                    }
                    targets.push_back(&prop);
                    break;
                }
                if (!matching_property_found)
                {
                    found_in_all_components = false;
                    break;
                }
            }

            if (found_in_all_components)
            {
                SharedCachedProperty new_prop;
                static_cast<CachedProperty&>(new_prop) = base_prop;
                new_prop.min_value_ = final_min;
                new_prop.max_value_ = final_max;
                new_prop.target_properties_ = targets;
                new_prop.is_multi_value_ = !same_value_in_all_components;
                target_list.push_back(new_prop);
            }
        }
    };

    merge_list(&CachedComponent::attributes_, attributes_);
    merge_list(&CachedComponent::properties_, properties_);
    merge_list(&CachedComponent::signal_descriptor_properties_, signal_descriptor_properties_);
    merge_list(&CachedComponent::signal_domain_descriptor_properties_, signal_domain_descriptor_properties_);
}


PropertiesWindow::PropertiesWindow(const PropertiesWindow& other)
{
    // grouped_selected_components_ are not copied, they will be regenerated
    selected_component_ids_ = other.selected_component_ids_;
    freeze_selection_ = true;
    show_parents_and_children_ = other.show_parents_and_children_;
    tabbed_interface_ = other.tabbed_interface_;
    show_debug_properties_ = other.show_debug_properties_;
    is_cloned_ = true;
    on_reselect_click_ = other.on_reselect_click_;
    on_property_changed_ = other.on_property_changed_;
    all_components_ = other.all_components_;
    group_components_ = other.group_components_;

    RebuildComponents();
}

static void RenderFunctionProperty(SharedCachedProperty& cached_prop)
{
    ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    if (ImGui::CollapsingHeader(cached_prop.display_name_.c_str(), ImGuiTreeNodeFlags_SpanLabelWidth))
    {
        cached_prop.EnsureFunctionInfoCached();
        ImGui::Indent();

        CachedProperty::FunctionInfo& fn_info = *cached_prop.function_info_;
        for (size_t i = 0; i < fn_info.parameters.size(); ++i)
        {
            auto& param = fn_info.parameters[i];
            ImGui::PushID((int)i);
            switch (param.type)
            {
                case daq::ctBool:
                {
                    bool v = std::get<bool>(param.value);
                    if (ImGui::Checkbox(param.name.c_str(), &v))
                        param.value = v;
                    break;
                }
                case daq::ctInt:
                {
                    int64_t v = std::get<int64_t>(param.value);
                    int temp = (int)v;
                    if (ImGui::InputInt(param.name.c_str(), &temp))
                        param.value = (int64_t)temp;
                    break;
                }
                case daq::ctFloat:
                {
                    double v = std::get<double>(param.value);
                    if (ImGui::InputDouble(param.name.c_str(), &v))
                        param.value = v;
                    break;
                }
                case daq::ctString:
                {
                    std::string v = std::get<std::string>(param.value);
                    if (ImGui::InputText(param.name.c_str(), &v))
                        param.value = v;
                    break;
                }
                default:
                    ImGui::Text("Unsupported type for %s", param.name.c_str());
                    break;
            }
            ImGui::PopID();
        }

        if (ImGui::Button("Execute"))
        {
            daq::ListPtr<daq::IBaseObject> args = daq::List<daq::IBaseObject>();
            for (const auto& param : fn_info.parameters)
            {
                if (std::holds_alternative<std::string>(param.value))
                    args.pushBack(daq::String(std::get<std::string>(param.value)));
                else if (std::holds_alternative<int64_t>(param.value))
                    args.pushBack(daq::Int(std::get<int64_t>(param.value)));
                else if (std::holds_alternative<double>(param.value))
                    args.pushBack(daq::Float(std::get<double>(param.value)));
                else if (std::holds_alternative<bool>(param.value))
                    args.pushBack(daq::Boolean(std::get<bool>(param.value)));
            }

            std::vector<CachedProperty*> targets = cached_prop.target_properties_;
            if (targets.empty())
                targets.push_back(&cached_prop);

            for (CachedProperty* target : targets)
            {
                target->EnsureFunctionInfoCached();
                std::string& execution_result = target->function_info_->last_execution_result;

                try
                {
                    daq::BaseObjectPtr value = target->property_.getValue();
                    daq::BaseObjectPtr result;

                    if (cached_prop.type_ == daq::ctFunc)
                    {
                        daq::FunctionPtr func = value.asPtrOrNull<daq::IFunction>();
                        // There is no way this is the way to properly call functions in openDAQ... Huh??
                        if (args.getCount() == 0)
                            result = func.call();
                        else if (args.getCount() == 1)
                            result = func.call(args[0]);
                        else
                            result = func.call(args);
                    }
                    else
                    {
                        daq::ProcedurePtr proc = value.asPtrOrNull<daq::IProcedure>();
                        if (args.getCount() == 0)
                            proc.dispatch();
                        else if (args.getCount() == 1)
                            proc.dispatch(args[0]);
                        else
                            proc.dispatch(args);
                        result = nullptr;
                    }

                     execution_result = result.assigned() ? ValueToString(result) : "Success";
                }
                catch (const std::exception& e)
                {
                    execution_result = std::string("Error: ") + e.what();
                }
                catch (...)
                {
                    execution_result = "Unknown error";
                }
            }
        }

        std::vector<CachedProperty*> targets = cached_prop.target_properties_;
        if (targets.empty())
            targets.push_back(&cached_prop);

        if (targets.size() == 1)
        {
            CachedProperty* target = targets[0];
            if (target->function_info_.has_value())
            {
                ImGui::SameLine();
                ImGui::InputText("##ExecutionResult", &target->function_info_->last_execution_result, ImGuiInputTextFlags_ReadOnly);
            }
        }
        else
        {
            ImGui::Indent();
            for (size_t i = 0; i < targets.size(); ++i)
            {
                CachedProperty* target = targets[i];
                if (!target->function_info_.has_value())
                    continue;
                ImGui::InputText((target->owner_->name_ + "##ExecutionResult" + std::to_string(i)).c_str(), &target->function_info_->last_execution_result, ImGuiInputTextFlags_ReadOnly);
            }
            ImGui::Unindent();
        }

        ImGui::Unindent();
    }
    ImGui::PopStyleColor(1);
}

void PropertiesWindow::RenderProperty(SharedCachedProperty& cached_prop, SharedCachedComponent* owner)
{
    if (!show_debug_properties_ && cached_prop.is_debug_property_)
        return;

    ImGui::PushID(cached_prop.uid_.c_str());

    if (cached_prop.is_multi_value_)
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_NavHighlight]);

    bool is_disabled = cached_prop.is_read_only_ && cached_prop.type_ != daq::ctProc && cached_prop.type_ != daq::ctFunc;
    if (is_disabled)
        ImGui::BeginDisabled();

    if (cached_prop.depth_ > 0)
        ImGui::Indent(cached_prop.depth_ * 10.0f);

    auto SetValue = [&](const CachedProperty::ValueType& val) {
        for (CachedProperty* target : cached_prop.target_properties_)
            target->SetValue(val);
        if (on_property_changed_)
        {
            for (CachedProperty* target : cached_prop.target_properties_)
            {
                if (target->owner_)
                    on_property_changed_(target->owner_->uid_, target->name_);
            }
        }
        if (owner)
            owner->needs_refresh_ = true;
    };

    switch (cached_prop.type_)
    {
        case daq::ctBool:
            assert(std::holds_alternative<bool>(cached_prop.value_));
            {
                bool value = std::get<bool>(cached_prop.value_);
                if (ImGui::Checkbox(cached_prop.display_name_.c_str(), &value))
                    SetValue(value);
            }
            break;
        case daq::ctInt:
            assert(std::holds_alternative<int64_t>(cached_prop.value_));
            {
                if (cached_prop.name_ == "@SignalColor")
                {
                    ImVec4 color = ImGui::ColorConvertU32ToFloat4((ImU32)std::get<int64_t>(cached_prop.value_));
                    if (ImGui::ColorEdit4(cached_prop.display_name_.c_str(), (float*)&color, ImGuiColorEditFlags_NoInputs))
                        SetValue((int64_t)ImGui::ColorConvertFloat4ToU32(color));
                }
                else if (cached_prop.selection_values_count_ > 0)
                {
                    assert(cached_prop.selection_values_);
                    int value = (int)std::get<int64_t>(cached_prop.value_);

                    // We need to do create a combo manually because otherwise shared component properties
                    // do not correctly call SetValue when changed.
                    const char* items = cached_prop.selection_values_->c_str();
                    const char* preview = items;
                    for (int i = 0; i < value && *preview; ++i)
                        preview += strlen(preview) + 1;
                    if (ImGui::BeginCombo(cached_prop.display_name_.c_str(), *preview ? preview : ""))
                    {
                        const char* item = items;
                        for (int i = 0; i < (int)cached_prop.selection_values_count_ && *item; ++i)
                        {
                            if (ImGui::Selectable(item, i == value))
                                SetValue((int64_t)i);
                            item += strlen(item) + 1;
                        }
                        ImGui::EndCombo();
                    }
                }
                else
                {
                    int value = (int)std::get<int64_t>(cached_prop.value_);
                    ImGui::InputInt(cached_prop.display_name_.c_str(), &value, 0, 0);
                    if (ImGui::IsItemDeactivatedAfterEdit() || (cached_prop.is_multi_value_ && ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)))
                        SetValue((int64_t)value);
                }
            }
            break;
        case daq::ctFloat:
            assert(std::holds_alternative<double>(cached_prop.value_));
            {
                double value = std::get<double>(cached_prop.value_);
                ImGui::InputDouble(cached_prop.display_name_.c_str(), &value, 0.0, 0.0, "%.6f");
                if (ImGui::IsItemDeactivatedAfterEdit() || (cached_prop.is_multi_value_ && ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)))
                    SetValue(value);
            }
            break;
        case daq::ctString:
            assert(std::holds_alternative<std::string>(cached_prop.value_));
            {
                std::string value = std::get<std::string>(cached_prop.value_);
                bool entered = ImGui::InputText(cached_prop.display_name_.c_str(), &value, cached_prop.is_multi_value_ ? ImGuiInputTextFlags_EnterReturnsTrue : 0);
                if (entered || (!cached_prop.is_multi_value_ && ImGui::IsItemDeactivatedAfterEdit()))
                    SetValue(value);
                if (is_disabled && !cached_prop.is_multi_value_ && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone | ImGuiHoveredFlags_AllowWhenDisabled) && ImGui::BeginTooltip())
                {
                    ImGui::Text("%s", value.c_str());
                    ImGui::EndTooltip();
                }
            }
            break;
        case daq::ctProc:
        case daq::ctFunc:
            RenderFunctionProperty(cached_prop);
            break;
        case daq::ctObject:
            ImGui::Text("%s", cached_prop.display_name_.c_str());
            break;
        case daq::ctStruct:
            ImGui::Text("%s", cached_prop.display_name_.c_str());
            break;
        default:
            {
                std::string n = "!Unsupported prop t" + std::to_string((int)cached_prop.type_) + ": " + cached_prop.display_name_;
                ImGui::Text("%s", n.c_str());
                break;
            }
    }

    if (cached_prop.depth_ > 0)
        ImGui::Unindent(cached_prop.depth_ * 10.0f);

    if (is_disabled)
        ImGui::EndDisabled();

    if (cached_prop.is_multi_value_)
    {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            if (ImGui::BeginTooltip())
            {
                for (CachedProperty* target : cached_prop.target_properties_)
                {
                    std::string val_str;
                    if (target->type_ == daq::ctInt && target->selection_values_count_ > 0 && std::holds_alternative<int64_t>(target->value_))
                    {
                        int64_t v = std::get<int64_t>(target->value_);
                        if (target->selection_values_.has_value())
                        {
                            const char* items = target->selection_values_->c_str();
                            const char* preview = items;
                            for (int i = 0; i < v && *preview; ++i)
                                preview += strlen(preview) + 1;
                            val_str = preview;
                        }
                        else
                        {
                             val_str = std::to_string(v);
                        }
                    }
                    else if (std::holds_alternative<bool>(target->value_))
                        val_str = std::get<bool>(target->value_) ? "True" : "False";
                    else if (std::holds_alternative<int64_t>(target->value_))
                        val_str = std::to_string(std::get<int64_t>(target->value_));
                    else if (std::holds_alternative<double>(target->value_))
                        val_str = std::to_string(std::get<double>(target->value_));
                    else if (std::holds_alternative<std::string>(target->value_))
                        val_str = std::get<std::string>(target->value_);

                    ImGui::Text("%s: %s", target->owner_->name_.c_str(), val_str.c_str());
                }
                ImGui::EndTooltip();
            }
        }
        ImGui::PopStyleColor();
    }

    ImGui::PopID();
}

void PropertiesWindow::AddGroupedComponentsTooltip(SharedCachedComponent& shared_cached_component)
{
    if (!group_components_ || !ImGui::IsItemHovered())
        return;

    std::stringstream names;
    names << "Grouped components:\n";
    for (size_t i = 0; i < shared_cached_component.source_components_.size(); ++i)
    {
        if (i > 0)
            names << "\n";
        names << " - " << shared_cached_component.source_components_[i]->name_;
    }
    ImGui::SetTooltip("%s", names.str().c_str());
}

void PropertiesWindow::RenderComponent(SharedCachedComponent& shared_cached_component, bool draw_header)
{
    if (draw_header && (!tabbed_interface_ || (show_parents_and_children_ && !group_components_)))
    {
        ImGui::SetNextItemOpen(true);
        ImGui::CollapsingHeader(shared_cached_component.name_.c_str(), ImGuiTreeNodeFlags_Leaf);
        AddGroupedComponentsTooltip(shared_cached_component);
    }

    if (shared_cached_component.source_components_.size() == 1)
    {
        CachedComponent* comp = shared_cached_component.source_components_[0];
        if (!comp->error_message_.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, COLOR_ERROR);
            ImGui::TextWrapped("%s", comp->error_message_.c_str());
            ImGui::PopStyleColor();
        }
        else if (!comp->warning_message_.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, COLOR_WARNING);
            ImGui::TextWrapped("%s", comp->warning_message_.c_str());
            ImGui::PopStyleColor();
        }
    }

    for (SharedCachedProperty& cached_attr : shared_cached_component.attributes_)
        RenderProperty(cached_attr, &shared_cached_component);

    for (SharedCachedProperty& cached_prop : shared_cached_component.properties_)
        RenderProperty(cached_prop, &shared_cached_component);

    if (!shared_cached_component.signal_descriptor_properties_.empty() || !shared_cached_component.signal_domain_descriptor_properties_.empty())
    {
        if (ImGui::BeginTabBar("SignalDescriptors"))
        {
            if (!shared_cached_component.signal_descriptor_properties_.empty())
            {
                if (ImGui::BeginTabItem("Signal Descriptor"))
                {
                    for (SharedCachedProperty& cached_prop : shared_cached_component.signal_descriptor_properties_)
                        RenderProperty(cached_prop, &shared_cached_component);
                    ImGui::EndTabItem();
                }
            }
            if (!shared_cached_component.signal_domain_descriptor_properties_.empty())
            {
                if (ImGui::BeginTabItem("Domain Signal Descriptor"))
                {
                    for (SharedCachedProperty& cached_prop : shared_cached_component.signal_domain_descriptor_properties_)
                        RenderProperty(cached_prop, &shared_cached_component);
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }

    if (shared_cached_component.needs_refresh_)
    {
        for (CachedComponent* component : shared_cached_component.source_components_)
            component->RefreshProperties();
        RebuildComponents();
    }
}

void PropertiesWindow::RenderChildren(SharedCachedComponent& shared_cached_component)
{
    if (shared_cached_component.source_components_.size() != 1)
        return;

    CachedComponent* base = shared_cached_component.source_components_[0];
    if (!all_components_ || base->children_.empty())
        return;

    ImGui::Indent();
    for (const ImGui::ImGuiNodesIdentifier& child_id_struct : base->children_)
    {
        std::string child_id = child_id_struct.id_;
        auto it = all_components_->find(child_id);
        if (it == all_components_->end())
            continue;

        CachedComponent* child = it->second.get();
        if (!child->initial_properties_loaded_)
            child->RefreshProperties();

        // skip folders that are just for structure
        if (child->name_ == "IO" || child->name_ == "AI" || child->name_ == "AO" || child->name_ == "Dev" || child->name_ == "FB")
        {
            ImGui::Unindent();
            SharedCachedComponent shared_child({child});
            RenderChildren(shared_child);
            ImGui::Indent();
        }
        else
        {
            ImGui::PushID(child_id.c_str());
            if (ImGui::CollapsingHeader((child->name_ + "###" + child->uid_).c_str()))
            {
                SharedCachedComponent shared_child({child});
                RenderComponent(shared_child, false);
                if (show_parents_and_children_ && !group_components_)
                    RenderChildren(shared_child);
            }
            ImGui::PopID();
        }
    }
    ImGui::Unindent();
}

void PropertiesWindow::RenderComponentWithParents(SharedCachedComponent& shared_cached_component)
{
    if (!show_parents_and_children_ || group_components_ || !all_components_)
    {
        RenderComponent(shared_cached_component);
        return;
    }

    std::vector<CachedComponent*> parent_components;
    daq::ComponentPtr current_parent_ptr = shared_cached_component.source_components_[0]->parent_;
    while (current_parent_ptr.assigned())
    {
        std::string id = current_parent_ptr.getGlobalId().toStdString();
        auto it = all_components_->find(id);
        if (it == all_components_->end())
            break;

        parent_components.push_back(it->second.get());
        current_parent_ptr = it->second->parent_;
    }

    for (auto it = parent_components.rbegin(); it != parent_components.rend(); ++it)
    {
        if (!(*it)->initial_properties_loaded_)
            (*it)->RefreshProperties();

        if ((*it)->name_ == "IO" || (*it)->name_ == "AI" || (*it)->name_ == "AO" || (*it)->name_ == "Dev" || (*it)->name_ == "FB")
            continue;

        ImGui::PushID((*it)->component_.getGlobalId().toStdString().c_str());
        if (ImGui::CollapsingHeader((*it)->name_.c_str()))
        {
             SharedCachedComponent parent_component({*it});
             RenderComponent(parent_component, false);
        }
        ImGui::PopID();
    }

    RenderComponent(shared_cached_component);
    RenderChildren(shared_cached_component);
}

void PropertiesWindow::OnSelectionChanged(const std::vector<std::string>& selected_ids, const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components)
{
    if (freeze_selection_)
        return;

    selected_component_ids_ = selected_ids;
    RestoreSelection(all_components);
}

void PropertiesWindow::RestoreSelection(const std::unordered_map<std::string, std::unique_ptr<CachedComponent>>& all_components)
{
    all_components_ = &all_components;
    RebuildComponents();
}

void PropertiesWindow::RefreshComponents()
{
    for (SharedCachedComponent& group : grouped_selected_components_)
    {
        for (CachedComponent* component : group.source_components_)
            component->needs_refresh_ = true;
    }
}

void PropertiesWindow::Render()
{
    ImGui::SetNextWindowPos(ImVec2(300.f, 20.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(100.f, 100.f), ImGuiCond_FirstUseEver);
    
    std::string title = !is_cloned_ ? "Properties" : "Properties (cloned)##" + std::to_string((uintptr_t)this);
    if (!ImGui::Begin(title.c_str(), is_cloned_ ? &is_open_ : nullptr, is_cloned_ ? ImGuiWindowFlags_AlwaysAutoResize : 0))
    {
        ImGui::End();
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_TabSelected));
    if (is_cloned_)
    {
        if (ImGui::Button(ICON_FA_ARROWS_ROTATE))
        {
            if (on_reselect_click_)
                on_reselect_click_(selected_component_ids_);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Reapply selection");

        ImGui::SameLine();
    }

    if (!is_cloned_)
    {
        if (ImGui::Button(freeze_selection_ ? " " ICON_FA_LOCK " ": " " ICON_FA_LOCK_OPEN))
            freeze_selection_ = !freeze_selection_;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(freeze_selection_ ? "Unlock selection" : "Lock selection");

        ImGui::SameLine();

        ImGui::BeginDisabled(grouped_selected_components_.empty());
        if (ImGui::Button(ICON_FA_CLONE))
        {
            if (on_clone_click_)
                on_clone_click_(this);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Clone into a new window");
        ImGui::EndDisabled();

        ImGui::SameLine();
    }

    ImGui::BeginDisabled(group_components_);
    if (ImGui::Button((show_parents_and_children_ && !group_components_) ? ICON_FA_FOLDER_TREE " " ICON_FA_TOGGLE_ON : ICON_FA_FOLDER_TREE " " ICON_FA_TOGGLE_OFF))
        show_parents_and_children_ = !show_parents_and_children_;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(show_parents_and_children_ ? "Hide parents and children" : "Show parents and children");
    ImGui::EndDisabled();

    ImGui::SameLine();

    if (ImGui::Button(group_components_ ? ICON_FA_OBJECT_GROUP " " ICON_FA_TOGGLE_ON : ICON_FA_OBJECT_GROUP " " ICON_FA_TOGGLE_OFF))
    {
        group_components_ = !group_components_;
        RebuildComponents();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(group_components_ ? "Disable component grouping" : "Group components by type");

    ImGui::SameLine();

    if (ImGui::Button(tabbed_interface_ ? ICON_FA_TABLE_COLUMNS " " ICON_FA_TOGGLE_OFF : ICON_FA_TABLE_COLUMNS " " ICON_FA_TOGGLE_ON))
        tabbed_interface_ = !tabbed_interface_;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(tabbed_interface_ ? "Show multiple components side by side" : "Use tabs for multiple components");

    ImGui::SameLine();

    if (ImGui::Button(show_debug_properties_ ? ICON_FA_BUG " " ICON_FA_TOGGLE_ON : ICON_FA_BUG " " ICON_FA_TOGGLE_OFF))
        show_debug_properties_ = !show_debug_properties_;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(show_debug_properties_ ? "Hide debug properties" : "Show debug properties");


    if (grouped_selected_components_.empty())
    {
        ImGui::Text("No component selected");
    }
    else
    {
        bool needs_rebuild = false;
        for (auto& comp : grouped_selected_components_)
        {
            for (auto* source : comp.source_components_)
            {
                if (source->needs_refresh_)
                {
                    source->RefreshProperties();
                    needs_rebuild = true;
                }
            }
        }
        if (needs_rebuild)
            RebuildComponents();

        if (grouped_selected_components_.size() == 1)
        {
            RenderComponentWithParents(grouped_selected_components_[0]);
        }
        else if (tabbed_interface_ && grouped_selected_components_.size() > 1)
        {
            if (ImGui::BeginTabBar("Components"))
            {
                int uid = 0;
                for (auto& comp : grouped_selected_components_)
                {
                    bool open = ImGui::BeginTabItem((comp.name_ + "###" + std::to_string(uid++)).c_str());
                    AddGroupedComponentsTooltip(comp);
                    if (open)
                    {
                        RenderComponentWithParents(comp);
                        ImGui::EndTabItem();
                    }
                }
                ImGui::EndTabBar();
            }
        }
        else
        {
            ImGui::BeginChild("##ContentScrollRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
            int uid = 0;
            for (auto& comp : grouped_selected_components_)
            {
                ImGui::BeginChild((comp.name_ + "##" + std::to_string(uid++)).c_str(), ImVec2(0, 0), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);
                RenderComponentWithParents(comp);
                ImGui::EndChild();
                ImGui::SameLine();
            }
            ImGui::EndChild();
        }
    }
    ImGui::PopStyleColor(1);
    ImGui::End();
}

void PropertiesWindow::RebuildComponents()
{
    grouped_selected_components_.clear();
    if (selected_component_ids_.empty() || !all_components_)
        return;

    std::vector<CachedComponent*> all_selected_components;
    for (const std::string& id : selected_component_ids_)
    {
        if (auto it = all_components_->find(id); it != all_components_->end())
        {
            CachedComponent* comp = it->second.get();
            if (comp->properties_.empty() && comp->attributes_.empty())
                comp->RefreshProperties();
            all_selected_components.push_back(comp);
        }
    }

    if (all_selected_components.empty())
        return;

    std::map<std::string, std::vector<CachedComponent*>> component_groups;
    if (group_components_)
    {
        for (CachedComponent* comp : all_selected_components)
        {
            std::string type_id = "";
            for (const CachedProperty& attr : comp->attributes_)
            {
                if (attr.name_ == "@TypeID" && std::holds_alternative<std::string>(attr.value_))
                {
                    type_id = std::get<std::string>(attr.value_);
                    break;
                }
            }
            component_groups[type_id].push_back(comp);
        }
    }
    else
    {
        for (CachedComponent* comp : all_selected_components)
            component_groups[comp->component_.getGlobalId().toStdString()].push_back(comp);
    }

    for (auto& [type_id, components] : component_groups)
    {
        if (components.empty())
            continue;

        if (components.size() == 1)
            grouped_selected_components_.push_back(SharedCachedComponent({components[0]}, group_components_ ? type_id : components[0]->name_));
        else
            grouped_selected_components_.push_back(SharedCachedComponent(components, type_id));
    }
}
