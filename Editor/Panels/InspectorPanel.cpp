#include "Panels/InspectorPanel.h"
#include "Core/FileSystem/FileSystem.h"

#include "EditorActions.h"
#include "EditorContext.h"
#include "EditorIcons.h"
#include "EditorLocale.h"
#include "ProjectSession.h"

#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <cstring>
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
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

Error TypeMismatch(const PropertyDescriptor& property)
{
    return Error{ErrorCode::InvalidState,
                 "Inspector reflected value does not match property type for '" + property.name +
                     "'."};
}

class ScalarDraftInputScope final
{
  public:
    ScalarDraftInputScope()
    {
        ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
    }
    ~ScalarDraftInputScope()
    {
        Release();
    }
    void Release()
    {
        if (m_Active)
            ImGui::PopItemFlag();
        m_Active = false;
    }

  private:
    bool m_Active = true;
};

template <std::size_t Size>
void CopyStringToBuffer(std::string_view value, std::array<char, Size>& buffer)
{
    buffer.fill('\0');

    const std::size_t count = std::min(value.size(), buffer.size() - 1);

    std::copy_n(value.data(), count, buffer.data());
}

} // namespace

InspectorPanel::InspectorPanel(EditorContext& context, EditorActions& actions) noexcept
    : m_Context(context), m_Actions(actions)
{
}

