#include "properties_window.h"
#include "opendaq_control.h"
#include <string>
#include "imgui.h"
#include "imgui_stdlib.h"


void RenderSelectedComponent(daq::ComponentPtr component, bool show_parents, bool show_attributes)
{
    if (!canCastTo<daq::IPropertyObject>(component))
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextPadding, ImVec2(5.0f, 5.0f));
    while (component.assigned())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::SeparatorText(("[" + component.getName().toStdString() + "]").c_str());
        ImGui::PopStyleColor();
        RenderComponentPropertiesAndAttributes(component, show_attributes);
        if (!show_parents)
            break;
        component = component.getParent();
    }
    ImGui::PopStyleVar(2);
}

void RenderComponentPropertiesAndAttributes(const daq::ComponentPtr& component, bool show_attributes)
{
    const daq::PropertyObjectPtr& property_holder = castTo<daq::IPropertyObject>(component);
    for (const auto& property : property_holder.getVisibleProperties())
    {
        std::string prop_name = property.getName().toStdString();
        std::string prop_name_for_display = property.getName().toStdString();
        if (property.getUnit().assigned() && property.getUnit().getSymbol().assigned())
            prop_name_for_display += " [" + static_cast<std::string>(property.getUnit().getSymbol().toString()) + ']';

        if (property.getReadOnly())
            ImGui::BeginDisabled();
        // if (property.getMinValue() != nullptr) min = static_cast<double>(property.getMinValue());
        // if (property.getMaxValue() != nullptr) max = static_cast<double>(property.getMaxValue());
        // suggested_values = property.getSuggestedValues();
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
                    if (daq::ListPtr<daq::IString> selection_values = property.getSelectionValues(); selection_values.assigned())
                    {
                        std::string values = "";
                        for (int i = 0; i < selection_values.getCount(); i++)
                            values += selection_values.getItemAt(i).toStdString() + '\0';
                        int value = (int64_t)property_holder.getPropertyValue(prop_name);
                        if (ImGui::Combo(prop_name_for_display.c_str(), &value, values.c_str(), selection_values.getCount()))
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
                    ImGui::InputText(prop_name_for_display.c_str(), &value);
                    break;
                }
            case daq::ctProc:
                {
                    if (ImGui::Button(prop_name_for_display.c_str()))
                        property_holder.getPropertyValue(prop_name).asPtr<daq::IProcedure>().dispatch();
                    break;
                }
            default:
                {
                    std::string n = prop_name_for_display + " " + std::to_string(property.getValueType());
                    ImGui::Text("%s", n.c_str());
                }
        }
        if (property.getReadOnly())
            ImGui::EndDisabled();
    }

    if (!show_attributes)
        return;

    if (!ImGui::CollapsingHeader(("Attributes##" + component.getName().toStdString()).c_str()))
        return;

    {
        std::string value = component.getName();
        if (ImGui::InputText("Name", &value))
            component.setName(value);
    }
    {
        std::string value = component.getDescription();
        if (ImGui::InputText("Description", &value))
            component.setDescription(value);
    }
    {
        bool value = component.getActive();
        if (ImGui::Checkbox("Active", &value))
            component.setActive(value);
    }
    {
        bool value = component.getVisible();
        if (ImGui::Checkbox("Visible", &value))
            component.setVisible(value);
    }
    {
        std::string value = component.getLocalId();
        ImGui::BeginDisabled();
        ImGui::InputText("Local ID", &value);
        ImGui::EndDisabled();
    }
    {
        std::string value = component.getGlobalId();
        ImGui::BeginDisabled();
        ImGui::InputText("Global ID", &value);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone | ImGuiHoveredFlags_AllowWhenDisabled) && ImGui::BeginTooltip())
        {
            ImGui::Text("%s", value.c_str());
            ImGui::EndTooltip();
        }
    }
}

void DrawPropertiesWindow(const std::vector<daq::ComponentPtr>& selected_components)
{
    ImGui::Begin("Property editor", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_MenuBar);
    {
        static bool show_parents = false;
        static bool tabbed_interface = false;
        static bool show_attributes = false;
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Settings"))
            {
                ImGui::Checkbox("Show parents", &show_parents);
                ImGui::Checkbox("Show attributes", &show_attributes);
                ImGui::Checkbox("Use tabs for multiple selected components", &tabbed_interface);

                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (selected_components.empty())
        {
            ImGui::Text("No component selected");
        }
        else if (selected_components.size() == 1)
        {
            daq::ComponentPtr component = selected_components[0];
            if (component == nullptr || !component.assigned())
                return;

            RenderSelectedComponent(component, show_parents, show_attributes);
        }
        else if (tabbed_interface)
        {
            if (ImGui::BeginTabBar("Selected components"))
            {
                for (const auto& component : selected_components)
                {
                    if (component == nullptr || !component.assigned())
                        continue;

                    if (ImGui::BeginTabItem(component.getName().toStdString().c_str()))
                    {
                        RenderSelectedComponent(component, show_parents, show_attributes);
                        ImGui::EndTabItem();
                    }
                }
                ImGui::EndTabBar();
            }
        }
        else
        {
            for (const auto& component : selected_components)
            {
                if (component == nullptr || !component.assigned())
                    continue;

                ImGui::BeginChild(component.getName().toStdString().c_str(), ImVec2(0, 0), ImGuiChildFlags_None | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);
                RenderSelectedComponent(component, show_parents, show_attributes);
                ImGui::EndChild();

                ImGui::SameLine();
            }
        }
    }
    ImGui::End();
}
