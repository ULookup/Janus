#include "Scene/SceneReflection.h"

#include "Asset/AssetMetadata.h"
#include "Asset/AssetRegistry.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <string>
#include <utility>
#include <vector>

namespace Janus
{
namespace
{

using namespace SceneReflectionIds;

Result<ECS::Entity> ResolveEntity(
    const Scene& scene,
    UUID id)
{
    if (!id.IsValid())
    {
        return Result<ECS::Entity>::Failure(
            ErrorCode::InvalidArgument,
            "Scene reflection requires a valid entity UUID.");
    }

    const ECS::Entity entity = scene.FindEntity(id);
    if (!entity.IsValid())
    {
        return Result<ECS::Entity>::Failure(
            ErrorCode::EntityNotFound,
            "Scene reflection entity no longer exists.");
    }

    return Result<ECS::Entity>::Success(entity);
}

Result<const ComponentDescriptor*> RequireComponent(
    const ReflectionRegistry& registry,
    ComponentTypeId component)
{
    const auto* descriptor = registry.FindComponent(component);
    if (descriptor == nullptr)
    {
        return Result<const ComponentDescriptor*>::Failure(
            ErrorCode::InvalidArgument,
            "Unknown reflected Scene component.");
    }

    return Result<const ComponentDescriptor*>::Success(descriptor);
}

Result<const PropertyDescriptor*> RequireProperty(
    const ComponentDescriptor& component,
    PropertyId property)
{
    const auto* descriptor = component.FindProperty(property);
    if (descriptor == nullptr)
    {
        return Result<const PropertyDescriptor*>::Failure(
            ErrorCode::InvalidArgument,
            "Unknown reflected Scene property.");
    }

    return Result<const PropertyDescriptor*>::Success(descriptor);
}

Result<void*> GetMutableComponent(
    Scene& scene,
    ECS::Entity entity,
    ComponentTypeId component)
{
    if (component == Transform)
    {
        if (auto* value =
                scene.GetComponent<TransformComponent>(entity);
            value != nullptr)
        {
            return Result<void*>::Success(value);
        }
    }
    else if (component == SpriteRenderer)
    {
        if (auto* value =
                scene.GetComponent<SpriteRendererComponent>(entity);
            value != nullptr)
        {
            return Result<void*>::Success(value);
        }
    }
    else if (component == Camera)
    {
        if (auto* value =
                scene.GetComponent<CameraComponent>(entity);
            value != nullptr)
        {
            return Result<void*>::Success(value);
        }
    }
    else if (component == LuaScript)
    {
        if (auto* value =
                scene.GetComponent<LuaScriptComponent>(entity);
            value != nullptr)
        {
            return Result<void*>::Success(value);
        }
    }
    else
    {
        return Result<void*>::Failure(
            ErrorCode::InvalidArgument,
            "Reflected component has no Scene binding.");
    }

    return Result<void*>::Failure(
        ErrorCode::InvalidState,
        "Entity does not have the reflected component.");
}

Result<bool> HasBoundComponent(
    const Scene& scene,
    ECS::Entity entity,
    ComponentTypeId component)
{
    if (component == Transform)
    {
        return Result<bool>::Success(
            scene.HasComponent<TransformComponent>(entity));
    }
    if (component == SpriteRenderer)
    {
        return Result<bool>::Success(
            scene.HasComponent<SpriteRendererComponent>(entity));
    }
    if (component == Camera)
    {
        return Result<bool>::Success(
            scene.HasComponent<CameraComponent>(entity));
    }
    if (component == LuaScript)
    {
        return Result<bool>::Success(
            scene.HasComponent<LuaScriptComponent>(entity));
    }

    return Result<bool>::Failure(
        ErrorCode::InvalidArgument,
        "Reflected component has no Scene binding.");
}


Result<const void*> GetConstComponent(
    const Scene& scene,
    ECS::Entity entity,
    ComponentTypeId component)
{
    if (component == Transform)
    {
        if (const auto* value =
                scene.GetComponent<TransformComponent>(entity);
            value != nullptr)
        {
            return Result<const void*>::Success(value);
        }
    }
    else if (component == SpriteRenderer)
    {
        if (const auto* value =
                scene.GetComponent<SpriteRendererComponent>(entity);
            value != nullptr)
        {
            return Result<const void*>::Success(value);
        }
    }
    else if (component == Camera)
    {
        if (const auto* value =
                scene.GetComponent<CameraComponent>(entity);
            value != nullptr)
        {
            return Result<const void*>::Success(value);
        }
    }
    else if (component == LuaScript)
    {
        if (const auto* value =
                scene.GetComponent<LuaScriptComponent>(entity);
            value != nullptr)
        {
            return Result<const void*>::Success(value);
        }
    }
    else
    {
        return Result<const void*>::Failure(
            ErrorCode::InvalidArgument,
            "Reflected component has no Scene binding.");
    }

    return Result<const void*>::Failure(
        ErrorCode::InvalidState,
        "Entity does not have the reflected component.");
}

PropertyDescriptor TransformPositionDescriptor()
{
    return PropertyDescriptor{
        TransformPosition,
        "position",
        "position",
        PropertyType::Vector2,
        true,
        true,
        true,
        {},
        [](const void* component)
        {
            const auto* transform =
                static_cast<const TransformComponent*>(component);
            return Result<PropertyValue>::Success(
                PropertyValue{transform->position});
        },
        [](void* component, const PropertyValue& value)
        {
            auto* transform =
                static_cast<TransformComponent*>(component);
            transform->position = std::get<Vector2>(value);
            transform->dirty = true;
            return Result<void>::Success();
        }};
}

PropertyDescriptor TransformRotationDescriptor()
{
    return PropertyDescriptor{
        TransformRotation,
        "rotation",
        "rotation",
        PropertyType::Float32,
        true,
        true,
        true,
        {},
        [](const void* component)
        {
            const auto* transform =
                static_cast<const TransformComponent*>(component);
            return Result<PropertyValue>::Success(
                PropertyValue{transform->rotationRadians});
        },
        [](void* component, const PropertyValue& value)
        {
            auto* transform =
                static_cast<TransformComponent*>(component);
            transform->rotationRadians = std::get<f32>(value);
            transform->dirty = true;
            return Result<void>::Success();
        }};
}

PropertyDescriptor TransformScaleDescriptor()
{
    return PropertyDescriptor{
        TransformScale,
        "scale",
        "scale",
        PropertyType::Vector2,
        true,
        true,
        true,
        {},
        [](const void* component)
        {
            const auto* transform =
                static_cast<const TransformComponent*>(component);
            return Result<PropertyValue>::Success(
                PropertyValue{transform->scale});
        },
        [](void* component, const PropertyValue& value)
        {
            auto* transform =
                static_cast<TransformComponent*>(component);
            transform->scale = std::get<Vector2>(value);
            transform->dirty = true;
            return Result<void>::Success();
        }};
}

PropertyDescriptor SpriteTextureDescriptor()
{
    return PropertyDescriptor{
        SpriteTexture,
        "texture",
        "texture",
        PropertyType::AssetReference,
        true,
        true,
        true,
        "Texture",
        [](const void* component)
        {
            const auto* sprite =
                static_cast<const SpriteRendererComponent*>(component);
            return Result<PropertyValue>::Success(
                PropertyValue{
                    AssetReferenceValue{sprite->texture.id}});
        },
        [](void* component, const PropertyValue& value)
        {
            auto* sprite =
                static_cast<SpriteRendererComponent*>(component);
            sprite->texture = AssetHandle{
                std::get<AssetReferenceValue>(value).id};
            return Result<void>::Success();
        }};
}

PropertyDescriptor SpriteSizeDescriptor()
{
    return PropertyDescriptor{
        SpriteSize,
        "size",
        "size",
        PropertyType::Vector2,
        true,
        true,
        true,
        {},
        [](const void* component)
        {
            const auto* sprite =
                static_cast<const SpriteRendererComponent*>(component);
            return Result<PropertyValue>::Success(
                PropertyValue{sprite->size});
        },
        [](void* component, const PropertyValue& value)
        {
            auto* sprite =
                static_cast<SpriteRendererComponent*>(component);
            sprite->size = std::get<Vector2>(value);
            return Result<void>::Success();
        }};
}

PropertyDescriptor SpriteColorDescriptor()
{
    return PropertyDescriptor{
        SpriteColor,
        "color",
        "color",
        PropertyType::Color,
        true,
        true,
        true,
        {},
        [](const void* component)
        {
            const auto* sprite =
                static_cast<const SpriteRendererComponent*>(component);
            return Result<PropertyValue>::Success(
                PropertyValue{
                    ColorValue{
                        sprite->color.r,
                        sprite->color.g,
                        sprite->color.b,
                        sprite->color.a}});
        },
        [](void* component, const PropertyValue& value)
        {
            auto* sprite =
                static_cast<SpriteRendererComponent*>(component);
            const ColorValue color =
                std::get<ColorValue>(value);
            sprite->color = Color{
                color.r,
                color.g,
                color.b,
                color.a};
            return Result<void>::Success();
        }};
}

PropertyDescriptor SpriteLayerDescriptor()
{
    return PropertyDescriptor{
        SpriteLayer,
        "layer",
        "layer",
        PropertyType::Int32,
        true,
        true,
        true,
        {},
        [](const void* component)
        {
            const auto* sprite =
                static_cast<const SpriteRendererComponent*>(component);
            return Result<PropertyValue>::Success(
                PropertyValue{sprite->layer});
        },
        [](void* component, const PropertyValue& value)
        {
            auto* sprite =
                static_cast<SpriteRendererComponent*>(component);
            sprite->layer = std::get<i32>(value);
            return Result<void>::Success();
        }};
}

PropertyDescriptor SpriteUvMinDescriptor()
{
    return PropertyDescriptor{
        SpriteUvMin,
        "uvMin",
        "uvMin",
        PropertyType::Vector2,
        false,
        true,
        true,
        {},
        [](const void* component)
        {
            const auto* sprite =
                static_cast<const SpriteRendererComponent*>(component);
            return Result<PropertyValue>::Success(
                PropertyValue{sprite->uv.min});
        },
        [](void* component, const PropertyValue& value)
        {
            auto* sprite =
                static_cast<SpriteRendererComponent*>(component);
            sprite->uv.min = std::get<Vector2>(value);
            return Result<void>::Success();
        }};
}

PropertyDescriptor SpriteUvMaxDescriptor()
{
    return PropertyDescriptor{
        SpriteUvMax,
        "uvMax",
        "uvMax",
        PropertyType::Vector2,
        false,
        true,
        true,
        {},
        [](const void* component)
        {
            const auto* sprite =
                static_cast<const SpriteRendererComponent*>(component);
            return Result<PropertyValue>::Success(
                PropertyValue{sprite->uv.max});
        },
        [](void* component, const PropertyValue& value)
        {
            auto* sprite =
                static_cast<SpriteRendererComponent*>(component);
            sprite->uv.max = std::get<Vector2>(value);
            return Result<void>::Success();
        }};
}

PropertyDescriptor SpriteEnabledDescriptor()
{
    return PropertyDescriptor{
        SpriteEnabled,
        "enabled",
        "enabled",
        PropertyType::Bool,
        true,
        true,
        true,
        {},
        [](const void* component)
        {
            const auto* sprite =
                static_cast<const SpriteRendererComponent*>(component);
            return Result<PropertyValue>::Success(
                PropertyValue{sprite->enabled});
        },
        [](void* component, const PropertyValue& value)
        {
            auto* sprite =
                static_cast<SpriteRendererComponent*>(component);
            sprite->enabled = std::get<bool>(value);
            return Result<void>::Success();
        }};
}

PropertyDescriptor CameraZoomDescriptor()
{
    return PropertyDescriptor{
        CameraZoom,
        "zoom",
        "zoom",
        PropertyType::Float32,
        true,
        true,
        true,
        {},
        [](const void* component)
        {
            const auto* camera =
                static_cast<const CameraComponent*>(component);
            return Result<PropertyValue>::Success(
                PropertyValue{camera->zoom});
        },
        [](void* component, const PropertyValue& value)
        {
            auto* camera =
                static_cast<CameraComponent*>(component);
            camera->zoom = std::get<f32>(value);
            return Result<void>::Success();
        }};
}

PropertyDescriptor CameraPrimaryDescriptor()
{
    return PropertyDescriptor{
        CameraPrimary,
        "primary",
        "primary",
        PropertyType::Bool,
        true,
        true,
        true,
        {},
        [](const void* component)
        {
            const auto* camera =
                static_cast<const CameraComponent*>(component);
            return Result<PropertyValue>::Success(
                PropertyValue{camera->primary});
        },
        [](void* component, const PropertyValue& value)
        {
            auto* camera =
                static_cast<CameraComponent*>(component);
            camera->primary = std::get<bool>(value);
            return Result<void>::Success();
        }};
}

PropertyDescriptor LuaScriptAssetDescriptor()
{
    return PropertyDescriptor{
        LuaScriptAsset,
        "script",
        "script",
        PropertyType::AssetReference,
        true,
        true,
        true,
        "LuaScript",
        [](const void* component)
        {
            const auto* script =
                static_cast<const LuaScriptComponent*>(component);
            return Result<PropertyValue>::Success(
                PropertyValue{
                    AssetReferenceValue{script->script.id}});
        },
        [](void* component, const PropertyValue& value)
        {
            auto* script =
                static_cast<LuaScriptComponent*>(component);
            script->script = AssetHandle{
                std::get<AssetReferenceValue>(value).id};
            return Result<void>::Success();
        }};
}

PropertyDescriptor LuaScriptEnabledDescriptor()
{
    return PropertyDescriptor{
        LuaScriptEnabled,
        "enabled",
        "enabled",
        PropertyType::Bool,
        true,
        true,
        true,
        {},
        [](const void* component)
        {
            const auto* script =
                static_cast<const LuaScriptComponent*>(component);
            return Result<PropertyValue>::Success(
                PropertyValue{script->enabled});
        },
        [](void* component, const PropertyValue& value)
        {
            auto* script =
                static_cast<LuaScriptComponent*>(component);
            script->enabled = std::get<bool>(value);
            return Result<void>::Success();
        }};
}

Result<void> ValidateAssetReference(
    const AssetRegistry* assets,
    const PropertyDescriptor& property,
    const AssetReferenceValue& reference)
{
    if (!reference.id.IsValid())
    {
        return Result<void>::Success();
    }

    if (property.referenceConstraint.empty())
    {
        return Result<void>::Success();
    }

    if (assets == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Asset reference validation requires an AssetRegistry.");
    }

    const AssetHandle handle{reference.id};
    const AssetMetadata* metadata = assets->Find(handle);
    if (metadata == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::AssetNotFound,
            "Reflected asset reference is not registered.");
    }

