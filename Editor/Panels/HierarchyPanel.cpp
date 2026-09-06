#include "Panels/HierarchyPanel.h"

#include "EditorActions.h"
#include "EditorContext.h"
#include "ProjectSession.h"

#include "Scene/Components.h"
#include "Scene/Hierarchy.h"
#include "Scene/Scene.h"

#include <imgui.h>

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
    const bool visible = ImGui::Begin(
        "Hierarchy",
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
    m_Context.selection.Validate(scene);

    const bool playing =
        m_Context.project->IsPlaying();

    ImGui::BeginDisabled(playing);

    if (ImGui::Button("+ Entity"))
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
    if (ImGui::Button("Delete")
        && m_Context.selection.GetSelectedUUID().has_value())
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

    if (playing)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Read-only in Play");
    }

    std::optional<Error> reparentError;
    if (auto selected = m_Context.selection.GetSelectedUUID(); selected.has_value())
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

    ImGui::Separator();

    for (const ECS::Entity entity : scene.GetEntities())
    {
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

    const bool open =
        ImGui::TreeNodeEx(
            "##entity",
            flags,
            "%s",
            identity->name.c_str());

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
