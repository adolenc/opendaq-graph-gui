#include "properties_window.h"
#include "property_cache.h"
#include <string>
#include "imgui.h"
#include "imgui_stdlib.h"


void PropertiesWindow::RenderCachedProperty(CachedProperty& cached_prop)
{
    if (!(show_detail_properties_ || !cached_prop.is_detail_))
        return;

    if (cached_prop.is_read_only_)
        ImGui::BeginDisabled();

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
                if (cached_prop.is_read_only_ && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone | ImGuiHoveredFlags_AllowWhenDisabled) && ImGui::BeginTooltip())
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

    if (cached_prop.is_read_only_)
        ImGui::EndDisabled();
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
        if (ImGui::BeginTabBar("CachedSignalDescriptors"))
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
        cached_component.Refresh();
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
                ImGui::Checkbox("Show debug properties", &show_detail_properties_);
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