    const auto expected =
        ParseAssetType(property.referenceConstraint);
    if (!expected)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Reflected asset constraint is not a known AssetType.");
    }

    if (metadata->type != expected.Value())
    {
        return Result<void>::Failure(
            ErrorCode::AssetTypeMismatch,
            "Reflected asset reference has the wrong AssetType.");
    }

    return Result<void>::Success();
}

Result<void> ApplyChanges(
    const SceneReflection& reflection,
    Scene& scene,
    const std::vector<PropertyChange>& changes,
    PropertyMutationState state)
{
    std::vector<PropertyValue> originalValues;
    originalValues.reserve(changes.size());

    for (const PropertyChange& change : changes)
    {
        auto original = reflection.GetProperty(
            scene,
            change.entity,
            change.component,
            change.property);
        if (!original)
        {
            return Result<void>::Failure(original.GetError());
        }

        originalValues.push_back(
            std::move(original).Value());
    }

    usize appliedCount = 0;
    for (; appliedCount < changes.size(); ++appliedCount)
    {
        const PropertyChange& change = changes[appliedCount];
        const PropertyValue& desired =
            state == PropertyMutationState::Before
                ? change.before
                : change.after;

        auto restored = reflection.RestoreProperty(
            scene,
            change.entity,
            change.component,
            change.property,
            desired);
        if (!restored)
        {
            while (appliedCount > 0)
            {
                --appliedCount;
                const PropertyChange& rollback =
                    changes[appliedCount];
                (void)reflection.RestoreProperty(
                    scene,
                    rollback.entity,
                    rollback.component,
                    rollback.property,
                    originalValues[appliedCount]);
            }

            return restored;
        }
    }

    return Result<void>::Success();
}

} // namespace

