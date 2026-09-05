#include "Panels/AssetBrowserPanel.h"

#include "EditorActions.h"
#include "EditorContext.h"
#include "ProjectSession.h"

#include "Asset/AssetMetadata.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <imgui.h>

#include <string>

namespace Janus::Editor
{

AssetBrowserPanel::AssetBrowserPanel(
    EditorContext& context,
    EditorActions& actions) noexcept
    : m_Context(context),
      m_Actions(actions)
{
}

std::optional<Error> AssetBrowserPanel::Draw()
{
    const bool visible = ImGui::Begin("Asset Browser");

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

    static const char* FilterNames[] = {
        "All",
        "Texture",
        "LuaScript",
        "ShaderSource"};

    ImGui::SetNextItemWidth(140.0f);
    ImGui::Combo(
        "Type",
        &m_TypeFilter,
        FilterNames,
        static_cast<int>(
            sizeof(FilterNames) / sizeof(FilterNames[0])));

    const auto assets =
        m_Context.project->GetAssetRegistry().GetAssets();

    ImGui::Separator();

    for (const AssetMetadata& asset : assets)
    {
        bool include = true;

        switch (m_TypeFilter)
        {
        case 1:
            include = asset.type == AssetType::Texture;
            break;
        case 2:
            include = asset.type == AssetType::LuaScript;
            break;
        case 3:
            include = asset.type == AssetType::ShaderSource;
            break;
        default:
            break;
        }

        if (!include)
        {
            continue;
        }

        const std::string path =
            asset.relativePath.generic_string();
        const std::string id =
            asset.handle.ToString();

        ImGui::PushID(id.c_str());

        const bool selected =
            m_SelectedAsset.IsValid()
            && m_SelectedAsset == asset.handle;

        if (ImGui::Selectable(
                path.c_str(),
                selected))
        {
            m_SelectedAsset = asset.handle;
        }

        const std::string typeName{
            AssetTypeName(asset.type)};

        ImGui::SameLine();
        ImGui::TextDisabled(
            "[%s]",
            typeName.c_str());

        ImGui::PopID();
    }

    ImGui::Separator();

    if (!m_SelectedAsset.IsValid())
    {
        ImGui::TextDisabled("Select an asset to inspect or assign.");
        ImGui::End();
        return std::nullopt;
    }

    const AssetMetadata* selected =
        m_Context.project->GetAssetRegistry().Find(
            m_SelectedAsset);

    if (selected == nullptr)
    {
        m_SelectedAsset = {};
        ImGui::TextDisabled("Selected asset is no longer registered.");
        ImGui::End();
        return std::nullopt;
    }

    const std::string selectedPath =
        selected->relativePath.generic_string();
    const std::string selectedHandle =
        selected->handle.ToString();

    ImGui::TextWrapped(
        "Path: %s",
        selectedPath.c_str());
    const std::string selectedType{
        AssetTypeName(selected->type)};

    ImGui::Text(
        "Type: %s",
        selectedType.c_str());
    ImGui::TextWrapped(
        "Handle: %s",
        selectedHandle.c_str());

    Scene& scene =
        m_Context.project->GetEditorScene();

    const ECS::Entity target =
        m_Context.selection.Resolve(scene);

    if (!target.IsValid()
        || !m_Context.selection.GetSelectedUUID().has_value())
    {
        ImGui::TextDisabled(
            "Select an entity to assign this asset.");
        ImGui::End();
        return std::nullopt;
    }

    const UUID entityId =
        *m_Context.selection.GetSelectedUUID();

    const bool playing =
        m_Context.project->IsPlaying();
    ImGui::BeginDisabled(playing);

    std::optional<Error> error;

    if (selected->type == AssetType::Texture)
    {
        if (scene.HasComponent<SpriteRendererComponent>(target))
        {
            if (ImGui::Button("Assign to SpriteRenderer"))
            {
                const auto assigned =
                    m_Actions.SetSpriteTexture(
                        entityId,
                        selected->handle);
                if (!assigned)
                {
                    error = assigned.GetError();
                }
            }
        }
        else
        {
            ImGui::TextDisabled(
                "Selected entity has no SpriteRenderer.");
        }
    }
    else if (selected->type == AssetType::LuaScript)
    {
        if (scene.HasComponent<LuaScriptComponent>(target))
        {
            if (ImGui::Button("Assign to LuaScript"))
            {
                const auto assigned =
                    m_Actions.SetLuaScriptAsset(
                        entityId,
                        selected->handle);
                if (!assigned)
                {
                    error = assigned.GetError();
                }
            }
        }
        else
        {
            ImGui::TextDisabled(
                "Selected entity has no LuaScript.");
        }
    }
    else
    {
        ImGui::TextDisabled(
            "This asset type has no v0.6 Inspector assignment.");
    }

    ImGui::EndDisabled();

    if (playing)
    {
        ImGui::TextDisabled("Read-only in Play Mode.");
    }

    ImGui::End();
    return error;
}

} // namespace Janus::Editor
