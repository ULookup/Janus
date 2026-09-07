#pragma once

#include "Core/Error/Error.h"
#include "ECS/Entity.h"

#include <array>
#include <optional>

namespace Janus
{

class Scene;

namespace Editor
{

class EditorActions;
struct EditorContext;

class HierarchyPanel final
{
public:
    HierarchyPanel(
        EditorContext& context,
        EditorActions& actions) noexcept;

    [[nodiscard]] std::optional<Error> Draw();

private:
    void DrawEntity(
        Scene& scene,
        ECS::Entity entity);

    EditorContext& m_Context;
    EditorActions& m_Actions;
    std::array<char, 128> m_Search{};
};

} // namespace Editor
} // namespace Janus
