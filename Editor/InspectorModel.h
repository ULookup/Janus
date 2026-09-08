#pragma once

#include "Core/Error/Result.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Core/UUID/UUID.h"

#include <optional>
#include <vector>

namespace Janus
{

class Scene;

namespace Editor
{
class ProjectSession;
class EditorActions;

// UI buffers remain presentation state. This value model owns the original binding and
// authoring stamp so retrying a failed edit cannot accidentally overwrite newer authoring.
class InspectorEditDraft final
{
  public:
    Result<void> BeginName(ProjectSession& project, UUID entity, std::string original);
    Result<void> BeginProperty(ProjectSession& project, UUID entity, ComponentTypeId component,
                               PropertyId property, PropertyValue original);
    void SetValue(PropertyValue value);
    Result<void> Commit(ProjectSession& project, EditorActions& actions);
    void Cancel() noexcept;
    bool IsActive() const noexcept
    {
        return m_Active;
    }
    bool IsEdited() const;
    bool MatchesName(UUID entity) const noexcept;
    bool MatchesProperty(UUID entity, ComponentTypeId component,
                         PropertyId property) const noexcept;
    UUID GetEntity() const noexcept
    {
        return m_Entity;
    }
    const PropertyValue& GetValue() const noexcept
    {
        return m_Value;
    }
    const std::optional<Error>& GetError() const noexcept
    {
        return m_Error;
    }

  private:
    Result<void> Begin(ProjectSession& project, UUID entity, ComponentTypeId component,
                       PropertyId property, PropertyValue original, bool name);
    bool m_Active = false;
    bool m_Name = false;
    UUID m_Project;
    UUID m_Entity;
    u64 m_Revision = 0;
    u64 m_Generation = 0;
    ComponentTypeId m_Component;
    PropertyId m_Property;
    PropertyValue m_Original;
    PropertyValue m_Value;
    std::optional<Error> m_Error;
};

struct InspectorPropertyModel
{
    const PropertyDescriptor* descriptor = nullptr;
    PropertyValue value;
};

struct InspectorComponentModel
{
    const ComponentDescriptor* descriptor = nullptr;
    bool present = false;
    std::vector<InspectorPropertyModel> properties;
};

[[nodiscard]] Result<std::vector<InspectorComponentModel>>
BuildInspectorModel(
    const Scene& scene,
    UUID entity,
    const ReflectionRegistry& registry);

} // namespace Editor
} // namespace Janus
