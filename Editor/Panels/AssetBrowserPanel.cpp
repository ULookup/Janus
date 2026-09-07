#include "Panels/AssetBrowserPanel.h"

#include "EditorActions.h"
#include "EditorContext.h"
#include "EditorIcons.h"
#include "ProjectSession.h"

#include "Asset/AssetMetadata.h"
#include "Audio/AudioSourceComponent.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "UI/UIComponents.h"

#include <imgui.h>

#include "Animation/AnimatorComponent.h"
#include "Asset/AssetService.h"
#include "Renderer/Renderer2D.h"
#include <algorithm>
#include <set>
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

std::optional<Error> AssetBrowserPanel::DrawContents()
{
    if (m_Context.project == nullptr)
    {
        ImGui::TextUnformatted("No project open.");
        return std::nullopt;
    }

    const auto assets = m_Context.project->GetAssetRegistry().GetAssets();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.48f);
    ImGui::InputTextWithHint("##Search", "Search assets...", m_Search.data(), m_Search.size());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.64f);
    ImGui::Combo(
        "##Type", &m_TypeFilter,
        "All types\0Texture\0LuaScript\0ShaderSource\0Font\0AudioClip\0Prefab\0AnimationClip\0");
    ImGui::SameLine();
    if (IconButton(m_Grid ? Icon::List : Icon::Grid, m_Grid ? "List" : "Grid"))
        m_Grid = !m_Grid;
    const float footer = m_SelectedAsset.IsValid() ? ImGui::GetFrameHeightWithSpacing() * 3.0f
                                                   : ImGui::GetTextLineHeightWithSpacing() +
                                                         ImGui::GetStyle().ItemSpacing.y * 2;
    const float height = std::max(60.0f, ImGui::GetContentRegionAvail().y - footer);
    if (ImGui::BeginChild("AssetFolders", ImVec2{ImGui::GetFontSize() * 9, height},
                          ImGuiChildFlags_Borders))
    {
        if (IconSelectable(Icon::Folder, "All assets", m_Folder.empty()))
            m_Folder.clear();
        std::set<std::string> folders;
        for (const auto& asset : assets)
            folders.insert(asset.relativePath.parent_path().generic_string());
        for (const auto& folder : folders)
            if (IconSelectable(Icon::Folder, folder.empty() ? "(root)" : folder.c_str(),
                               m_Folder == folder))
                m_Folder = folder;
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("AssetItems", ImVec2{0, height}, ImGuiChildFlags_Borders);
    ImGui::TextDisabled("Assets / %s", m_Folder.empty() ? "All" : m_Folder.c_str());
    const AssetType types[] = {
        AssetType::Texture,   AssetType::LuaScript, AssetType::ShaderSource, AssetType::Font,
        AssetType::AudioClip, AssetType::Prefab,    AssetType::AnimationClip};
    const float tile = ImGui::GetFontSize() * 8;
    int column = 0;
    const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / tile));
    for (const auto& asset : assets)
    {
        const std::string path = asset.relativePath.generic_string();
        if (m_TypeFilter > 0 && asset.type != types[m_TypeFilter - 1])
            continue;
        if (!m_Folder.empty() && asset.relativePath.parent_path().generic_string() != m_Folder)
            continue;
        if (m_Search[0] && path.find(m_Search.data()) == std::string::npos)
            continue;
        ImGui::PushID(asset.handle.ToString().c_str());
        if (m_Grid)
        {
            if (column % columns)
                ImGui::SameLine();
            ImGui::BeginGroup();
            bool previewDrawn = false;
            if (asset.type == AssetType::Texture && m_Context.renderer)
            {
                const auto texture = m_Context.project->GetAssetService().LoadTexture(asset.handle);
                if (texture)
                {
                    const auto handle =
                        m_Context.renderer->GetTexturePresentationHandle(texture.Value());
                    if (handle)
                    {
                        ImGui::Image(ImTextureRef{static_cast<ImTextureID>(handle.Value().value)},
                                     ImVec2{tile - ImGui::GetStyle().ItemSpacing.x,
                                            ImGui::GetFontSize() * 3});
                        if (ImGui::IsItemClicked())
                            m_SelectedAsset = asset.handle;
                        previewDrawn = true;
                    }
                }
            }
            if (!previewDrawn)
            {
                const ImVec2 origin = ImGui::GetCursorScreenPos();
                const ImVec2 size{tile - ImGui::GetStyle().ItemSpacing.x, ImGui::GetFontSize() * 3};
                ImGui::GetWindowDrawList()->AddRectFilled(
                    origin, ImVec2{origin.x + size.x, origin.y + size.y}, IM_COL32(42, 51, 64, 255),
                    4);
                const float iconSize = ImGui::GetFontSize() * 2;
                DrawIcon(
                    AssetIcon(AssetTypeName(asset.type)),
                    {origin.x + (size.x - iconSize) * 0.5f, origin.y + (size.y - iconSize) * 0.5f},
                    iconSize);
                ImGui::Dummy(size);
                if (ImGui::IsItemClicked())
                    m_SelectedAsset = asset.handle;
            }
            const auto type = AssetTypeName(asset.type);
            const std::string label =
                "[ " + std::string(type) + " ]\n" + asset.relativePath.stem().string();
            if (ImGui::Selectable(
                    label.c_str(), asset.handle == m_SelectedAsset, 0,
                    ImVec2{tile - ImGui::GetStyle().ItemSpacing.x, ImGui::GetFontSize() * 3.5f}))
                m_SelectedAsset = asset.handle;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", path.c_str());
            ImGui::EndGroup();
            ++column;
        }
        else if (IconSelectable(AssetIcon(AssetTypeName(asset.type)), path.c_str(),
                                asset.handle == m_SelectedAsset))
            m_SelectedAsset = asset.handle;
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::Separator();

    if (!m_SelectedAsset.IsValid())
    {
        ImGui::TextDisabled("Select an asset to inspect or assign.");
        return std::nullopt;
    }

    const AssetMetadata* selected =
        m_Context.project->GetAssetRegistry().Find(
            m_SelectedAsset);

    if (selected == nullptr)
    {
        m_SelectedAsset = {};
        ImGui::TextDisabled("Selected asset is no longer registered.");
        return std::nullopt;
    }

    const std::string selectedPath =
        selected->relativePath.generic_string();
    const std::string selectedHandle =
        selected->handle.ToString();

    ImGui::TextUnformatted(selected->relativePath.filename().string().c_str());
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s\nUUID: %s", selectedPath.c_str(), selectedHandle.c_str());
    if (selected->type == AssetType::Prefab)
    {
        std::optional<Error> error;
        ImGui::BeginDisabled(m_Context.project->IsAuthoringReadOnly());
        if (IconButton(Icon::Prefab, "Instantiate Prefab"))
        {
            auto created = m_Actions.InstantiatePrefab(selected->handle);
            if (!created)
                error = created.GetError();
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("Creates an independent subtree. Undo removes the whole instance.");
        return error;
    }

    Scene& scene =
        m_Context.project->GetEditorScene();

    const ECS::Entity target =
        m_Context.selection.Resolve(scene);

    if (!target.IsValid()
        || !m_Context.selection.GetSelectedUUID().has_value())
    {
        ImGui::TextDisabled(
            "Select an entity to assign this asset.");
        return std::nullopt;
    }

    const UUID entityId =
        *m_Context.selection.GetSelectedUUID();

    const bool playing = m_Context.project->IsAuthoringReadOnly();
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
    else if (selected->type == AssetType::AnimationClip)
    {
        if (scene.HasComponent<AnimatorComponent>(target) && ImGui::Button("Assign to Animator"))
        {
            auto assigned = m_Actions.SetProperty(entityId, MakeComponentTypeId("Animator"),
                                                  MakePropertyId("Animator.clip"),
                                                  AssetReferenceValue{selected->handle.id});
            if (!assigned)
                error = assigned.GetError();
        }
    }
    else if (selected->type == AssetType::Font)
    {
        if (scene.HasComponent<TextComponent>(target))
        {
            if (ImGui::Button("Assign to Text"))
            {
                auto assigned = m_Actions.SetProperty(entityId, MakeComponentTypeId("Text"),
                                                      MakePropertyId("Text.font"),
                                                      AssetReferenceValue{selected->handle.id});
                if (!assigned)
                    error = assigned.GetError();
            }
        }
        else
            ImGui::TextDisabled("Selected entity has no Text.");
    }
    else if (selected->type == AssetType::AudioClip)
    {
        if (scene.HasComponent<AudioSourceComponent>(target))
        {
            if (ImGui::Button("Assign to AudioSource"))
            {
                auto assigned = m_Actions.SetProperty(entityId, MakeComponentTypeId("AudioSource"),
                                                      MakePropertyId("AudioSource.clip"),
                                                      AssetReferenceValue{selected->handle.id});
                if (!assigned)
                    error = assigned.GetError();
            }
        }
        else
            ImGui::TextDisabled("Selected entity has no AudioSource.");
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
        ImGui::TextDisabled("This asset type has no component assignment.");
    }

    ImGui::EndDisabled();

    if (playing)
    {
        ImGui::TextDisabled("Read-only in Play Mode.");
    }

    return error;
}

} // namespace Janus::Editor
