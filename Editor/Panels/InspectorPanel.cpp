#include "Panels/InspectorPanel.h"

#include "EditorActions.h"
#include "EditorContext.h"
#include "EditorIcons.h"
#include "ProjectSession.h"

#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <cctype>
#include <cstdio>
#include <string>
#include <utility>

namespace Janus::Editor
{
namespace
{

std::string PropertyLabel(const PropertyDescriptor& property)
{
    std::string label;
    for (const unsigned char c : property.name)
    {
        if (!label.empty() && std::isupper(c))
            label += ' ';
        label += label.empty() ? static_cast<char>(std::toupper(c)) : static_cast<char>(c);
    }
    if (property.id == MakePropertyId("Transform.rotation"))
        label += " (rad)";
    return label;
}

Error TypeMismatch(
    const PropertyDescriptor& property)
{
    return Error{
        ErrorCode::InvalidState,
        "Inspector reflected value does not match property type for '"
            + property.name
            + "'."};
}

template <std::size_t Size>
void CopyStringToBuffer(std::string_view value, std::array<char, Size>& buffer)
{
    buffer.fill('\0');

    const std::size_t count =
        std::min(
            value.size(),
            buffer.size() - 1);

    std::copy_n(
        value.data(),
        count,
        buffer.data());
}

} // namespace

InspectorPanel::InspectorPanel(
    EditorContext& context,
    EditorActions& actions) noexcept
    : m_Context(context),
      m_Actions(actions)
{
}

std::optional<Error> InspectorPanel::Draw()
{
    const bool visible = ImGui::Begin("      Inspector###Inspector", nullptr,
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                          ImGuiWindowFlags_NoCollapse);

    DrawTitleIcon(Icon::Inspector);

    if (!visible)
    {
        ImGui::End();
        return std::nullopt;
    }

    if (m_Context.project == nullptr)
    {
        ImGui::TextUnformatted("No project open.");
        ImGui::End();
        return std::nullopt;
    }

    Scene& scene =
        m_Context.project->GetEditorScene();

    if (!m_Context.selection.Validate(scene)
        || !m_Context.selection.GetSelectedUUID().has_value())
    {
        ImGui::TextUnformatted("No entity selected.");
        ImGui::End();
        return std::nullopt;
    }

    const UUID id =
        *m_Context.selection.GetSelectedUUID();
    const ECS::Entity entity =
        scene.FindEntity(id);

    const auto* identity =
        scene.GetComponent<EntityIdentityComponent>(
            entity);
    if (identity == nullptr)
    {
        m_Context.selection.Clear();
        ImGui::TextUnformatted(
            "Selected entity is missing persistent identity.");
        ImGui::End();
        return std::nullopt;
    }

    auto model = BuildInspectorModel(
        scene,
        id,
        m_Context.project->GetReflectionRegistry());
    if (!model)
    {
        const Error error = model.GetError();
        ImGui::TextWrapped(
            "Inspector unavailable: %s",
            error.message.c_str());
        ImGui::End();
        return error;
    }

    const bool readOnly = m_Context.project->IsAuthoringReadOnly();

    if (readOnly)
    {
        ImGui::TextDisabled("Authoring is currently read-only.");
        ImGui::Separator();
    }

    SyncNameBuffer(
        id,
        identity->name.c_str());
    SyncPropertyBuffers(id);

    std::optional<Error> error;

    ImGui::BeginDisabled(readOnly);

    const auto nameIconPos = ImGui::GetCursorScreenPos();
    const float nameIconSize = ImGui::GetFrameHeight();
    ImGui::Dummy({nameIconSize, nameIconSize});
    DrawIcon(Icon::Entity, {nameIconPos.x + 2, nameIconPos.y + 2}, nameIconSize - 4);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    const bool renameCommitted = ImGui::InputText(
        "##Name", m_NameBuffer.data(), m_NameBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue);

    m_NameEditing = ImGui::IsItemActive();
    m_NameEdited = m_NameEdited || ImGui::IsItemEdited();

    if (!readOnly && (renameCommitted || ImGui::IsItemDeactivatedAfterEdit()))
    {
        const auto renamed = m_Actions.RenameEntity(id, std::string{m_NameBuffer.data()});
        if (!renamed)
        {
            error = renamed.GetError();
        }
        else
            m_NameEdited = false;
    }

    ImGui::Separator();

    std::stable_sort(model.Value().begin(), model.Value().end(),
                     [](const auto& left, const auto& right)
                     {
                         const auto rank = [](const auto* descriptor)
                         {
                             if (descriptor && descriptor->name == "Transform")
                                 return 0;
                             if (descriptor && descriptor->name == "SpriteRenderer")
                                 return 1;
                             return 2;
                         };
                         return rank(left.descriptor) < rank(right.descriptor);
                     });

    for (const InspectorComponentModel& componentModel :
         model.Value())
    {
        const ComponentDescriptor* component =
            componentModel.descriptor;
        if (component == nullptr)
        {
            continue;
        }

        ImGui::PushID(component->name.c_str());

        if (!componentModel.present)
        {
            ImGui::PopID();
            continue;
        }

        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4{0.16f, 0.19f, 0.23f, 1.0f});
        ImGui::SetNextItemAllowOverlap();
        const bool open = IconHeader(ComponentIcon(component->name), component->name.c_str(),
                                     ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::PopStyleColor();
        const bool headerContext = ImGui::IsItemClicked(ImGuiMouseButton_Right);
        if (component->removable)
        {
            const auto max = ImGui::GetItemRectMax();
            const auto min = ImGui::GetItemRectMin();
            const auto next = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos({max.x - ImGui::GetFrameHeight(), min.y});
            const bool menuClicked = IconOnlyButton(Icon::Settings, "Component actions");
            ImGui::SetCursorScreenPos(next);
            if (headerContext || menuClicked)
                ImGui::OpenPopup("ComponentActions");
        }
        bool removedComponent = false;
        if (component->removable && ImGui::BeginPopup("ComponentActions"))
        {
            if (ImGui::MenuItem("Remove Component"))
            {
                const auto removed = m_Actions.RemoveComponent(id, component->id);
                if (!removed)
                    error = removed.GetError();
                else
                    removedComponent = true;
            }
            ImGui::EndPopup();
        }

        if (open && !removedComponent)
        {
            for (const InspectorPropertyModel& property :
                 componentModel.properties)
            {
                const auto propertyError =
                    DrawProperty(
                        id,
                        component->id,
                        property);
                if (propertyError.has_value())
                {
                    error = propertyError;
                    break;
                }
            }

        }

        ImGui::PopID();

        if (error.has_value())
        {
            break;
        }
    }

    ImGui::Spacing();
    if (IconButton(Icon::Add, "Add Component", ImVec2{-1, 0}))
        ImGui::OpenPopup("AddComponent");
    if (ImGui::BeginPopup("AddComponent"))
    {
        for (const auto& candidate : model.Value())
        {
            if (!candidate.present && candidate.descriptor && candidate.descriptor->removable &&
                ImGui::Selectable(candidate.descriptor->name.c_str()))
            {
                const auto added = m_Actions.AddComponent(id, candidate.descriptor->id);
                if (!added)
                    error = added.GetError();
            }
        }
        ImGui::EndPopup();
    }

    ImGui::EndDisabled();

    ImGui::End();
    return error;
}

std::optional<Error> InspectorPanel::DrawProperty(
    UUID entity,
    ComponentTypeId component,
    const InspectorPropertyModel& propertyModel)
{
    const PropertyDescriptor* descriptor =
        propertyModel.descriptor;
    if (descriptor == nullptr)
    {
        return Error{
            ErrorCode::InvalidState,
            "Inspector property metadata is missing."};
    }

    const u64 key = descriptor->id.value;

    ImGui::PushID(descriptor->name.c_str());
    ImGui::BeginDisabled(!descriptor->editable);
    const float labelX = ImGui::GetCursorPosX();
    const float labelWidth = ImGui::GetContentRegionAvail().x * 0.34f;
    ImGui::AlignTextToFramePadding();
    const auto displayLabel = PropertyLabel(*descriptor);
    const auto labelPos = ImGui::GetCursorScreenPos();
    ImGui::Dummy({labelWidth - ImGui::GetStyle().ItemSpacing.x, ImGui::GetFrameHeight()});
    DrawEllipsizedText(ImGui::GetWindowDrawList(),
                       {labelPos.x, labelPos.y + ImGui::GetStyle().FramePadding.y},
                       labelWidth - ImGui::GetStyle().ItemSpacing.x,
                       ImGui::GetColorU32(ImGuiCol_Text), displayLabel);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s\nField: %s", displayLabel.c_str(), descriptor->name.c_str());
    ImGui::SameLine(labelX + labelWidth);
    ImGui::SetNextItemWidth(-1);

    bool changed = false;
    bool commit = false;
    PropertyValue desired = propertyModel.value;

    if (descriptor->type == PropertyType::String)
    {
        const auto* current =
            std::get_if<std::string>(
                &propertyModel.value);
        if (current == nullptr)
        {
            ImGui::EndDisabled();
            ImGui::PopID();
            return TypeMismatch(*descriptor);
        }

        auto& buffer =
            m_StringBuffers[key];

        if (m_ActiveProperty != key)
        {
            CopyStringToBuffer(
                *current,
                buffer);
        }

        if (descriptor->id == MakePropertyId("Text.content"))
            changed = ImGui::InputTextMultiline("##Value", buffer.data(), buffer.size(),
                                                ImVec2(0, ImGui::GetTextLineHeight() * 5));
        else
            changed = ImGui::InputText("##Value", buffer.data(), buffer.size());

        if (ImGui::IsItemActivated())
        {
            m_ActiveProperty = key;
            m_ActiveComponent = component;
        }

        commit =
            ImGui::IsItemDeactivatedAfterEdit();

        if (ImGui::IsItemDeactivated() && !commit && m_ActiveProperty == key && !m_PropertyEdited)
        {
            m_ActiveProperty.reset();
            m_StringBuffers.erase(key);
        }

        desired =
            PropertyValue{
                std::string{buffer.data()}};
    }
    else if (descriptor->type
             == PropertyType::AssetReference)
    {
        const auto* reference =
            std::get_if<AssetReferenceValue>(
                &propertyModel.value);
        if (reference == nullptr)
        {
            ImGui::EndDisabled();
            ImGui::PopID();
            return TypeMismatch(*descriptor);
        }

        const auto& registry = m_Context.project->GetAssetRegistry();
        const auto* current = registry.Find(AssetHandle{reference->id});
        const std::string label = current ? current->relativePath.filename().string()
                                          : (reference->id.IsValid() ? "Missing asset" : "None");
        if (ImGui::BeginCombo("##Asset", ("      " + label).c_str()))
        {
            if (ImGui::Selectable("None", !reference->id.IsValid()))
            {
                desired = AssetReferenceValue{};
                commit = true;
            }
            for (const auto& asset : registry.GetAssets())
            {
                if (!descriptor->referenceConstraint.empty() &&
                    AssetTypeName(asset.type) != descriptor->referenceConstraint)
                    continue;
                ImGui::PushID(asset.handle.ToString().c_str());
                if (ImGui::Selectable(asset.relativePath.generic_string().c_str(),
                                      asset.handle.id == reference->id))
                {
                    desired = AssetReferenceValue{asset.handle.id};
                    commit = true;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        DrawItemIcon(current ? AssetIcon(AssetTypeName(current->type)) : Icon::Folder,
                     ImGui::GetStyle().FramePadding.x);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\nUUID: %s",
                              current ? current->relativePath.generic_string().c_str()
                                      : "Select a registered asset",
                              reference->id.ToString().c_str());
    }
    else
    {
        auto [bufferIt, inserted] =
            m_PropertyBuffers.try_emplace(
                key,
                propertyModel.value);

        if (!inserted
            && m_ActiveProperty != key)
        {
            bufferIt->second =
                propertyModel.value;
        }

        PropertyValue& buffer =
            bufferIt->second;

        switch (descriptor->type)
        {
        case PropertyType::Bool:
        {
            auto* value =
                std::get_if<bool>(&buffer);
            if (value == nullptr)
            {
                ImGui::EndDisabled();
                ImGui::PopID();
                return TypeMismatch(*descriptor);
            }

            changed = ImGui::Checkbox("##Value", value);
            break;
        }

        case PropertyType::Int32:
        {
            auto* value =
                std::get_if<i32>(&buffer);
            if (value == nullptr)
            {
                ImGui::EndDisabled();
                ImGui::PopID();
                return TypeMismatch(*descriptor);
            }

            int edited =
                static_cast<int>(*value);
            changed = ImGui::InputInt("##Value", &edited);
            if (changed)
            {
                *value =
                    static_cast<i32>(edited);
            }
            break;
        }

        case PropertyType::Float32:
        {
            auto* value =
                std::get_if<f32>(&buffer);
            if (value == nullptr)
            {
                ImGui::EndDisabled();
                ImGui::PopID();
                return TypeMismatch(*descriptor);
            }

            changed = ImGui::DragFloat("##Value", value, 0.05f);
            break;
        }

        case PropertyType::Vector2:
        {
            auto* value =
                std::get_if<Vector2>(&buffer);
            if (value == nullptr)
            {
                ImGui::EndDisabled();
                ImGui::PopID();
                return TypeMismatch(*descriptor);
            }

            float edited[]{
                value->x,
                value->y};

            changed = ImGui::DragFloat2("##Value", edited, 0.1f);
            // Axis labels are a display overlay; the existing compound field keeps its commit
            // semantics.
            const auto min = ImGui::GetItemRectMin();
            const auto max = ImGui::GetItemRectMax();
            const float gap = ImGui::GetStyle().ItemInnerSpacing.x;
            const float half = (max.x - min.x - gap) * .5f;
            if (half > ImGui::GetFontSize() * 4 && !ImGui::IsItemActive())
            {
                auto* draw = ImGui::GetWindowDrawList();
                const float y = min.y + ImGui::GetStyle().FramePadding.y;
                draw->AddText({min.x + ImGui::GetStyle().FramePadding.x, y},
                              IM_COL32(232, 125, 120, 255), "X");
                draw->AddText({min.x + half + gap + ImGui::GetStyle().FramePadding.x, y},
                              IM_COL32(130, 205, 152, 255), "Y");
            }
            if (changed)
            {
                *value =
                    Vector2{
                        edited[0],
                        edited[1]};
            }
            break;
        }

        case PropertyType::Color:
        {
            auto* value =
                std::get_if<ColorValue>(&buffer);
            if (value == nullptr)
            {
                ImGui::EndDisabled();
                ImGui::PopID();
                return TypeMismatch(*descriptor);
            }

            float edited[]{
                value->r,
                value->g,
                value->b,
                value->a};

            changed = ImGui::ColorEdit4("##Value", edited);
            if (changed)
            {
                *value =
                    ColorValue{
                        edited[0],
                        edited[1],
                        edited[2],
                        edited[3]};
            }
            break;
        }

        case PropertyType::Unknown:
        case PropertyType::String:
        case PropertyType::AssetReference:
        default:
            ImGui::TextDisabled(
                "%s: <unsupported>",
                descriptor->name.c_str());
            break;
        }

        if (ImGui::IsItemActivated())
        {
            m_ActiveProperty = key;
            m_ActiveComponent = component;
        }

        commit =
            ImGui::IsItemDeactivatedAfterEdit();

        if (descriptor->type == PropertyType::Bool
            && changed
            && !ImGui::IsItemActive())
        {
            commit = true;
        }

        if (ImGui::IsItemDeactivated() && !commit && m_ActiveProperty == key && !m_PropertyEdited)
        {
            m_ActiveProperty.reset();
            m_PropertyBuffers.erase(key);
        }

        desired = buffer;
    }

    ImGui::EndDisabled();

    if (changed)
    {
        m_ActiveProperty = key;
        m_ActiveComponent = component;
        m_PropertyEdited = true;
    }

    if (descriptor->editable && commit && !m_Context.project->IsAuthoringReadOnly())
    {
        const auto updated =
            m_Actions.SetProperty(
                entity,
                component,
                descriptor->id,
                std::move(desired));

        if (!updated)
        {
            ImGui::PopID();
            return updated.GetError();
        }
        m_ActiveProperty.reset();
        m_PropertyEdited = false;
        m_PropertyBuffers.erase(key);
        m_StringBuffers.erase(key);
    }

    ImGui::PopID();
    return std::nullopt;
}

void InspectorPanel::SyncNameBuffer(
    UUID id,
    const char* name)
{
    if (m_NameBufferEntity == id && (m_NameEditing || m_NameEdited))
    {
        return;
    }

    m_NameBuffer.fill('\0');

    if (name != nullptr)
    {
        CopyStringToBuffer(
            name,
            m_NameBuffer);
    }

    m_NameBufferEntity = id;
    m_NameEdited = false;
}

void InspectorPanel::SyncPropertyBuffers(UUID id)
{
    if (m_PropertyBufferEntity == id)
    {
        return;
    }

    m_PropertyBufferEntity = id;
    m_ActiveProperty.reset();
    m_PropertyEdited = false;
    m_PropertyBuffers.clear();
    m_StringBuffers.clear();
}

Result<void> InspectorPanel::CommitPendingEdit()
{
    if (m_NameEdited)
    {
        auto result = m_Actions.RenameEntity(m_NameBufferEntity, m_NameBuffer.data());
        if (!result)
            return result;
        m_NameEdited = false;
    }
    if (m_PropertyEdited && m_ActiveProperty)
    {
        const auto key = *m_ActiveProperty;
        PropertyValue value;
        if (auto text = m_StringBuffers.find(key); text != m_StringBuffers.end())
            value = std::string{text->second.data()};
        else if (auto property = m_PropertyBuffers.find(key); property != m_PropertyBuffers.end())
            value = property->second;
        else
            return Result<void>::Failure(ErrorCode::InvalidState,
                                         "Pending Inspector value is unavailable.");
        auto result = m_Actions.SetProperty(m_PropertyBufferEntity, m_ActiveComponent,
                                            PropertyId{key}, std::move(value));
        if (!result)
            return result;
        DiscardPendingEdit();
    }
    return Result<void>::Success();
}

void InspectorPanel::DiscardPendingEdit()
{
    m_NameEdited = false;
    m_NameEditing = false;
    m_PropertyEdited = false;
    m_ActiveProperty.reset();
    m_PropertyBuffers.clear();
    m_StringBuffers.clear();
}

} // namespace Janus::Editor