Result<void> RegisterBuiltinSceneReflection(
    ReflectionRegistry& registry)
{
    auto transform = registry.RegisterComponent(
        ComponentDescriptor{
            Transform,
            "Transform",
            "Transform",
            false,
            true,
            {
                TransformPositionDescriptor(),
                TransformRotationDescriptor(),
                TransformScaleDescriptor()}});
    if (!transform)
    {
        return transform;
    }

    auto sprite = registry.RegisterComponent(
        ComponentDescriptor{
            SpriteRenderer,
            "SpriteRenderer",
            "SpriteRenderer",
            true,
            true,
            {
                SpriteTextureDescriptor(),
                SpriteSizeDescriptor(),
                SpriteColorDescriptor(),
                SpriteLayerDescriptor(),
                SpriteUvMinDescriptor(),
                SpriteUvMaxDescriptor(),
                SpriteEnabledDescriptor()}});
    if (!sprite)
    {
        return sprite;
    }

    auto camera = registry.RegisterComponent(
        ComponentDescriptor{
            Camera,
            "Camera",
            "Camera",
            true,
            true,
            {
                CameraZoomDescriptor(),
                CameraPrimaryDescriptor()},
            [](const void* component)
            {
                const auto* camera =
                    static_cast<const CameraComponent*>(component);
                if (camera->zoom <= 0.0f)
                {
                    return Result<void>::Failure(
                        ErrorCode::InvalidArgument,
                        "Camera zoom must be positive.");
                }
                return Result<void>::Success();
            }});
    if (!camera)
    {
        return camera;
    }

    return registry.RegisterComponent(
        ComponentDescriptor{
            LuaScript,
            "LuaScript",
            "LuaScript",
            true,
            true,
            {
                LuaScriptAssetDescriptor(),
                LuaScriptEnabledDescriptor()},
            [](const void* component)
            {
                const auto* script =
                    static_cast<const LuaScriptComponent*>(component);
                if (script->enabled && !script->script.IsValid())
                {
                    return Result<void>::Failure(
                        ErrorCode::InvalidState,
                        "Enabled LuaScript requires a valid Script asset reference.");
                }
                return Result<void>::Success();
            }});
}

