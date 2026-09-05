#pragma once

#include "Core/Command/ICommand.h"
#include "Core/Reflection/ReflectionTypes.h"
#include "Core/UUID/UUID.h"
#include "Scene/SceneReflection.h"

#include <optional>
#include <string_view>
#include <vector>

namespace Janus
{

class Scene;

struct ReflectedPropertySnapshot
{
    PropertyId property;
    PropertyValue value;
};

struct ReflectedComponentSnapshot
{
    ComponentTypeId component;
    std::vector<ReflectedPropertySnapshot> properties;
};

class SetPropertyCommand final : public ICommand
{
public:
    SetPropertyCommand(
        Scene& scene,
        SceneReflection reflection,
        UUID entity,
        ComponentTypeId component,
        PropertyId property,
        PropertyValue value);

    [[nodiscard]] Result<void> Execute() override;
    [[nodiscard]] Result<void> Undo() override;
    [[nodiscard]] Result<void> Redo() override;
    [[nodiscard]] std::string_view Describe() const noexcept override;

private:
    Scene& m_Scene;
    SceneReflection m_Reflection;
    UUID m_Entity;
    ComponentTypeId m_Component;
    PropertyId m_Property;
    PropertyValue m_Value;
    std::optional<PropertyMutationDelta> m_Delta;
};

class AddComponentCommand final : public ICommand
{
public:
    AddComponentCommand(
        Scene& scene,
        SceneReflection reflection,
        UUID entity,
        ComponentTypeId component);

    [[nodiscard]] Result<void> Execute() override;
    [[nodiscard]] Result<void> Undo() override;
    [[nodiscard]] Result<void> Redo() override;
    [[nodiscard]] std::string_view Describe() const noexcept override;

private:
    Scene& m_Scene;
    SceneReflection m_Reflection;
    UUID m_Entity;
    ComponentTypeId m_Component;
};

class RemoveComponentCommand final : public ICommand
{
public:
    RemoveComponentCommand(
        Scene& scene,
        SceneReflection reflection,
        UUID entity,
        ComponentTypeId component);

    [[nodiscard]] Result<void> Execute() override;
    [[nodiscard]] Result<void> Undo() override;
    [[nodiscard]] Result<void> Redo() override;
    [[nodiscard]] std::string_view Describe() const noexcept override;

private:
    [[nodiscard]] Result<ReflectedComponentSnapshot> CaptureSnapshot() const;
    [[nodiscard]] Result<void> RestoreSnapshot();

    Scene& m_Scene;
    SceneReflection m_Reflection;
    UUID m_Entity;
    ComponentTypeId m_Component;
    std::optional<ReflectedComponentSnapshot> m_Snapshot;
};

} // namespace Janus
