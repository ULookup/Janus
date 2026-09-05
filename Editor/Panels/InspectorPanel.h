#pragma once

#include "Core/Error/Error.h"
#include "Core/Reflection/ReflectionTypes.h"
#include "Core/UUID/UUID.h"
#include "InspectorModel.h"

#include <array>
#include <optional>
#include <unordered_map>

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

    void SyncPropertyBuffers(UUID id);

    [[nodiscard]] std::optional<Error> DrawProperty(
        UUID entity,
        ComponentTypeId component,
        const InspectorPropertyModel& property);

    EditorContext& m_Context;
    EditorActions& m_Actions;

    UUID m_NameBufferEntity;
    std::array<char, 256> m_NameBuffer{};
    bool m_NameEditing = false;

    UUID m_PropertyBufferEntity;
    std::optional<u64> m_ActiveProperty;
    std::unordered_map<u64, PropertyValue> m_PropertyBuffers;
    std::unordered_map<u64, std::array<char, 256>> m_StringBuffers;
};

} // namespace Janus::Editor