SceneReflection::SceneReflection(
    const ReflectionRegistry& registry,
    const AssetRegistry* assets) noexcept
    : m_Registry(registry),
      m_Assets(assets)
{
}

Result<bool> SceneReflection::HasComponent(
    const Scene& scene,
    UUID entityId,
    ComponentTypeId component) const
{
    auto descriptor = RequireComponent(m_Registry, component);
    if (!descriptor)
    {
        return Result<bool>::Failure(
            descriptor.GetError());
    }

    auto entity = ResolveEntity(scene, entityId);
    if (!entity)
    {
        return Result<bool>::Failure(entity.GetError());
    }

    return HasBoundComponent(
        scene,
        entity.Value(),
        component);
}

Result<void> SceneReflection::AddComponent(
    Scene& scene,
    UUID entityId,
    ComponentTypeId component) const
{
    auto descriptor = RequireComponent(m_Registry, component);
    if (!descriptor)
    {
        return Result<void>::Failure(
            descriptor.GetError());
    }

    auto entity = ResolveEntity(scene, entityId);
    if (!entity)
    {
        return Result<void>::Failure(entity.GetError());
    }

    auto present = HasBoundComponent(
        scene,
        entity.Value(),
        component);
    if (!present)
    {
        return Result<void>::Failure(present.GetError());
    }

    if (present.Value())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Entity already has the reflected component.");
    }

    if (!descriptor.Value()->removable)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Required reflected components cannot be added manually.");
    }

    void* added = nullptr;

    if (component == SpriteRenderer)
    {
        added = scene.AddComponent<SpriteRendererComponent>(
            entity.Value(),
            SpriteRendererComponent{});
    }
    else if (component == Camera)
    {
        added = scene.AddComponent<CameraComponent>(
            entity.Value(),
            CameraComponent{});
    }
    else if (component == LuaScript)
    {
        added = scene.AddComponent<LuaScriptComponent>(
            entity.Value(),
            LuaScriptComponent{AssetHandle{}, false});
    }
    else
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Reflected component cannot be added through Scene binding.");
    }

    if (added == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Failed to add reflected Scene component.");
    }

    return Result<void>::Success();
}

