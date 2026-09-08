#pragma once

#include "Core/Error/Error.h"
#include "Core/UUID/UUID.h"
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
    std::array<char, 256> m_PrefabName{};
    UUID m_ExportEntity;
    bool m_OpenExport = false;
    std::optional<Error> m_ExportError;
};

} // namespace Editor
} // namespace Janus
