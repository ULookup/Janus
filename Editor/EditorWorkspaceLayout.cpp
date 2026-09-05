#include "EditorWorkspaceLayout.h"

#include <algorithm>

namespace Janus::Editor
{

namespace
{

constexpr f32 PanelGap = 2.0f;
constexpr f32 MinimumCenterWidth = 320.0f;

[[nodiscard]] f32 ClampNonNegative(f32 value) noexcept
{
    return std::max(value, 0.0f);
}

} // namespace

EditorWorkspaceLayout BuildEditorWorkspaceLayout(
    f32 width,
    f32 height) noexcept
{
    width = ClampNonNegative(width);
    height = ClampNonNegative(height);

    EditorWorkspaceLayout layout;

    const f32 toolbarHeight =
        std::clamp(
            height * 0.052f,
            42.0f,
            50.0f);

    layout.toolbar = EditorPanelRect{
        0.0f,
        0.0f,
        width,
        std::min(toolbarHeight, height)};

    const f32 contentY =
        layout.toolbar.height + PanelGap;
    const f32 contentHeight =
        ClampNonNegative(
            height - contentY);

    const f32 utilityHeight =
        std::min(
            std::clamp(
                height * 0.25f,
                160.0f,
                230.0f),
            contentHeight);

    const f32 topHeight =
        ClampNonNegative(
            contentHeight
            - utilityHeight
            - PanelGap);

    f32 leftWidth =
        std::clamp(
            width * 0.19f,
            220.0f,
            290.0f);
    f32 rightWidth =
        std::clamp(
            width * 0.23f,
            260.0f,
            340.0f);

    const f32 requiredWidth =
        leftWidth
        + rightWidth
        + MinimumCenterWidth
        + PanelGap * 2.0f;

    if (requiredWidth > width)
    {
        const f32 sideBudget =
            ClampNonNegative(
                width
                - MinimumCenterWidth
                - PanelGap * 2.0f);

        leftWidth = sideBudget * 0.45f;
        rightWidth = sideBudget - leftWidth;
    }

    const f32 centerX =
        leftWidth + PanelGap;
    const f32 centerWidth =
        ClampNonNegative(
            width
            - leftWidth
            - rightWidth
            - PanelGap * 2.0f);
    const f32 rightX =
        centerX
        + centerWidth
        + PanelGap;

    layout.hierarchy = EditorPanelRect{
        0.0f,
        contentY,
        leftWidth,
        topHeight};

    layout.viewport = EditorPanelRect{
        centerX,
        contentY,
        centerWidth,
        topHeight};

    layout.inspector = EditorPanelRect{
        rightX,
        contentY,
        rightWidth,
        topHeight};

    layout.utility = EditorPanelRect{
        0.0f,
        contentY + topHeight + PanelGap,
        width,
        utilityHeight};

    return layout;
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