Result<void> SceneReflection::RemoveComponent(
    Scene& scene,
    UUID entityId,
    ComponentTypeId component) const
{
    auto descriptor = RequireComponent(m_Registry, component);
    if (!descriptor)
    {
        return Result<void>::Failure(
            descriptor.GetError());
    }

    if (!descriptor.Value()->removable)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Required reflected components cannot be removed.");
    }

    auto entity = ResolveEntity(scene, entityId);
    if (!entity)
    {
        return Result<void>::Failure(entity.GetError());
    }

    bool removed = false;

    if (component == SpriteRenderer)
    {
        removed = scene.RemoveComponent<SpriteRendererComponent>(
            entity.Value());
    }
    else if (component == Camera)
    {
        removed = scene.RemoveComponent<CameraComponent>(
            entity.Value());
    }
    else if (component == LuaScript)
    {
        removed = scene.RemoveComponent<LuaScriptComponent>(
            entity.Value());
    }
    else
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Reflected component cannot be removed through Scene binding.");
    }

    if (!removed)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Entity does not have the reflected component.");
    }

    return Result<void>::Success();
}

Result<void> SceneReflection::ValidateComponent(
    const Scene& scene,
    UUID entityId,
    ComponentTypeId component) const
{
    auto componentDescriptor =
        RequireComponent(m_Registry, component);
    if (!componentDescriptor)
    {
        return Result<void>::Failure(
            componentDescriptor.GetError());
    }

    auto entity = ResolveEntity(scene, entityId);
    if (!entity)
    {
        return Result<void>::Failure(entity.GetError());
    }

    auto componentValue = GetConstComponent(
        scene,
        entity.Value(),
        component);
    if (!componentValue)
    {
        return Result<void>::Failure(
            componentValue.GetError());
    }

    return componentDescriptor.Value()->Validate(
        componentValue.Value());
}