std::optional<Error> InspectorPanel::Draw()
{
    m_OwnsKeyboardInput = false;
    const std::string title = "      " + EditorLabel(m_Context.language, "Inspector");
    const bool visible = ImGui::Begin(title.c_str(), nullptr,
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
        ImGui::TextUnformatted(EditorText(m_Context.language, "No project open."));
        ImGui::End();
        return std::nullopt;
    }

    Scene& scene = m_Context.project->GetEditorScene();

    // Keep a failed draft reachable even if selection changed or the edited entity was deleted.
    const auto pendingError =
        m_NameDraft.GetError() ? m_NameDraft.GetError() : m_PropertyDraft.GetError();
    if (pendingError)
    {
        ImGui::TextWrapped("%s", pendingError->message.c_str());
        if (ImGui::Button(EditorLabel(m_Context.language, "Retry field draft").c_str()))
        {
            const auto retried = CommitPendingEdit();
            if (!retried)
                return ImGui::End(), std::optional<Error>{retried.GetError()};
        }
        ImGui::SameLine();
        if (ImGui::Button(EditorLabel(m_Context.language, "Discard field draft").c_str()))
            DiscardPendingEdit();
        ImGui::Separator();
    }

    if (!m_Context.selection.Validate(scene) || !m_Context.selection.GetSelectedUUID().has_value())
    {
        if (m_NameDraft.IsEdited() || m_PropertyDraft.IsEdited())
        {
            if (m_NameDraft.GetError() || m_PropertyDraft.GetError())
            {
                ImGui::End();
                return std::nullopt;
            }
            const auto pending = CommitPendingEdit();
            if (!pending)
                return ImGui::End(), std::optional<Error>{pending.GetError()};
        }
        ImGui::TextUnformatted(EditorText(m_Context.language, "No entity selected."));
        ImGui::End();
        return std::nullopt;
    }

    const UUID id = *m_Context.selection.GetSelectedUUID();
    if ((m_NameDraft.IsEdited() && m_NameDraft.GetEntity() != id) ||
        (m_PropertyDraft.IsEdited() && m_PropertyDraft.GetEntity() != id))
    {
        if (m_NameDraft.GetError() || m_PropertyDraft.GetError())
        {
            ImGui::End();
            return std::nullopt;
        }
        const auto pending = CommitPendingEdit();
        if (!pending)
            return ImGui::End(), std::optional<Error>{pending.GetError()};
    }
    const ECS::Entity entity = scene.FindEntity(id);

    const auto* identity = scene.GetComponent<EntityIdentityComponent>(entity);
    if (identity == nullptr)
    {
        m_Context.selection.Clear();
        ImGui::TextUnformatted("Selected entity is missing persistent identity.");
        ImGui::End();
        return std::nullopt;
    }

    auto model = BuildInspectorModel(scene, id, m_Context.project->GetReflectionRegistry());
    if (!model)
    {
        const Error error = model.GetError();
        ImGui::TextWrapped("Inspector unavailable: %s", error.message.c_str());
        ImGui::End();
        return error;
    }

    const bool readOnly = m_Context.project->IsAuthoringReadOnly();

    if (readOnly)
    {
        ImGui::TextDisabled("%s",
                            EditorText(m_Context.language, "Authoring is currently read-only."));
        ImGui::Separator();
    }

    SyncNameBuffer(id, identity->name.c_str());
    SyncPropertyBuffers(id);

    std::optional<Error> error;

    ImGui::BeginDisabled(readOnly);

    const auto nameIconPos = ImGui::GetCursorScreenPos();
    const float nameIconSize = ImGui::GetFrameHeight();
    ImGui::Dummy({nameIconSize, nameIconSize});
    DrawIcon(Icon::Entity, {nameIconPos.x + 2, nameIconPos.y + 2}, nameIconSize - 4);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    const bool nameWasActive = m_NameEditing;
    ImGui::BeginDisabled(m_PropertyDraft.GetError().has_value());
    const std::string nameInputId = "##Name" + std::to_string(m_InputGeneration);
    const bool renameCommitted =
        ImGui::InputText(nameInputId.c_str(), m_NameBuffer.data(), m_NameBuffer.size(),
                         ImGuiInputTextFlags_EnterReturnsTrue);

    m_NameEditing = ImGui::IsItemActive();
    m_OwnsKeyboardInput |= m_NameEditing || ImGui::IsItemDeactivated() || nameWasActive;
    if (ImGui::IsItemActivated() || (m_NameEditing && !m_NameDraft.MatchesName(id)))
    {
        auto begun = m_PropertyDraft.Commit(*m_Context.project, m_Actions);
        if (begun)
        {
            m_PropertyEdited = false;
            m_ActiveProperty.reset();
            begun = m_NameDraft.BeginName(*m_Context.project, id, identity->name);
        }
        if (!begun)
            error = begun.GetError();
    }
    m_NameEdited = m_NameEdited || ImGui::IsItemEdited();
    if (ImGui::IsItemEdited())
        m_NameDraft.SetValue(std::string{m_NameBuffer.data()});

    if ((nameWasActive || m_NameEditing) && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        m_NameDraft.Cancel();
        m_NameEdited = false;
        m_NameEditing = false;
        ++m_InputGeneration;
        CopyStringToBuffer(identity->name, m_NameBuffer);
    }
    else if (!readOnly && (renameCommitted || ImGui::IsItemDeactivatedAfterEdit()))
    {
        const auto renamed = m_NameDraft.Commit(*m_Context.project, m_Actions);
        if (!renamed)
        {
            error = renamed.GetError();
        }
        else
            m_NameEdited = false;
    }
    ImGui::EndDisabled();

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

    for (const InspectorComponentModel& componentModel : model.Value())
    {
        const ComponentDescriptor* component = componentModel.descriptor;
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
        if (m_Context.panelExpanded)
        {
            const auto saved = m_Context.panelExpanded->find(component->name);
            if (saved != m_Context.panelExpanded->end())
                ImGui::SetNextItemOpen(saved->second);
        }
        const bool open =
            IconHeader(ComponentIcon(component->name),
                       EditorLabel(m_Context.language, component->name.c_str()).c_str(),
                       ImGuiTreeNodeFlags_DefaultOpen);
        if (!open && ImGui::IsItemToggledOpen() && m_PropertyDraft.IsActive() &&
            m_ActiveComponent == component->id)
        {
            const auto settled = CommitPendingEdit();
            if (!settled)
                error = settled.GetError();
        }
        if (m_Context.panelExpanded)
            (*m_Context.panelExpanded)[component->name] = open;
        ImGui::PopStyleColor();
        const bool headerContext = ImGui::IsItemClicked(ImGuiMouseButton_Right);
        if (component->removable)
        {
            const auto max = ImGui::GetItemRectMax();
            const auto min = ImGui::GetItemRectMin();
            const auto next = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos({max.x - ImGui::GetFrameHeight(), min.y});
            const bool menuClicked = IconOnlyButton(
                Icon::Settings, EditorLabel(m_Context.language, "Component actions").c_str());
            ImGui::SetCursorScreenPos(next);
            if (headerContext || menuClicked)
                ImGui::OpenPopup("ComponentActions");
        }
        bool removedComponent = false;
        if (component->removable && ImGui::BeginPopup("ComponentActions"))
        {
            if (ImGui::MenuItem(EditorLabel(m_Context.language, "Remove Component").c_str()))
            {
                auto removed = CommitPendingEdit();
                if (removed)
                    removed = m_Actions.RemoveComponent(id, component->id);
                if (!removed)
                    error = removed.GetError();
                else
                    removedComponent = true;
            }
            ImGui::EndPopup();
        }

        if (open && !removedComponent)
        {
            for (const InspectorPropertyModel& property : componentModel.properties)
            {
                const auto propertyError = DrawProperty(id, component->id, property);
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
    if (IconButton(Icon::Add, EditorLabel(m_Context.language, "Add Component").c_str(),
                   ImVec2{-1, 0}))
        ImGui::OpenPopup("AddComponent");
    if (ImGui::BeginPopup("AddComponent"))
    {
        for (const auto& candidate : model.Value())
        {
            if (!candidate.present && candidate.descriptor && candidate.descriptor->removable &&
                ImGui::Selectable(
                    EditorLabel(m_Context.language, candidate.descriptor->name.c_str()).c_str()))
            {
                auto added = CommitPendingEdit();
                if (added)
                    added = m_Actions.AddComponent(id, candidate.descriptor->id);
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

std::optional<Error> InspectorPanel::DrawProperty(UUID entity, ComponentTypeId component,
                                                  const InspectorPropertyModel& propertyModel)
{
    const PropertyDescriptor* descriptor = propertyModel.descriptor;
    if (descriptor == nullptr)
    {
        return Error{ErrorCode::InvalidState, "Inspector property metadata is missing."};
    }

    const u64 key = descriptor->id.value;
    const std::string valueInputId = "##Value" + std::to_string(m_InputGeneration);

    ImGui::PushID(descriptor->name.c_str());
    const bool blockedByDraft =
        m_NameDraft.GetError().has_value() ||
        (m_PropertyDraft.GetError().has_value() &&
         !m_PropertyDraft.MatchesProperty(entity, component, descriptor->id));
    ImGui::BeginDisabled(!descriptor->editable || blockedByDraft);
    const float labelX = ImGui::GetCursorPosX();
    const float labelWidth = ImGui::GetContentRegionAvail().x * 0.34f;
    ImGui::AlignTextToFramePadding();
    const auto originalLabel = PropertyLabel(*descriptor);
    const std::string displayLabel = EditorText(m_Context.language, originalLabel.c_str());
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
        const auto* current = std::get_if<std::string>(&propertyModel.value);
        if (current == nullptr)
        {
            ImGui::EndDisabled();
            ImGui::PopID();
            return TypeMismatch(*descriptor);
        }

        auto& buffer = m_StringBuffers[key];

        if (m_ActiveProperty != key)
        {
            CopyStringToBuffer(*current, buffer);
        }

        if (descriptor->id == MakePropertyId("Text.content"))
            commit = ImGui::InputTextMultiline(valueInputId.c_str(), buffer.data(), buffer.size(),
                                               ImVec2(0, ImGui::GetTextLineHeight() * 5),
                                               ImGuiInputTextFlags_EnterReturnsTrue |
                                                   ImGuiInputTextFlags_CtrlEnterForNewLine);
        else
            commit = ImGui::InputText(valueInputId.c_str(), buffer.data(), buffer.size(),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
        changed = ImGui::IsItemEdited();

        desired = PropertyValue{std::string{buffer.data()}};
    }
    else if (descriptor->type == PropertyType::AssetReference)
    {
        const auto* reference = std::get_if<AssetReferenceValue>(&propertyModel.value);
        if (reference == nullptr)
        {
            ImGui::EndDisabled();
            ImGui::PopID();
            return TypeMismatch(*descriptor);
        }

        const auto& registry = m_Context.project->GetAssetRegistry();
        const auto* current = registry.Find(AssetHandle{reference->id});
        const std::string label =
            current ? FileSystem::PathToUtf8(current->relativePath.filename())
                    : EditorText(m_Context.language,
                                 reference->id.IsValid() ? "Missing asset" : "None");
        auto& search = m_AssetSearch[key];
        ImGui::SetNextItemWidth(std::max(30.0f, ImGui::GetContentRegionAvail().x -
                                                    ImGui::GetFrameHeight() -
                                                    ImGui::GetStyle().ItemSpacing.x));
        if (ImGui::BeginCombo("##Asset", ("      " + label).c_str()))
        {
            if (ImGui::IsWindowAppearing())
            {
                search.fill(0);
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::InputTextWithHint("##AssetSearch",
                                     EditorText(m_Context.language, "Search matching assets..."),
                                     search.data(), search.size());
            m_OwnsKeyboardInput |= ImGui::IsItemActive() || ImGui::IsItemDeactivated();
            if (ImGui::Selectable(EditorLabel(m_Context.language, "None").c_str(),
                                  !reference->id.IsValid()))
            {
                desired = AssetReferenceValue{};
                commit = true;
            }
            for (const auto& asset : registry.GetAssets())
            {
                if (!descriptor->referenceConstraint.empty() &&
                    AssetTypeName(asset.type) != descriptor->referenceConstraint)
                    continue;
                if (search[0] && FileSystem::PathToUtf8(asset.relativePath).find(search.data()) ==
                                     std::string::npos)
                    continue;
                ImGui::PushID(asset.handle.ToString().c_str());
                if (ImGui::Selectable(FileSystem::PathToUtf8(asset.relativePath).c_str(),
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
                              current ? FileSystem::PathToUtf8(current->relativePath).c_str()
                                      : EditorText(m_Context.language, "Select a registered asset"),
                              reference->id.ToString().c_str());
        if (ImGui::BeginDragDropTarget())
        {
            if (const auto* accepted = ImGui::AcceptDragDropPayload(EditorAssetPayloadType))
            {
                if (accepted->DataSize == sizeof(EditorAssetPayload))
                {
                    EditorAssetPayload payload;
                    std::memcpy(&payload, accepted->Data, sizeof(payload));
                    auto assigned = CommitPendingEdit();
                    if (assigned)
                        assigned = m_Actions.AssignAssetPayload(entity, component, descriptor->id,
                                                                payload);
                    if (!assigned)
                    {
                        ImGui::EndDragDropTarget();
                        ImGui::EndDisabled();
                        ImGui::PopID();
                        return assigned.GetError();
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(current == nullptr);
        if (IconOnlyButton(Icon::Folder, EditorLabel(m_Context.language, "Locate asset").c_str()))
            m_Context.locateAsset = reference->id;
        ImGui::EndDisabled();
    }
    else
    {
        auto [bufferIt, inserted] = m_PropertyBuffers.try_emplace(key, propertyModel.value);

        if (!inserted && m_ActiveProperty != key)
        {
            bufferIt->second = propertyModel.value;
        }

        PropertyValue& buffer = bufferIt->second;

        // ImGui 1.92.9 defers typed scalar values by default. Our backing values are UI drafts:
        // keep them current so a native close request can settle the last typed value.
        ScalarDraftInputScope scalarDraftInput;
        switch (descriptor->type)
        {
        case PropertyType::Bool:
        {
            auto* value =
                std::get_if<bool>(&buffer);
            if (value == nullptr)
            {
                scalarDraftInput.Release();
                ImGui::EndDisabled();
                ImGui::PopID();
                return TypeMismatch(*descriptor);
            }

            changed = ImGui::Checkbox(valueInputId.c_str(), value);
            break;
        }

        case PropertyType::Int32:
        {
            auto* value =
                std::get_if<i32>(&buffer);
            if (value == nullptr)
            {
                scalarDraftInput.Release();
                ImGui::EndDisabled();
                ImGui::PopID();
                return TypeMismatch(*descriptor);
            }

            int edited =
                static_cast<int>(*value);
            changed = ImGui::InputInt(valueInputId.c_str(), &edited);
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
                scalarDraftInput.Release();
                ImGui::EndDisabled();
                ImGui::PopID();
                return TypeMismatch(*descriptor);
            }

            changed = ImGui::DragFloat(valueInputId.c_str(), value, 0.05f);
            break;
        }

        case PropertyType::Vector2:
        {
            auto* value =
                std::get_if<Vector2>(&buffer);
            if (value == nullptr)
            {
                scalarDraftInput.Release();
                ImGui::EndDisabled();
                ImGui::PopID();
                return TypeMismatch(*descriptor);
            }

            float edited[]{
                value->x,
                value->y};

            changed = ImGui::DragFloat2(valueInputId.c_str(), edited, 0.1f);
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
                scalarDraftInput.Release();
                ImGui::EndDisabled();
                ImGui::PopID();
                return TypeMismatch(*descriptor);
            }

            float edited[]{
                value->r,
                value->g,
                value->b,
                value->a};

            changed = ImGui::ColorEdit4(valueInputId.c_str(), edited);
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

        if (descriptor->type == PropertyType::Bool && changed && !ImGui::IsItemActive())
        {
            commit = true;
        }

        desired = buffer;
    }

    ImGui::EndDisabled();

    const bool isAsset = descriptor->type == PropertyType::AssetReference;
    const bool itemActive = !isAsset && ImGui::IsItemActive();
    const bool itemDeactivated = !isAsset && ImGui::IsItemDeactivated();
    m_OwnsKeyboardInput |= itemActive || itemDeactivated;
    const bool escaped = !isAsset && m_ActiveProperty == key && (itemActive || itemDeactivated) &&
                         ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    if (escaped)
    {
        ++m_InputGeneration;
        m_PropertyDraft.Cancel();
        m_ActiveProperty.reset();
        m_PropertyEdited = false;
        m_PropertyBuffers.erase(key);
        m_StringBuffers.erase(key);
        ImGui::PopID();
        return std::nullopt;
    }
    if (!isAsset &&
        (ImGui::IsItemActivated() || changed ||
         (itemActive && !m_PropertyDraft.MatchesProperty(entity, component, descriptor->id))))
    {
        if (!m_PropertyDraft.MatchesProperty(entity, component, descriptor->id))
        {
            auto previous = m_PropertyDraft.Commit(*m_Context.project, m_Actions);
            if (!previous)
            {
                ImGui::PopID();
                return previous.GetError();
            }
            previous = m_NameDraft.Commit(*m_Context.project, m_Actions);
            if (!previous)
            {
                ImGui::PopID();
                return previous.GetError();
            }
            m_NameEdited = false;
            const auto begun = m_PropertyDraft.BeginProperty(*m_Context.project, entity, component,
                                                             descriptor->id, propertyModel.value);
            if (!begun)
            {
                ImGui::PopID();
                return begun.GetError();
            }
        }
        m_ActiveProperty = key;
        m_ActiveComponent = component;
        if (changed)
            m_PropertyDraft.SetValue(desired);
        m_PropertyEdited = m_PropertyDraft.IsEdited();
    }

    const bool multilineNewline = descriptor->id == MakePropertyId("Text.content") &&
                                  (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift);
    commit |= !isAsset &&
              (ImGui::IsItemDeactivatedAfterEdit() ||
               (itemActive && !multilineNewline && ImGui::IsKeyPressed(ImGuiKey_Enter, false)));
    if (descriptor->editable && !blockedByDraft && commit &&
        (isAsset || m_PropertyDraft.MatchesProperty(entity, component, descriptor->id)) &&
        !m_Context.project->IsAuthoringReadOnly())
    {
        auto updated =
            isAsset ? CommitPendingEdit() : m_PropertyDraft.Commit(*m_Context.project, m_Actions);
        if (updated && isAsset)
            updated = m_Actions.SetProperty(entity, component, descriptor->id, std::move(desired));

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
    else if (!isAsset && itemDeactivated && m_ActiveProperty == key && !m_PropertyDraft.IsEdited())
    {
        m_PropertyDraft.Cancel();
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
    if (m_Context.project == nullptr)
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Inspector requires an open project.");
    auto result = m_NameDraft.Commit(*m_Context.project, m_Actions);
    if (!result)
        return result;
    m_NameEdited = false;
    result = m_PropertyDraft.Commit(*m_Context.project, m_Actions);
    if (!result)
        return result;
    DiscardPendingEdit();
    return Result<void>::Success();
}

void InspectorPanel::DiscardPendingEdit()
{
    ++m_InputGeneration;
    m_NameDraft.Cancel();
    m_PropertyDraft.Cancel();
    m_NameEdited = false;
    m_NameEditing = false;
    m_PropertyEdited = false;
    m_ActiveProperty.reset();
    m_PropertyBuffers.clear();
    m_StringBuffers.clear();
}

} // namespace Janus::Editor
