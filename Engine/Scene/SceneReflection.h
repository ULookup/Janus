#pragma once

#include "Core/Error/Result.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Core/UUID/UUID.h"

#include <vector>

namespace Janus
{

class AssetRegistry;
class Scene;

namespace SceneReflectionIds
{

inline constexpr ComponentTypeId Transform =
    MakeComponentTypeId("Transform");
inline constexpr ComponentTypeId SpriteRenderer =
    MakeComponentTypeId("SpriteRenderer");
inline constexpr ComponentTypeId Camera =
    MakeComponentTypeId("Camera");
inline constexpr ComponentTypeId LuaScript =
    MakeComponentTypeId("LuaScript");

inline constexpr PropertyId TransformPosition =
    MakePropertyId("Transform.position");
inline constexpr PropertyId TransformRotation =
    MakePropertyId("Transform.rotation");
inline constexpr PropertyId TransformScale =
    MakePropertyId("Transform.scale");

inline constexpr PropertyId SpriteTexture =
    MakePropertyId("SpriteRenderer.texture");
inline constexpr PropertyId SpriteSize =
    MakePropertyId("SpriteRenderer.size");
inline constexpr PropertyId SpriteColor =
    MakePropertyId("SpriteRenderer.color");
inline constexpr PropertyId SpriteLayer =
    MakePropertyId("SpriteRenderer.layer");
inline constexpr PropertyId SpriteUvMin =
    MakePropertyId("SpriteRenderer.uvMin");
inline constexpr PropertyId SpriteUvMax =
    MakePropertyId("SpriteRenderer.uvMax");
inline constexpr PropertyId SpriteEnabled =
    MakePropertyId("SpriteRenderer.enabled");

inline constexpr PropertyId CameraZoom =
    MakePropertyId("Camera.zoom");
inline constexpr PropertyId CameraPrimary =
    MakePropertyId("Camera.primary");

inline constexpr PropertyId LuaScriptAsset =
    MakePropertyId("LuaScript.script");
inline constexpr PropertyId LuaScriptEnabled =
    MakePropertyId("LuaScript.enabled");

} // namespace SceneReflectionIds

struct PropertyChange
{
    UUID entity;
    ComponentTypeId component;
    PropertyId property;
    PropertyValue before;
    PropertyValue after;
};

struct PropertyMutationDelta
{
    std::vector<PropertyChange> changes;
};

enum class PropertyMutationState
{
    Before,
    After
};

[[nodiscard]] Result<void> RegisterBuiltinSceneReflection(
    ReflectionRegistry& registry);

class SceneReflection final
{
public:
    explicit SceneReflection(
        const ReflectionRegistry& registry,
        const AssetRegistry* assets = nullptr) noexcept;

    [[nodiscard]] Result<bool> HasComponent(
        const Scene& scene,
        UUID entity,
        ComponentTypeId component) const;

    [[nodiscard]] Result<void> AddComponent(
        Scene& scene,
        UUID entity,
        ComponentTypeId component) const;

    [[nodiscard]] Result<void> RemoveComponent(
        Scene& scene,
        UUID entity,
        ComponentTypeId component) const;

    [[nodiscard]] Result<void> ValidateComponent(
        const Scene& scene,
        UUID entity,
        ComponentTypeId component) const;

    [[nodiscard]] Result<PropertyValue> GetProperty(
        const Scene& scene,
        UUID entity,
        ComponentTypeId component,
        PropertyId property) const;

    // Restores one local reflected field without applying contextual
    // authoring policy. Command undo/redo uses this through
    // RestorePropertyMutation after the complete delta is known.
    [[nodiscard]] Result<void> RestoreProperty(
        Scene& scene,
        UUID entity,
        ComponentTypeId component,
        PropertyId property,
        const PropertyValue& value) const;

    [[nodiscard]] Result<PropertyMutationDelta> ApplyPropertyMutation(
        Scene& scene,
        UUID entity,
        ComponentTypeId component,
        PropertyId property,
        const PropertyValue& value) const;

    [[nodiscard]] Result<void> RestorePropertyMutation(
        Scene& scene,
        const PropertyMutationDelta& delta,
        PropertyMutationState state) const;

private:
    const ReflectionRegistry& m_Registry;
    const AssetRegistry* m_Assets = nullptr;
};

} // namespace Janus
