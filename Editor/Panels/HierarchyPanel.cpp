#include "Panels/HierarchyPanel.h"

#include "EditorActions.h"
#include "EditorContext.h"
#include "EditorIcons.h"
#include "ProjectSession.h"
#include "UI/UIComponents.h"

#include "Scene/Components.h"
#include "Scene/Hierarchy.h"
#include "Scene/Scene.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <string>

namespace Janus::Editor
{

HierarchyPanel::HierarchyPanel(
    EditorContext& context,
    EditorActions& actions) noexcept
    : m_Context(context),
      m_Actions(actions)
{
}

std::optional<Error> HierarchyPanel::Draw()
{
    const bool visible = ImGui::Begin("      Hierarchy###Hierarchy", nullptr,
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                          ImGuiWindowFlags_NoCollapse);

    DrawTitleIcon(Icon::Hierarchy);

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
    m_Context.selection.Validate(scene);

    const bool playing = m_Context.project->IsAuthoringReadOnly();

    ImGui::BeginDisabled(playing);

    if (IconButton(Icon::Add, "Entity"))
    {
        const auto created =
            m_Actions.CreateEntity("Entity");
        if (!created)
        {
            const Error error = created.GetError();
            ImGui::EndDisabled();
            ImGui::End();
            return error;
        }
    }

    ImGui::SameLine();

    const bool canDelete =
        m_Context.selection.HasSelection();

    ImGui::BeginDisabled(!canDelete);
    if (IconButton(Icon::Delete, "Delete") && m_Context.selection.GetSelectedUUID().has_value())
    {
        const auto deleted =
            m_Actions.DeleteEntity(
                *m_Context.selection.GetSelectedUUID());
        if (!deleted)
        {
            const Error error = deleted.GetError();
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            ImGui::End();
            return error;
        }
    }
    ImGui::EndDisabled();

    ImGui::EndDisabled();

    ImGui::SameLine();
    if (IconOnlyButton(Icon::Settings, "Organize / Prefab"))
        ImGui::OpenPopup("OrganizePrefab");

    if (playing)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Read-only");
    }

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##Search", "Search entities...", m_Search.data(), m_Search.size());
    std::optional<Error> reparentError;
    ImGui::SetNextWindowSizeConstraints({280 * ImGui::GetStyle().FontScaleDpi, 0},
                                        {FLT_MAX, FLT_MAX});
    const bool organize = ImGui::BeginPopup("OrganizePrefab");
    if (organize)
    {
        IconText(Icon::Hierarchy, "Organize / Prefab");
        ImGui::Separator();
        if (!m_Context.selection.HasSelection())
            ImGui::TextDisabled("Select an entity first.");
    }
    if (auto selected = m_Context.selection.GetSelectedUUID(); organize && selected.has_value())
    {
        const auto entity = scene.FindEntity(*selected);
        const auto& hierarchy = *scene.GetComponent<HierarchyComponent>(entity);
        const auto parent = hierarchy.parent;
        const auto* parentIdentity = scene.GetComponent<EntityIdentityComponent>(parent);
        ImGui::BeginDisabled(playing);
        if (ImGui::BeginCombo("Parent", parentIdentity ? parentIdentity->name.c_str() : "<Root>"))
        {
            if (ImGui::Selectable("<Root>", !parent.IsValid()))
            {
                auto moved = m_Actions.ReparentEntity(*selected, {});
                if (!moved)
                    reparentError = moved.GetError();
            }
            for (auto candidate : scene.GetEntities())
            {
                // Cycles are rejected by Engine as well as hidden from the selector.
                auto ancestor = candidate;
                while (ancestor.IsValid() && ancestor != entity)
                    ancestor = scene.GetComponent<HierarchyComponent>(ancestor)->parent;
                if (ancestor == entity)
                    continue;
                const auto* identity = scene.GetComponent<EntityIdentityComponent>(candidate);
                ImGui::PushID(identity->id.ToString().c_str());
                if (ImGui::Selectable(identity->name.c_str(), parent == candidate))
                {
                    auto moved = m_Actions.ReparentEntity(*selected, identity->id);
                    if (!moved)
                        reparentError = moved.GetError();
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        if (parentIdentity)
        {
            usize index = 0;
            auto sibling = scene.GetComponent<HierarchyComponent>(parent)->firstChild;
            while (sibling.IsValid() && sibling != entity)
            {
                sibling = scene.GetComponent<HierarchyComponent>(sibling)->nextSibling;
                ++index;
            }
            ImGui::BeginDisabled(index == 0);
            if (ImGui::Button("Earlier"))
            {
                auto moved = m_Actions.ReparentEntity(*selected, parentIdentity->id, index - 1);
                if (!moved)
                    reparentError = moved.GetError();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(
                !scene.GetComponent<HierarchyComponent>(entity)->nextSibling.IsValid());
            if (ImGui::Button("Later"))
            {
                auto moved = m_Actions.ReparentEntity(*selected, parentIdentity->id, index + 1);
                if (!moved)
                    reparentError = moved.GetError();
            }
            ImGui::EndDisabled();
        }
        ImGui::EndDisabled();
    }

    if (auto selected = m_Context.selection.GetSelectedUUID(); organize && selected.has_value())
    {
        ImGui::BeginDisabled(m_Context.project->IsAuthoringReadOnly());
        if (ImGui::Button("Duplicate (Ctrl+D)"))
        {
            auto duplicated = m_Actions.DuplicateEntity(*selected);
            if (!duplicated)
                reparentError = duplicated.GetError();
            else
                ImGui::CloseCurrentPopup();
        }
        if (IconButton(Icon::Prefab, "Export Prefab"))
        {
            m_ExportEntity = *selected;
            const auto& name =
                scene.GetComponent<EntityIdentityComponent>(scene.FindEntity(*selected))->name;
            m_PrefabName.fill(0);
            std::copy_n(name.data(), std::min(name.size(), m_PrefabName.size() - 1),
                        m_PrefabName.data());
            m_ExportError.reset();
            m_OpenExport = true;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Save this subtree as a new asset. Select the Prefab in Assets to instantiate it.");
        ImGui::EndDisabled();
    }

    if (organize)
        ImGui::EndPopup();
    if (m_OpenExport)
    {
        ImGui::OpenPopup("Export Prefab");
        m_OpenExport = false;
    }
    if (ImGui::BeginPopupModal("Export Prefab", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::SetNextItemWidth(360 * ImGui::GetStyle().FontScaleDpi);
        if (ImGui::InputText("Name", m_PrefabName.data(), m_PrefabName.size()))
            m_ExportError.reset();
        const std::string name(m_PrefabName.data());
        auto valid = ProjectSession::ValidatePrefabName(name);
        ImGui::TextUnformatted("Destination (UUID assigned on export):");
        ImGui::TextWrapped("Prefabs/%s-<UUID>.prefab", name.c_str());
        if (!valid)
            ImGui::TextWrapped("%s", valid.GetError().message.c_str());
        if (m_ExportError)
            ImGui::TextWrapped("%s", m_ExportError->message.c_str());
        ImGui::BeginDisabled(!valid || m_Context.project->IsAuthoringReadOnly());
        if (ImGui::Button("Export"))
        {
            auto exported = m_Actions.ExportPrefab(m_ExportEntity, name);
            if (!exported)
                m_ExportError = exported.GetError();
            else
                ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::Separator();

    ImGui::Text("%s%s", scene.GetMetadata().name.c_str(), m_Context.project->IsDirty() ? " *" : "");
    for (const ECS::Entity entity : scene.GetEntities())
    {
        if (m_Search[0] != '\0')
        {
            const auto* identity = scene.GetComponent<EntityIdentityComponent>(entity);
            if (identity && identity->name.find(m_Search.data()) != std::string::npos &&
                ImGui::Selectable((identity->name + "##" + identity->id.ToString()).c_str(),
                                  m_Context.selection.GetSelectedUUID() == identity->id))
                m_Context.selection.Select(identity->id);
            continue;
        }
        const auto* hierarchy =
            scene.GetComponent<HierarchyComponent>(entity);

        if (hierarchy != nullptr
            && !hierarchy->parent.IsValid())
        {
            DrawEntity(scene, entity);
        }
    }

    ImGui::End();
    return reparentError;
}

void HierarchyPanel::DrawEntity(
    Scene& scene,
    ECS::Entity entity)
{
    const auto* identity =
        scene.GetComponent<EntityIdentityComponent>(entity);
    const auto* hierarchy =
        scene.GetComponent<HierarchyComponent>(entity);

    if (identity == nullptr || hierarchy == nullptr)
    {
        return;
    }

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow
        | ImGuiTreeNodeFlags_OpenOnDoubleClick
        | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (m_Context.selection.GetSelectedUUID().has_value()
        && *m_Context.selection.GetSelectedUUID() == identity->id)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    const bool hasChildren =
        hierarchy->firstChild.IsValid();

    if (!hasChildren)
    {
        flags |= ImGuiTreeNodeFlags_Leaf
            | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    const std::string id = identity->id.ToString();
    ImGui::PushID(id.c_str());

    const bool open = ImGui::TreeNodeEx("##entity", flags, "      %s", identity->name.c_str());
    Icon icon = Icon::Entity;
    if (scene.HasComponent<CameraComponent>(entity))
        icon = Icon::Camera;
    else if (scene.HasComponent<CanvasComponent>(entity) ||
             scene.HasComponent<UIRectComponent>(entity))
        icon = Icon::Canvas;
    else if (scene.HasComponent<LuaScriptComponent>(entity) &&
             !scene.HasComponent<SpriteRendererComponent>(entity))
        icon = Icon::Script;
    DrawItemIcon(icon, ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        m_Context.selection.Select(identity->id);
    }

    if (hasChildren && open)
    {
        ECS::Entity child = hierarchy->firstChild;

        while (child.IsValid())
        {
            const auto* childHierarchy =
                scene.GetComponent<HierarchyComponent>(child);
            const ECS::Entity next =
                childHierarchy == nullptr
                    ? ECS::Entity{}
                    : childHierarchy->nextSibling;

            DrawEntity(scene, child);
            child = next;
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
}

} // namespace Janus::Editor
