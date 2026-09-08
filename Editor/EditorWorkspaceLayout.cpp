#include "EditorWorkspaceLayout.h"

#include <algorithm>
#include <cmath>

namespace Janus::Editor
{

namespace
{


[[nodiscard]] f32 ClampNonNegative(f32 value) noexcept
{
    return std::isfinite(value) ? std::max(value, 0.0f) : 0.0f;
}

} // namespace

EditorWorkspacePreferences GetDefaultWorkspacePreferences(EditorLayoutMode mode) noexcept
{
    if (mode == EditorLayoutMode::Debug)
        return {220, 330, 400, mode};
    return {260, 390, 300, mode};
}

EditorWorkspaceLayout
BuildEditorWorkspaceLayout(f32 width, f32 height,
                           const EditorWorkspacePreferences& preferences) noexcept
{
    width = ClampNonNegative(width);
    height = ClampNonNegative(height);
    const f32 gap = std::min(6.0f, width / 4.0f);
    const f32 toolbar = std::min(height, 80.0f);
    const f32 status = std::min(height - toolbar, 26.0f);
    const f32 contentY = toolbar;
    const f32 contentHeight = std::max(0.0f, height - toolbar - status);
    if (preferences.mode == EditorLayoutMode::Focus)
    {
        EditorWorkspaceLayout result;
        result.toolbar = {0, 0, width, toolbar};
        result.viewport = {0, contentY, width, contentHeight};
        result.status = {0, height - status, width, status};
        return result;
    }
    const f32 sideBudget = std::max(0.0f, width - std::min(width, 360.0f) - gap * 2);
    f32 left = std::clamp(std::isfinite(preferences.leftWidth) ? preferences.leftWidth : 260.0f,
                          170.0f, 420.0f);
    f32 right = std::clamp(std::isfinite(preferences.rightWidth) ? preferences.rightWidth : 390.0f,
                           250.0f, 520.0f);
    if (left + right > sideBudget)
    {
        const f32 factor = sideBudget / (left + right);
        left *= factor;
        right *= factor;
    }
    const f32 centerX = left + gap;
    const f32 rightX = std::max(centerX, width - right);
    const f32 centerWidth = std::max(0.0f, rightX - centerX - gap);
    const f32 bottomWidth = std::max(0.0f, rightX - gap);
    const f32 utility = std::min(
        std::clamp(std::isfinite(preferences.utilityHeight) ? preferences.utilityHeight : 300.0f,
                   120.0f, 600.0f),
        contentHeight * 0.48f);
    const f32 verticalGap = std::min(gap, contentHeight - utility);
    const f32 top = std::max(0.0f, contentHeight - utility - verticalGap);
    EditorWorkspaceLayout result;
    result.toolbar = {0, 0, width, toolbar};
    result.hierarchy = {0, contentY, left, top};
    result.viewport = {centerX, contentY, centerWidth, top};
    result.inspector = {rightX, contentY, right, contentHeight};
    result.utility = {0, contentY + top + verticalGap, bottomWidth, utility};
    result.assets = result.utility;
    if (width >= 1200 && bottomWidth >= 800)
    {
        const f32 assetWidth =
            (bottomWidth - gap) * (preferences.mode == EditorLayoutMode::Debug ? 0.35f : 0.60f);
        result.assets.width = assetWidth;
        result.diagnostics = {assetWidth + gap, result.utility.y, bottomWidth - assetWidth - gap,
                              utility};
    }
    result.status = {0, height - status, width, status};
    return result;
}

EditorPanelRect FitAspectRatio(
    f32 availableWidth,
    f32 availableHeight,
    f32 aspectRatio) noexcept
{
    availableWidth = ClampNonNegative(availableWidth);
    availableHeight = ClampNonNegative(availableHeight);

    if (availableWidth == 0.0f
        || availableHeight == 0.0f
        || aspectRatio <= 0.0f)
    {
        return {};
    }

    f32 width = availableWidth;
    f32 height = width / aspectRatio;

    if (height > availableHeight)
    {
        height = availableHeight;
        width = height * aspectRatio;
    }

    return EditorPanelRect{
        (availableWidth - width) * 0.5f,
        (availableHeight - height) * 0.5f,
        width,
        height};
}

} // namespace Janus::Editor
