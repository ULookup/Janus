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
};

[[nodiscard]] EditorWorkspaceLayout BuildEditorWorkspaceLayout(
    f32 width,
    f32 height) noexcept;

[[nodiscard]] EditorPanelRect FitAspectRatio(
    f32 availableWidth,
    f32 availableHeight,
    f32 aspectRatio) noexcept;

} // namespace Janus::Editor
