#include "Panels/InspectorPanel.h"

#include "EditorActions.h"
#include "EditorContext.h"
#include "ProjectSession.h"

#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <string>

namespace Janus::Editor
{

InspectorPanel::InspectorPanel(
    EditorContext& context,
    EditorActions& actions) noexcept
    : m_Context(context),
      m_Actions(actions)
{
}

std::optional<Error> InspectorPanel::Draw()
{
    const bool visible = ImGui::Begin("Inspector");

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

    auto* identity =
        scene.GetComponent<EntityIdentityComponent>(entity);
    auto* transform =
        scene.GetComponent<TransformComponent>(entity);

    if (identity == nullptr || transform == nullptr)
    {
        m_Context.selection.Clear();
        ImGui::TextUnformatted("Selected entity is incomplete.");
        ImGui::End();
        return std::nullopt;
    }

    const bool readOnly =
        m_Context.project->IsPlaying();

    if (readOnly)
    {
        ImGui::TextDisabled(
            "Authoring is read-only while Play Mode is active.");
        ImGui::Separator();
    }

    SyncNameBuffer(id, identity->name.c_str());

    std::optional<Error> error;

    ImGui::BeginDisabled(readOnly);

    if (ImGui::InputText(
            "Name",
            m_NameBuffer.data(),
            m_NameBuffer.size(),
            ImGuiInputTextFlags_EnterReturnsTrue))
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

    if (ImGui::CollapsingHeader(
            "Transform",
            ImGuiTreeNodeFlags_DefaultOpen))
    {
        float position[]{
            transform->position.x,
            transform->position.y};
        float rotation =
            transform->rotationRadians;
        float scale[]{
            transform->scale.x,
            transform->scale.y};

        bool changed = false;
        changed |= ImGui::DragFloat2(
            "Position",
            position,
            0.5f);
        changed |= ImGui::DragFloat(
            "Rotation",
            &rotation,
            0.01f);
        changed |= ImGui::DragFloat2(
            "Scale",
            scale,
            0.01f);

        if (changed && !error.has_value())
        {
            const auto updated =
                m_Actions.SetTransform(
                    id,
                    Vector2{position[0], position[1]},
                    rotation,
                    Vector2{scale[0], scale[1]});
            if (!updated)
            {
                error = updated.GetError();
            }
        }

        ImGui::TextDisabled(
            "World: (%.2f, %.2f)",
            transform->worldPosition.x,
            transform->worldPosition.y);
    }

    auto* sprite =
        scene.GetComponent<SpriteRendererComponent>(entity);

    if (sprite != nullptr)
    {
        if (ImGui::CollapsingHeader(
                "SpriteRenderer",
                ImGuiTreeNodeFlags_DefaultOpen))
        {
            const std::string handle =
                sprite->texture.IsValid()
                    ? sprite->texture.ToString()
                    : std::string{"<none>"};

            ImGui::TextWrapped(
                "Texture: %s",
                handle.c_str());

            float size[]{
                sprite->size.x,
                sprite->size.y};
            float color[]{
                sprite->color.r,
                sprite->color.g,
                sprite->color.b,
                sprite->color.a};
            int layer = sprite->layer;
            bool enabled = sprite->enabled;

            bool changed = false;
            changed |= ImGui::DragFloat2(
                "Size##Sprite",
                size,
                0.5f);
            changed |= ImGui::ColorEdit4(
                "Color##Sprite",
                color);
            changed |= ImGui::InputInt(
                "Layer##Sprite",
                &layer);
            changed |= ImGui::Checkbox(
                "Enabled##Sprite",
                &enabled);

            if (changed && !error.has_value())
            {
                const auto updated =
                    m_Actions.SetSpriteRenderer(
                        id,
                        Vector2{size[0], size[1]},
                        Color{
                            color[0],
                            color[1],
                            color[2],
                            color[3]},
                        layer,
                        enabled);
                if (!updated)
                {
                    error = updated.GetError();
                }
            }

            if (ImGui::Button("Remove SpriteRenderer")
                && !error.has_value())
            {
                const auto removed =
                    m_Actions.RemoveSpriteRenderer(id);
                if (!removed)
                {
                    error = removed.GetError();
                }
            }
        }
    }
    else if (ImGui::Button("Add SpriteRenderer")
        && !error.has_value())
    {
        const auto added =
            m_Actions.AddSpriteRenderer(id);
        if (!added)
        {
            error = added.GetError();
        }
    }

    auto* camera =
        scene.GetComponent<CameraComponent>(entity);

    if (camera != nullptr)
    {
        if (ImGui::CollapsingHeader(
                "Camera",
                ImGuiTreeNodeFlags_DefaultOpen))
        {
            float zoom = camera->zoom;
            bool primary = camera->primary;

            bool changed = false;
            changed |= ImGui::DragFloat(
                "Zoom##Camera",
                &zoom,
                0.05f,
                0.05f,
                20.0f);
            changed |= ImGui::Checkbox(
                "Primary##Camera",
                &primary);

            if (changed && !error.has_value())
            {
                const auto updated =
                    m_Actions.SetCamera(
                        id,
                        zoom,
                        primary);
                if (!updated)
                {
                    error = updated.GetError();
                }
            }

            if (ImGui::Button("Remove Camera")
                && !error.has_value())
            {
                const auto removed =
                    m_Actions.RemoveCamera(id);
                if (!removed)
                {
                    error = removed.GetError();
                }
            }
        }
    }
    else if (ImGui::Button("Add Camera")
        && !error.has_value())
    {
        const auto added =
            m_Actions.AddCamera(id);
        if (!added)
        {
            error = added.GetError();
        }
    }

    auto* script =
        scene.GetComponent<LuaScriptComponent>(entity);

    if (script != nullptr)
    {
        if (ImGui::CollapsingHeader(
                "LuaScript",
                ImGuiTreeNodeFlags_DefaultOpen))
        {
            const std::string handle =
                script->script.IsValid()
                    ? script->script.ToString()
                    : std::string{"<none>"};

            ImGui::TextWrapped(
                "Script: %s",
                handle.c_str());

            bool enabled = script->enabled;

            ImGui::BeginDisabled(
                !script->script.IsValid());
            const bool enabledChanged =
                ImGui::Checkbox(
                    "Enabled##Lua",
                    &enabled);
            ImGui::EndDisabled();

            if (enabledChanged && !error.has_value())
            {
                const auto updated =
                    m_Actions.SetLuaScriptEnabled(
                        id,
                        enabled);
                if (!updated)
                {
                    error = updated.GetError();
                }
            }

            if (!script->script.IsValid())
            {
                ImGui::TextDisabled(
                    "Script assignment is completed by Asset Browser (#36).");
            }

            if (ImGui::Button("Remove LuaScript")
                && !error.has_value())
            {
                const auto removed =
                    m_Actions.RemoveLuaScript(id);
                if (!removed)
                {
                    error = removed.GetError();
                }
            }
        }
    }
    else if (ImGui::Button("Add LuaScript")
        && !error.has_value())
    {
        const auto added =
            m_Actions.AddLuaScript(id);
        if (!added)
        {
            error = added.GetError();
        }
    }

    ImGui::EndDisabled();

    ImGui::End();
    return error;
}

void InspectorPanel::SyncNameBuffer(
    UUID id,
    const char* name)
{
    if (m_NameBufferEntity == id)
    {
        return;
    }

    m_NameBuffer.fill('\0');

    if (name != nullptr)
    {
        const std::string value{name};
        const std::size_t count =
            std::min(
                value.size(),
                m_NameBuffer.size() - 1);

        std::copy_n(
            value.data(),
            count,
            m_NameBuffer.data());
    }

    m_NameBufferEntity = id;
}

} // namespace Janus::Editor
