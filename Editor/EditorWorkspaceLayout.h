#pragma once

#include "Core/Types.h"

namespace Janus::Editor
{

struct EditorPanelRect
{
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 width = 0.0f;
    f32 height = 0.0f;
};

struct EditorWorkspaceLayout
{
    EditorPanelRect toolbar;
    EditorPanelRect hierarchy;
    EditorPanelRect assetBrowser;
    EditorPanelRect sceneView;
    EditorPanelRect gameView;
    EditorPanelRect inspector;
    EditorPanelRect console;
};

[[nodiscard]] EditorWorkspaceLayout BuildEditorWorkspaceLayout(
    f32 width,
    f32 height) noexcept;

} // namespace Janus::Editor
