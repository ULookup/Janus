#include "Scene/Command/SceneCommands.h"

#include "Core/Reflection/ReflectionRegistry.h"
#include "Scene/Scene.h"

#include <utility>

namespace Janus
{

SetPropertyCommand::SetPropertyCommand(
    Scene& scene,
    SceneReflection reflection,
    UUID entity,
    ComponentTypeId component,
    PropertyId property,
    PropertyValue value)
    : m_Scene(scene),
      m_Reflection(std::move(reflection)),
      m_Entity(entity),
      m_Component(component),
      m_Property(property),
      m_Value(std::move(value))
{
}

Result<void> SetPropertyCommand::Execute()
{
    if (m_Delta.has_value())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "SetPropertyCommand may only be executed once before history owns it.");
    }

    auto delta = m_Reflection.ApplyPropertyMutation(
        m_Scene,
        m_Entity,
        m_Component,
        m_Property,
        m_Value);
    if (!delta)
    {
        return Result<void>::Failure(
            delta.GetError());
    }

    m_Delta = std::move(delta).Value();
    return Result<void>::Success();
}

Result<void> SetPropertyCommand::Undo()
{
    if (!m_Delta.has_value())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "SetPropertyCommand has no captured mutation delta.");
    }

    return m_Reflection.RestorePropertyMutation(
        m_Scene,
        *m_Delta,
        PropertyMutationState::Before);
}

Result<void> SetPropertyCommand::Redo()
{
    if (!m_Delta.has_value())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "SetPropertyCommand has no captured mutation delta.");
    }

    return m_Reflection.RestorePropertyMutation(
        m_Scene,
        *m_Delta,
        PropertyMutationState::After);
}

std::string_view SetPropertyCommand::Describe() const noexcept
{
    return "Set Property";
}

AddComponentCommand::AddComponentCommand(
    Scene& scene,
    SceneReflection reflection,
    UUID entity,
    ComponentTypeId component)
    : m_Scene(scene),
      m_Reflection(std::move(reflection)),
      m_Entity(entity),
      m_Component(component)
{
}

Result<void> AddComponentCommand::Execute()
{
    return m_Reflection.AddComponent(
        m_Scene,
        m_Entity,
        m_Component);
}

Result<void> AddComponentCommand::Undo()
{
    return m_Reflection.RemoveComponent(
        m_Scene,
        m_Entity,
        m_Component);
}

Result<void> AddComponentCommand::Redo()
{
    return m_Reflection.AddComponent(
        m_Scene,
        m_Entity,
        m_Component);
}

std::string_view AddComponentCommand::Describe() const noexcept
{
    return "Add Component";
}

RemoveComponentCommand::RemoveComponentCommand(
    Scene& scene,
    SceneReflection reflection,
    UUID entity,
    ComponentTypeId component)
    : m_Scene(scene),
      m_Reflection(std::move(reflection)),
      m_Entity(entity),
      m_Component(component)
{
}

Result<ReflectedComponentSnapshot>
RemoveComponentCommand::CaptureSnapshot() const
{
    const ComponentDescriptor* descriptor =
        m_Reflection.GetRegistry().FindComponent(
            m_Component);
    if (descriptor == nullptr)
    {
        return Result<ReflectedComponentSnapshot>::Failure(
            ErrorCode::InvalidArgument,
            "RemoveComponentCommand references an unknown reflected component.");
    }

    auto present = m_Reflection.HasComponent(
        m_Scene,
        m_Entity,
        m_Component);
    if (!present)
    {
        return Result<ReflectedComponentSnapshot>::Failure(
            present.GetError());
    }

    if (!present.Value())
    {
        return Result<ReflectedComponentSnapshot>::Failure(
            ErrorCode::InvalidState,
            "RemoveComponentCommand target component is not present.");
    }

    ReflectedComponentSnapshot snapshot;
    snapshot.component = m_Component;

    for (const PropertyDescriptor& property :
         descriptor->properties)
    {
        if (!property.serializable)
        {
            continue;
        }

        auto value = m_Reflection.GetProperty(
            m_Scene,
            m_Entity,
            m_Component,
            property.id);
        if (!value)
        {
            return Result<ReflectedComponentSnapshot>::Failure(
                value.GetError());
        }

        snapshot.properties.push_back(
            ReflectedPropertySnapshot{
                property.id,
                std::move(value).Value()});
    }

    return Result<ReflectedComponentSnapshot>::Success(
        std::move(snapshot));
}

Result<void> RemoveComponentCommand::Execute()
{
    if (m_Snapshot.has_value())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "RemoveComponentCommand may only be executed once before history owns it.");
    }

    auto snapshot = CaptureSnapshot();
    if (!snapshot)
    {
        return Result<void>::Failure(
            snapshot.GetError());
    }

    auto removed = m_Reflection.RemoveComponent(
        m_Scene,
        m_Entity,
        m_Component);
    if (!removed)
    {
        return removed;
    }

    m_Snapshot = std::move(snapshot).Value();
    return Result<void>::Success();
}

Result<void> RemoveComponentCommand::RestoreSnapshot()
{
    if (!m_Snapshot.has_value())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "RemoveComponentCommand has no captured component snapshot.");
    }

    auto added = m_Reflection.AddComponent(
        m_Scene,
        m_Entity,
        m_Component);
    if (!added)
    {
        return added;
    }

    for (const ReflectedPropertySnapshot& property :
         m_Snapshot->properties)
    {
        auto restored = m_Reflection.RestoreProperty(
            m_Scene,
            m_Entity,
            m_Component,
            property.property,
            property.value);
        if (!restored)
        {
            const auto cleanup =
                m_Reflection.RemoveComponent(
                    m_Scene,
                    m_Entity,
                    m_Component);
            if (!cleanup)
            {
                return Result<void>::Failure(
                    ErrorCode::InvalidState,
                    "Component snapshot restore and cleanup both failed: "
                        + restored.GetError().message
                        + "; cleanup: "
                        + cleanup.GetError().message);
            }

            return restored;
        }
    }

    auto valid = m_Reflection.ValidateComponent(
        m_Scene,
        m_Entity,
        m_Component);
    if (!valid)
    {
        const auto cleanup =
            m_Reflection.RemoveComponent(
                m_Scene,
                m_Entity,
                m_Component);
        if (!cleanup)
        {
            return Result<void>::Failure(
                ErrorCode::InvalidState,
                "Component snapshot validation and cleanup both failed: "
                    + valid.GetError().message
                    + "; cleanup: "
                    + cleanup.GetError().message);
        }

        return valid;
    }

    return Result<void>::Success();
}

Result<void> RemoveComponentCommand::Undo()
{
    return RestoreSnapshot();
}

Result<void> RemoveComponentCommand::Redo()
{
    if (!m_Snapshot.has_value())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "RemoveComponentCommand has no captured component snapshot.");
    }

    return m_Reflection.RemoveComponent(
        m_Scene,
        m_Entity,
        m_Component);
}

std::string_view RemoveComponentCommand::Describe() const noexcept
{
    return "Remove Component";
}

} // namespace Janus
