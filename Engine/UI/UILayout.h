#pragma once

#include "Core/Error/Result.h"
#include "Core/Math/Vector2.h"
#include "Core/UUID/UUID.h"
#include "Renderer/RendererTypes.h"
#include <vector>

namespace Janus
{
class Scene;
struct UIBounds
{
    Vector2 min;
    Vector2 max;
    [[nodiscard]] bool Contains(Vector2 point) const noexcept;
};

enum class UIDrawKind
{
    Panel,
    Image,
    Text
};
struct UILayoutItem
{
    UUID entity;
    UIDrawKind kind;
    UIBounds rect;
    UIBounds visible;
};

struct UILayoutResult
{
    // Preorder, then Panel, Image, then Text on the same entity. Never texture-sort.
    std::vector<UILayoutItem> items;
    [[nodiscard]] UUID HitTest(Vector2 point) const noexcept;
};

class UILayout final
{
  public:
    [[nodiscard]] static Result<UILayoutResult> Build(const Scene& scene, Viewport logicalViewport);
};
} // namespace Janus
