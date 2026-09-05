#include "Panels/InspectorPanel.h"

#include "EditorActions.h"
#include "EditorContext.h"
#include "ProjectSession.h"

#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>

namespace Janus::Editor
{
namespace
{

Error TypeMismatch(
    const PropertyDescriptor& property)
{
    return Error{
        ErrorCode::InvalidState,
        "Inspector reflected value does not match property type for '"
            + property.name
            + "'."};
}

void CopyStringToBuffer(
    std::string_view value,
    std::array<char, 256>& buffer)
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
    const bool visible = ImGui::Begin(
        "Inspector",
        nullptr,
        ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoCollapse);

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

    const bool readOnly =
        m_Context.project->IsPlaying();

    if (readOnly)
    {
        ImGui::TextDisabled(
            "Authoring is read-only while Play Mode is active.");
        ImGui::Separator();
    }

    SyncNameBuffer(
        id,
        identity->name.c_str());
    SyncPropertyBuffers(id);

    std::optional<Error> error;

    ImGui::BeginDisabled(readOnly);

    const bool renameCommitted =
        ImGui::InputText(
            "Name",
            m_NameBuffer.data(),
            m_NameBuffer.size(),
            ImGuiInputTextFlags_EnterReturnsTrue);

    m_NameEditing = ImGui::IsItemActive();

    if (renameCommitted)
    {
        const auto renamed =
            m_Actions.RenameEntity(
                id,
                std::string{m_NameBuffer.data()});
        if (!renamed)
        {
            error = renamed.GetError();
        }
    }

    ImGui::Separator();

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
            if (component->removable
                && ImGui::Button(
                    ("Add " + component->name).c_str()))
            {
                const auto added =
                    m_Actions.AddComponent(
                        id,
                        component->id);
                if (!added)
                {
                    error = added.GetError();
                }
            }

            ImGui::PopID();

            if (error.has_value())
            {
                break;
            }

            continue;
        }

        const bool open =
            ImGui::CollapsingHeader(
                component->name.c_str(),
                ImGuiTreeNodeFlags_DefaultOpen);

        if (open)
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

            if (!error.has_value()
                && component->removable)
            {
                ImGui::Separator();
                if (ImGui::Button(
                        ("Remove " + component->name).c_str()))
                {
                    const auto removed =
                        m_Actions.RemoveComponent(
                            id,
                            component->id);
                    if (!removed)
                    {
                        error = removed.GetError();
                    }
                }
            }
        }

        ImGui::PopID();

        if (error.has_value())
        {
            break;
        }
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

        changed =
            ImGui::InputText(
                descriptor->name.c_str(),
                buffer.data(),
                buffer.size(),
                ImGuiInputTextFlags_EnterReturnsTrue);

        if (ImGui::IsItemActivated())
        {
            m_ActiveProperty = key;
        }

        commit =
            changed
            || ImGui::IsItemDeactivatedAfterEdit();

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

        const std::string value =
            reference->id.IsValid()
                ? reference->id.ToString()
                : std::string{"<none>"};

        ImGui::TextWrapped(
            "%s: %s",
            descriptor->name.c_str(),
            value.c_str());

        if (!descriptor->referenceConstraint.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled(
                "(%s)",
                descriptor->referenceConstraint.c_str());
        }
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

            changed =
                ImGui::Checkbox(
                    descriptor->name.c_str(),
                    value);
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
            changed =
                ImGui::InputInt(
                    descriptor->name.c_str(),
                    &edited);
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

            changed =
                ImGui::DragFloat(
                    descriptor->name.c_str(),
                    value,
                    0.05f);
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

            changed =
                ImGui::DragFloat2(
                    descriptor->name.c_str(),
                    edited,
                    0.1f);
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

            changed =
                ImGui::ColorEdit4(
                    descriptor->name.c_str(),
                    edited);
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
        }

        commit =
            ImGui::IsItemDeactivatedAfterEdit();

        if (descriptor->type == PropertyType::Bool
            && changed
            && !ImGui::IsItemActive())
        {
            commit = true;
        }

        desired = buffer;
    }

    ImGui::EndDisabled();

    if (descriptor->editable
        && commit)
    {
        const auto updated =
            m_Actions.SetProperty(
                entity,
                component,
                descriptor->id,
                std::move(desired));

        m_ActiveProperty.reset();
        m_PropertyBuffers.erase(key);
        m_StringBuffers.erase(key);

        if (!updated)
        {
            ImGui::PopID();
            return updated.GetError();
        }
    }

    ImGui::PopID();
    return std::nullopt;
}

void InspectorPanel::SyncNameBuffer(
    UUID id,
    const char* name)
{
    if (m_NameBufferEntity == id
        && m_NameEditing)
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
}

void InspectorPanel::SyncPropertyBuffers(UUID id)
{
    if (m_PropertyBufferEntity == id)
    {
        return;
    }

    m_PropertyBufferEntity = id;
    m_ActiveProperty.reset();
    m_PropertyBuffers.clear();
    m_StringBuffers.clear();
}

} // namespace Janus::Editor
