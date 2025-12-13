#include "properties_window.h"
#include "utils.h"
#include <string>
#include "imgui.h"
#include "imgui_stdlib.h"


PropertiesWindow::PropertiesWindow(const PropertiesWindow& other)
{
    selected_cached_components_ = other.selected_cached_components_;
    selected_component_ids_ = other.selected_component_ids_;
    freeze_selection_ = true;
    show_parents_ = other.show_parents_;
    tabbed_interface_ = other.tabbed_interface_;
    show_debug_properties_ = other.show_debug_properties_;
    is_cloned_ = true;
    on_reselect_click_ = other.on_reselect_click_;
}

void PropertiesWindow::RenderCachedProperty(CachedProperty& cached_prop)
{
    if (!show_debug_properties_ && cached_prop.is_debug_property_)
        return;

    ImGui::PushID(cached_prop.uid_.c_str());

    bool is_disabled = cached_prop.is_read_only_ && cached_prop.type_ != daq::ctProc && cached_prop.type_ != daq::ctFunc;
    if (is_disabled)
        ImGui::BeginDisabled();

    if (cached_prop.depth_ > 0)
        ImGui::Indent(cached_prop.depth_ * 10.0f);

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
                if (is_disabled && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone | ImGuiHoveredFlags_AllowWhenDisabled) && ImGui::BeginTooltip())
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
        case daq::ctFunc:
            if (ImGui::Button(cached_prop.display_name_.c_str()))
                cached_prop.SetValue({});
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

    ImGui::PopID();
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

    for (auto& cached_attr : cached_component.attributes_)
        RenderCachedProperty(cached_attr);

    for (auto& cached_prop : cached_component.properties_)
        RenderCachedProperty(cached_prop);

    if (!cached_component.signal_descriptor_properties_.empty() || !cached_component.signal_domain_descriptor_properties_.empty())
    {
        if (ImGui::BeginTabBar("SignalDescriptors"))
        {
            if (!cached_component.signal_descriptor_properties_.empty())
            {
                if (ImGui::BeginTabItem("Signal Descriptor"))
                {
                    for (auto& cached_prop : cached_component.signal_descriptor_properties_)
                        RenderCachedProperty(cached_prop);
                    ImGui::EndTabItem();
                }
            }
            
            if (!cached_component.signal_domain_descriptor_properties_.empty())
            {
                if (ImGui::BeginTabItem("Domain Signal Descriptor"))
                {
                    for (auto& cached_prop : cached_component.signal_domain_descriptor_properties_)
                        RenderCachedProperty(cached_prop);
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
    
    if (cached_component.needs_refresh_)
        cached_component.RefreshProperties();
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
    selected_cached_components_.clear();
    for (const auto& id : selected_component_ids_)
    {
        if (auto it = all_components.find(id); it != all_components.end())
            selected_cached_components_.push_back(it->second.get());
    }
    
    for (auto* cached : selected_cached_components_)
    {
        if (cached)
            cached->RefreshProperties();
    }
}

void PropertiesWindow::RefreshComponents()
{
    for (auto* cached : selected_cached_components_)
        cached->needs_refresh_ = true;
}

void PropertiesWindow::Render()
{
    ImGui::SetNextWindowPos(ImVec2(300.f, 20.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(100.f, 100.f), ImGuiCond_FirstUseEver);
    
    std::string title = !is_cloned_ ? "Property editor" : "Property editor (cloned)##" + std::to_string((uintptr_t)this);
    if (!ImGui::Begin(title.c_str(), is_cloned_ ? &is_open_ : nullptr, is_cloned_ ? ImGuiWindowFlags_AlwaysAutoResize : 0))
    {
        ImGui::End();
        return;
    }

    {
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

            ImGui::BeginDisabled(selected_cached_components_.empty());
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

        if (ImGui::Button(show_parents_ ? ICON_FA_FOLDER_TREE " " ICON_FA_TOGGLE_ON : ICON_FA_FOLDER_TREE " " ICON_FA_TOGGLE_OFF))
            show_parents_ = !show_parents_;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(show_parents_ ? "Hide parents" : "Show parents");

        ImGui::SameLine();

        if (ImGui::Button(show_debug_properties_ ? ICON_FA_BUG " " ICON_FA_TOGGLE_ON : ICON_FA_BUG " " ICON_FA_TOGGLE_OFF))
            show_debug_properties_ = !show_debug_properties_;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(show_debug_properties_ ? "Hide debug properties" : "Show debug properties");

        ImGui::SameLine();

        if (ImGui::Button(tabbed_interface_ ? ICON_FA_TABLE_COLUMNS " " ICON_FA_TOGGLE_ON : ICON_FA_TABLE_COLUMNS " " ICON_FA_TOGGLE_OFF))
            tabbed_interface_ = !tabbed_interface_;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(tabbed_interface_ ? "Disable tabs for multiple components" : "Use tabs for multiple components");

        if (selected_cached_components_.empty())
        {
            ImGui::Text("No component selected");
        }
        else if (selected_cached_components_.size() == 1)
        {
            RenderCachedComponent(*selected_cached_components_[0]);
        }
        else if (tabbed_interface_)
        {
            if (ImGui::BeginTabBar("Selected components"))
            {
                int uid = 0;
                for (auto& cached_component : selected_cached_components_)
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
            for (auto& cached_component : selected_cached_components_)
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
