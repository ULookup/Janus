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
    const bool visible = ImGui::Begin("Hierarchy");

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
    return std::nullopt;
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