Result<PropertyValue> SceneReflection::GetProperty(
    const Scene& scene,
    UUID entityId,
    ComponentTypeId component,
    PropertyId property) const
{
    auto componentDescriptor =
        RequireComponent(m_Registry, component);
    if (!componentDescriptor)
    {
        return Result<PropertyValue>::Failure(
            componentDescriptor.GetError());
    }

    auto propertyDescriptor =
        RequireProperty(*componentDescriptor.Value(), property);
    if (!propertyDescriptor)
    {
        return Result<PropertyValue>::Failure(
            propertyDescriptor.GetError());
    }

    auto entity = ResolveEntity(scene, entityId);
    if (!entity)
    {
        return Result<PropertyValue>::Failure(
            entity.GetError());
    }

    auto componentValue = GetConstComponent(
        scene,
        entity.Value(),
        component);
    if (!componentValue)
    {
        return Result<PropertyValue>::Failure(
            componentValue.GetError());
    }

    return propertyDescriptor.Value()->Get(
        componentValue.Value());
}

Result<void> SceneReflection::RestoreProperty(
    Scene& scene,
    UUID entityId,
    ComponentTypeId component,
    PropertyId property,
    const PropertyValue& value) const
{
    auto componentDescriptor =
        RequireComponent(m_Registry, component);
    if (!componentDescriptor)
    {
        return Result<void>::Failure(
            componentDescriptor.GetError());
    }

    auto propertyDescriptor =
        RequireProperty(*componentDescriptor.Value(), property);
    if (!propertyDescriptor)
    {
        return Result<void>::Failure(
            propertyDescriptor.GetError());
    }

    auto entity = ResolveEntity(scene, entityId);
    if (!entity)
    {
        return Result<void>::Failure(entity.GetError());
    }

    auto componentValue = GetMutableComponent(
        scene,
        entity.Value(),
        component);
    if (!componentValue)
    {
        return Result<void>::Failure(
            componentValue.GetError());
    }

    return propertyDescriptor.Value()->Set(
        componentValue.Value(),
        value);
}

