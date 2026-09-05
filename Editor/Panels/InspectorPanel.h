#pragma once

#include "Core/Error/Error.h"
#include "Core/UUID/UUID.h"

#include <array>
#include <optional>

namespace Janus::Editor
{

class EditorActions;
struct EditorContext;

class InspectorPanel final
{
public:
    InspectorPanel(
        EditorContext& context,
        EditorActions& actions) noexcept;

    [[nodiscard]] std::optional<Error> Draw();

private:
    void SyncNameBuffer(
        UUID id,
        const char* name);

    EditorContext& m_Context;
    EditorActions& m_Actions;

    UUID m_NameBufferEntity;
    std::array<char, 256> m_NameBuffer{};
};

} // namespace Janus::Editor
