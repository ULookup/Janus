#include "EditorWorkspaceLayout.h"

#include <algorithm>

namespace Janus::Editor
{

namespace
{

constexpr f32 PanelGap = 2.0f;
constexpr f32 MinimumCenterWidth = 360.0f;

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
            height * 0.055f,
            40.0f,
            52.0f);

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

    f32 leftWidth =
        std::clamp(
            width * 0.20f,
            220.0f,
            300.0f);
    f32 rightWidth =
        std::clamp(
            width * 0.24f,
            260.0f,
            360.0f);

    const f32 required =
        leftWidth
        + rightWidth
        + MinimumCenterWidth
        + PanelGap * 2.0f;

    if (required > width)
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

    const f32 leftTopHeight =
        ClampNonNegative(
            contentHeight * 0.56f
            - PanelGap * 0.5f);
    const f32 leftBottomY =
        contentY
        + leftTopHeight
        + PanelGap;
    const f32 leftBottomHeight =
        ClampNonNegative(
            height - leftBottomY);

    layout.hierarchy = EditorPanelRect{
        0.0f,
        contentY,
        leftWidth,
        leftTopHeight};

    layout.assetBrowser = EditorPanelRect{
        0.0f,
        leftBottomY,
        leftWidth,
        leftBottomHeight};

    const f32 centerTopHeight =
        ClampNonNegative(
            contentHeight * 0.68f
            - PanelGap * 0.5f);
    const f32 centerBottomY =
        contentY
        + centerTopHeight
        + PanelGap;
    const f32 centerBottomHeight =
        ClampNonNegative(
            height - centerBottomY);

    layout.sceneView = EditorPanelRect{
        centerX,
        contentY,
        centerWidth,
        centerTopHeight};

    layout.gameView = EditorPanelRect{
        centerX,
        centerBottomY,
        centerWidth,
        centerBottomHeight};

    const f32 rightTopHeight =
        ClampNonNegative(
            contentHeight * 0.64f
            - PanelGap * 0.5f);
    const f32 rightBottomY =
        contentY
        + rightTopHeight
        + PanelGap;
    const f32 rightBottomHeight =
        ClampNonNegative(
            height - rightBottomY);

    layout.inspector = EditorPanelRect{
        rightX,
        contentY,
        rightWidth,
        rightTopHeight};

    layout.console = EditorPanelRect{
        rightX,
        rightBottomY,
        rightWidth,
        rightBottomHeight};

    return layout;
}

} // namespace Janus::Editor