Result<PropertyMutationDelta> SceneReflection::ApplyPropertyMutation(
    Scene& scene,
    UUID entityId,
    ComponentTypeId component,
    PropertyId property,
    const PropertyValue& value) const
{
    auto componentDescriptor =
        RequireComponent(m_Registry, component);
    if (!componentDescriptor)
    {
        return Result<PropertyMutationDelta>::Failure(
            componentDescriptor.GetError());
    }

    auto propertyDescriptor =
        RequireProperty(*componentDescriptor.Value(), property);
    if (!propertyDescriptor)
    {
        return Result<PropertyMutationDelta>::Failure(
            propertyDescriptor.GetError());
    }

    if (!propertyDescriptor.Value()->editable)
    {
        return Result<PropertyMutationDelta>::Failure(
            ErrorCode::InvalidState,
            "Reflected property is not authoring-editable.");
    }

    if (GetPropertyType(value) !=
        propertyDescriptor.Value()->type)
    {
        return Result<PropertyMutationDelta>::Failure(
            ErrorCode::InvalidArgument,
            "Property mutation value has the wrong reflected type.");
    }

    auto entity = ResolveEntity(scene, entityId);
    if (!entity)
    {
        return Result<PropertyMutationDelta>::Failure(
            entity.GetError());
    }

    if (propertyDescriptor.Value()->type
        == PropertyType::AssetReference)
    {
        const auto& reference =
            std::get<AssetReferenceValue>(value);
        auto validated = ValidateAssetReference(
            m_Assets,
            *propertyDescriptor.Value(),
            reference);
        if (!validated)
        {
            return Result<PropertyMutationDelta>::Failure(
                validated.GetError());
        }
    }

    PropertyMutationDelta delta;

    if (component == Camera
        && property == CameraPrimary
        && std::get<bool>(value))
    {
        for (const ECS::Entity cameraEntity : scene.GetEntities())
        {
            if (cameraEntity == entity.Value())
            {
                continue;
            }

            const auto* camera =
                scene.GetComponent<CameraComponent>(cameraEntity);
            if (camera == nullptr || !camera->primary)
            {
                continue;
            }

            const auto* identity =
                scene.GetComponent<EntityIdentityComponent>(
                    cameraEntity);
            if (identity == nullptr)
            {
                return Result<PropertyMutationDelta>::Failure(
                    ErrorCode::InvalidState,
                    "Camera entity is missing persistent identity.");
            }

            delta.changes.push_back(
                PropertyChange{
                    identity->id,
                    Camera,
                    CameraPrimary,
                    PropertyValue{true},
                    PropertyValue{false}});
        }
    }

    auto before = GetProperty(
        scene,
        entityId,
        component,
        property);
    if (!before)
    {
        return Result<PropertyMutationDelta>::Failure(
            before.GetError());
    }

    delta.changes.push_back(
        PropertyChange{
            entityId,
            component,
            property,
            std::move(before).Value(),
            value});

    auto applied = ApplyChanges(
        *this,
        scene,
        delta.changes,
        PropertyMutationState::After);
    if (!applied)
    {
        return Result<PropertyMutationDelta>::Failure(
            applied.GetError());
    }

    auto valid = ValidateComponent(
        scene,
        entityId,
        component);
    if (!valid)
    {
        const auto rolledBack = RestorePropertyMutation(
            scene,
            delta,
            PropertyMutationState::Before);
        if (!rolledBack)
        {
            return Result<PropertyMutationDelta>::Failure(
                ErrorCode::InvalidState,
                "Property mutation validation failed and rollback also failed: "
                    + valid.GetError().message
                    + "; rollback: "
                    + rolledBack.GetError().message);
        }

        return Result<PropertyMutationDelta>::Failure(
            valid.GetError());
    }

    return Result<PropertyMutationDelta>::Success(
        std::move(delta));
}

Result<void> SceneReflection::RestorePropertyMutation(
    Scene& scene,
    const PropertyMutationDelta& delta,
    PropertyMutationState state) const
{
    return ApplyChanges(
        *this,
        scene,
        delta.changes,
        state);
}

} // namespace Janus
