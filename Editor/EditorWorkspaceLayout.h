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
    EditorPanelRect viewport;
    EditorPanelRect inspector;
    EditorPanelRect utility;
    EditorPanelRect assets;
    EditorPanelRect diagnostics;
    EditorPanelRect status;
};

enum class EditorLayoutMode
{
    Standard,
    Focus,
    Debug
};

struct EditorWorkspacePreferences
{
    f32 leftWidth = 260.0f;
    f32 rightWidth = 390.0f;
    f32 utilityHeight = 300.0f;
    EditorLayoutMode mode = EditorLayoutMode::Standard;
};

[[nodiscard]] EditorWorkspacePreferences
GetDefaultWorkspacePreferences(EditorLayoutMode mode) noexcept;

[[nodiscard]] EditorWorkspaceLayout
BuildEditorWorkspaceLayout(f32 width, f32 height,
                           const EditorWorkspacePreferences& preferences = {}) noexcept;

[[nodiscard]] EditorPanelRect FitAspectRatio(
    f32 availableWidth,
    f32 availableHeight,
    f32 aspectRatio) noexcept;

} // namespace Janus::Editor
