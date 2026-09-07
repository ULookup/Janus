#include "UI/UILayout.h"
#include "Scene/Scene.h"
#include "UI/UIComponents.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace Janus
{
namespace
{
bool Finite(Vector2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}
bool Unit(Vector2 value)
{
    return Finite(value) && value.x >= 0 && value.x <= 1 && value.y >= 0 && value.y <= 1;
}
UIBounds Intersect(UIBounds a, UIBounds b)
{
    return {{std::max(a.min.x, b.min.x), std::max(a.min.y, b.min.y)},
            {std::min(a.max.x, b.max.x), std::min(a.max.y, b.max.y)}};
}
} // namespace

Result<void> ValidateUIRect(const UIRectComponent& rect)
{
    if (!Unit(rect.anchorMin) || !Unit(rect.anchorMax) || !Unit(rect.pivot) ||
        rect.anchorMin.x > rect.anchorMax.x || rect.anchorMin.y > rect.anchorMax.y ||
        !Finite(rect.offset) || !Finite(rect.size))
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "UIRect requires finite geometry, ordered anchors and a pivot in [0, 1].");
    return Result<void>::Success();
}

bool UIBounds::Contains(Vector2 point) const noexcept
{
    return point.x >= min.x && point.y >= min.y && point.x < max.x && point.y < max.y;
}
UUID UILayoutResult::HitTest(Vector2 point) const noexcept
{
    for (auto it = items.rbegin(); it != items.rend(); ++it)
        if (it->visible.Contains(point))
            return it->entity;
    return {};
}

Result<UILayoutResult> UILayout::Build(const Scene& scene, Viewport logicalViewport)
{
    using Output = Result<UILayoutResult>;
    if (logicalViewport.width == 0 || logicalViewport.height == 0)
        return Output::Failure(ErrorCode::InvalidArgument, "UI logical viewport must be non-zero.");
    ECS::Entity canvas;
    for (auto entity : scene.GetEntities())
    {
        if (!scene.HasComponent<CanvasComponent>(entity))
            continue;
        if (canvas.IsValid() || scene.GetComponent<HierarchyComponent>(entity)->parent.IsValid())
            return Output::Failure(
                ErrorCode::InvalidState,
                "UI supports one root Canvas; nested or multiple canvases are unsupported.");
        canvas = entity;
    }
    UILayoutResult result;
    result.screen = {
        {0, 0},
        {static_cast<f32>(logicalViewport.width), static_cast<f32>(logicalViewport.height)}};
    if (!canvas.IsValid() || !scene.GetComponent<CanvasComponent>(canvas)->enabled)
        return Output::Success(std::move(result));

    UIBounds screen{
        {0, 0},
        {static_cast<f32>(logicalViewport.width), static_cast<f32>(logicalViewport.height)}};
    std::function<Result<void>(ECS::Entity, UIBounds, UIBounds)> visit;
    visit = [&](ECS::Entity entity, UIBounds parent, UIBounds clip) -> Result<void>
    {
        UIBounds bounds = parent;
        const auto* rect = scene.GetComponent<UIRectComponent>(entity);
        if (rect && entity != canvas)
        {
            auto valid = ValidateUIRect(*rect);
            if (!valid)
                return valid;
            if (!rect->enabled)
                return Result<void>::Success();
            Vector2 parentSize{parent.max.x - parent.min.x, parent.max.y - parent.min.y};
            Vector2 size{parentSize.x * (rect->anchorMax.x - rect->anchorMin.x) + rect->size.x,
                         parentSize.y * (rect->anchorMax.y - rect->anchorMin.y) + rect->size.y};
            if (!Finite(size) || size.x < 0 || size.y < 0)
                return Result<void>::Failure(
                    ErrorCode::InvalidArgument,
                    "UIRect resolved size must be finite and non-negative.");
            bounds.min = {parent.min.x + parentSize.x * rect->anchorMin.x + rect->offset.x -
                              rect->pivot.x * rect->size.x,
                          parent.min.y + parentSize.y * rect->anchorMin.y + rect->offset.y -
                              rect->pivot.y * rect->size.y};
            bounds.max = {bounds.min.x + size.x, bounds.min.y + size.y};
            if (!Finite(bounds.min) || !Finite(bounds.max))
                return Result<void>::Failure(ErrorCode::InvalidArgument,
                                             "UIRect resolved bounds overflowed.");
        }
        const auto visible = Intersect(bounds, clip);
        if ((rect || entity == canvas) && visible.min.x < visible.max.x &&
            visible.min.y < visible.max.y)
        {
            const auto id = scene.GetComponent<EntityIdentityComponent>(entity)->id;
            const auto* button = scene.GetComponent<ButtonComponent>(entity);
            if (button && button->enabled)
                result.items.push_back({id, UIDrawKind::Button, bounds, visible});
            const auto* panel = scene.GetComponent<PanelComponent>(entity);
            if (panel && panel->enabled)
                result.items.push_back({id, UIDrawKind::Panel, bounds, visible});
            const auto* image = scene.GetComponent<ImageComponent>(entity);
            if (image && image->enabled && image->texture.id.IsValid())
                result.items.push_back({id, UIDrawKind::Image, bounds, visible});
            const auto* text = scene.GetComponent<TextComponent>(entity);
            if (text && text->enabled && !text->content.empty())
                result.items.push_back({id, UIDrawKind::Text, bounds, visible});
        }
        if (rect && rect->clipChildren)
            clip = visible;
        auto child = scene.GetComponent<HierarchyComponent>(entity)->firstChild;
        while (child.IsValid())
        {
            auto visited = visit(child, bounds, clip);
            if (!visited)
                return visited;
            child = scene.GetComponent<HierarchyComponent>(child)->nextSibling;
        }
        return Result<void>::Success();
    };
    auto visited = visit(canvas, screen, screen);
    if (!visited)
        return Output::Failure(visited.GetError());
    return Output::Success(std::move(result));
}
} // namespace Janus
