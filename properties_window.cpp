#include "properties_window.h"
#include "opendaq_control.h"
#include "imgui.h"
#include "imgui_stdlib.h"


void RenderSelectedComponent(daq::ComponentPtr component, bool show_parents)
{
    if (!canCastTo<daq::IPropertyObject>(component))
        return;

    while (component.assigned())
    {
        ImGui::SeparatorText(component.getName().toStdString().c_str());
        RenderComponentProperties(component);
        if (!show_parents)
            break;
        component = component.getParent();
    }
}

void RenderComponentProperties(const daq::ComponentPtr& component)
{
    const daq::PropertyObjectPtr& property_holder = castTo<daq::IPropertyObject>(component);
    for (const auto& prop : property_holder.getVisibleProperties())
    {
        OpenDAQ::Property property(property_holder, prop);
        std::string prop_name = property.name;
        if (property.unit)
            prop_name += " [" + *property.unit + ']';
        if (property.read_only)
            ImGui::BeginDisabled();
        switch (property.type)
        {
            case daq::ctBool:
                {
                    bool value = property.value;
                    if (ImGui::Checkbox(property.name.c_str(), &value))
                        property_holder.setPropertyValue(property.name, value);
                    break;
                }
            case daq::ctInt:
                {
                    if (property.selection_values)
                    {
                        daq::ListPtr<daq::IString> vals = *property.selection_values;
                        std::string values = "";
                        for (int i = 0; i < vals.getCount(); i++)
                            values += vals.getItemAt(i).toStdString() + '\0';
                        int value = (int64_t)property.value;
                        if (ImGui::Combo(property.name.c_str(), &value, values.c_str(), vals.getCount()))
                            property_holder.setPropertyValue(property.name, value);
                    }
                    else
                    {
                        int value = (int64_t)property.value;
                        if (ImGui::InputInt(property.name.c_str(), &value))
                            property_holder.setPropertyValue(property.name, value);
                    }
                    break;
                }
            case daq::ctFloat:
                {
                    double value = property.value;
                    if (ImGui::InputDouble(property.name.c_str(), &value))
                        property_holder.setPropertyValue(property.name, value);
                    break;
                }
            case daq::ctString:
                {
                    std::string value = property.value;
                    ImGui::InputText(property.name.c_str(), &value);
                    break;
                }
            case daq::ctProc:
                {
                    if (ImGui::Button(property.name.c_str()))
                        property.value.asPtr<daq::IProcedure>().dispatch();
                    break;
                }
            default:
                {
                    std::string n = property.name + " " + std::to_string(property.type);
                    ImGui::Text("%s", n.c_str());
                }
        }
        if (property.read_only)
            ImGui::EndDisabled();
    }

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

        // self.attributes['Tags'] = { 'Value': node.tags.list, 'Locked': False, 'Attribute': 'tags'}
        //
        // if daq.ISignal.can_cast_from(node):
        //     signal = daq.ISignal.cast_from(node)
        //
        //     self.attributes['Public'] = {'Value': bool(
        //         signal.public), 'Locked': False, 'Attribute': 'public'}
        //     self.attributes['Domain Signal ID'] = {
        //         'Value': signal.domain_signal.global_id if signal.domain_signal else '', 'Locked': True,
        //         'Attribute': '.domain_signal'}
        //     self.attributes['Related Signals IDs'] = {'Value': os.linesep.join(
        //         [s.global_id for s in signal.related_signals]), 'Locked': True, 'Attribute': 'related_signals'}
        //     self.attributes['Streamed'] = {'Value': bool(
        //         signal.streamed), 'Locked': True, 'Attribute': 'streamed'}
        //     self.attributes['Last Value'] = {
        //         'Value': get_last_value_for_signal(signal), 'Locked': True, 'Attribute': 'last_value'}
        //
        // if daq.IInputPort.can_cast_from(node):
        //     input_port = daq.IInputPort.cast_from(node)
        //
        //     self.attributes['Signal ID'] = {
        //         'Value': input_port.signal.global_id if input_port.signal else '', 'Locked': True,
        //         'Attribute': 'signal'}
        //     self.attributes['Requires Signal'] = {'Value': bool(
        //         input_port.requires_signal), 'Locked': True, 'Attribute': 'requires_signal'}
        //
        // locked_attributes = node.locked_attributes
        //
        // self.attributes['Status'] = { 'Value': dict(node.status_container.statuses.items()) or None, 'Locked': True, 'Attribute': 'status'}
        //
        // for locked_attribute in locked_attributes:
        //     if locked_attribute not in self.attributes:
        //         continue
        //     self.attributes[locked_attribute]['Locked'] = True

    return;
}

void DrawPropertiesWindow(const std::vector<daq::ComponentPtr>& selected_components)
{
    ImGui::Begin("Property editor", NULL, ImGuiWindowFlags_AlwaysAutoResize);
    {
        static bool show_parents = false;
        ImGui::Checkbox("Show parents", &show_parents);

        for (const auto& component : selected_components)
        {
            // ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(255, 0, 0, 30));
            ImGui::BeginChild(component.getName().toStdString().c_str(), ImVec2(0, 0), ImGuiChildFlags_None | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);
            RenderSelectedComponent(component, show_parents);
            ImGui::EndChild();
            // ImGui::PopStyleColor();

            ImGui::SameLine();
        }
    }
    ImGui::End();
}
