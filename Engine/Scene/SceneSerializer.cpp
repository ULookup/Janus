#include "Scene/SceneSerializer.h"

#include "Core/FileSystem/FileSystem.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneReflection.h"

#include <nlohmann/json.hpp>

#include <string>

namespace Janus
{
namespace
{

using Json = nlohmann::json;

Result<Json> PropertyValueJson(
    const PropertyValue& value)
{
    switch (GetPropertyType(value))
    {
    case PropertyType::Bool:
        return Result<Json>::Success(
            Json(std::get<bool>(value)));

    case PropertyType::Int32:
        return Result<Json>::Success(
            Json(std::get<i32>(value)));

    case PropertyType::Float32:
        return Result<Json>::Success(
            Json(std::get<f32>(value)));

    case PropertyType::String:
        return Result<Json>::Success(
            Json(std::get<std::string>(value)));

    case PropertyType::Vector2:
    {
        const Vector2 vector = std::get<Vector2>(value);
        return Result<Json>::Success(
            Json::array({vector.x, vector.y}));
    }

    case PropertyType::Color:
    {
        const ColorValue color =
            std::get<ColorValue>(value);
        return Result<Json>::Success(
            Json::array(
                {color.r, color.g, color.b, color.a}));
    }

    case PropertyType::AssetReference:
    {
        const AssetReferenceValue reference =
            std::get<AssetReferenceValue>(value);

        if (!reference.id.IsValid())
        {
            return Result<Json>::Success(Json(nullptr));
        }

        return Result<Json>::Success(
            Json(reference.id.ToString()));
    }

    case PropertyType::Unknown:
    default:
        return Result<Json>::Failure(
            ErrorCode::InvalidState,
            "Cannot serialize an unknown reflected PropertyValue type.");
    }
}

usize SiblingOrder(
    const Scene& scene,
    ECS::Entity entity,
    const HierarchyComponent& hierarchy)
{
    if (!hierarchy.parent.IsValid())
    {
        return 0;
    }

    const auto* parentHierarchy =
        scene.GetComponent<HierarchyComponent>(
            hierarchy.parent);
    if (parentHierarchy == nullptr)
    {
        return 0;
    }

    usize order = 0;
    ECS::Entity child = parentHierarchy->firstChild;
    while (child.IsValid())
    {
        if (child == entity)
        {
            return order;
        }

        const auto* childHierarchy =
            scene.GetComponent<HierarchyComponent>(child);
        if (childHierarchy == nullptr)
        {
            break;
        }

        child = childHierarchy->nextSibling;
        ++order;
    }

    return order;
}

} // namespace

Result<std::string> SceneSerializer::Serialize(
    const Scene& scene,
    const ReflectionRegistry& reflectionRegistry)
{
    const SceneMetadata& metadata = scene.GetMetadata();
    if (!metadata.id.IsValid())
    {
        return Result<std::string>::Failure(
            ErrorCode::InvalidState,
            "Cannot serialize a Scene with a nil UUID.");
    }

    SceneReflection reflection(reflectionRegistry);

    Json root;
    root["schema"] = "janus.scene";
    root["version"] = 1;
    root["scene"] = Json{
        {"id", metadata.id.ToString()},
        {"name", metadata.name}};
    root["entities"] = Json::array();

    for (const ECS::Entity entity : scene.GetEntities())
    {
        const auto* identity =
            scene.GetComponent<EntityIdentityComponent>(
                entity);
        const auto* hierarchy =
            scene.GetComponent<HierarchyComponent>(
                entity);

        if (identity == nullptr || hierarchy == nullptr)
        {
            return Result<std::string>::Failure(
                ErrorCode::InvalidState,
                "Scene entity is missing required persistent structural components.");
        }

        Json entityJson;
        entityJson["id"] = identity->id.ToString();
        entityJson["name"] = identity->name;
        entityJson["parent"] = nullptr;
        entityJson["siblingOrder"] =
            SiblingOrder(scene, entity, *hierarchy);

        if (hierarchy->parent.IsValid())
        {
            const auto* parentIdentity =
                scene.GetComponent<EntityIdentityComponent>(
                    hierarchy->parent);
            if (parentIdentity == nullptr)
            {
                return Result<std::string>::Failure(
                    ErrorCode::InvalidState,
                    "Scene hierarchy references an entity without persistent identity.");
            }

            entityJson["parent"] =
                parentIdentity->id.ToString();
        }

        Json components = Json::object();

        for (const ComponentDescriptor* component :
             reflectionRegistry.GetComponents())
        {
            if (component == nullptr
                || !component->serializable)
            {
                continue;
            }

            auto present = reflection.HasComponent(
                scene,
                identity->id,
                component->id);
            if (!present)
            {
                return Result<std::string>::Failure(
                    present.GetError());
            }

            if (!present.Value())
            {
                if (!component->removable)
                {
                    return Result<std::string>::Failure(
                        ErrorCode::InvalidState,
                        "Scene entity is missing required reflected component '"
                            + component->serializedName
                            + "'.");
                }

                continue;
            }

            auto valid = reflection.ValidateComponent(
                scene,
                identity->id,
                component->id);
            if (!valid)
            {
                return Result<std::string>::Failure(
                    valid.GetError());
            }

            Json componentJson = Json::object();

            for (const PropertyDescriptor& property :
                 component->properties)
            {
                if (!property.serializable)
                {
                    continue;
                }

                auto value = reflection.GetProperty(
                    scene,
                    identity->id,
                    component->id,
                    property.id);
                if (!value)
                {
                    return Result<std::string>::Failure(
                        value.GetError());
                }

                auto jsonValue =
                    PropertyValueJson(value.Value());
                if (!jsonValue)
                {
                    return Result<std::string>::Failure(
                        jsonValue.GetError());
                }

                componentJson[property.serializedName] =
                    std::move(jsonValue).Value();
            }

            components[component->serializedName] =
                std::move(componentJson);
        }

        entityJson["components"] =
            std::move(components);
        root["entities"].push_back(
            std::move(entityJson));
    }

    return Result<std::string>::Success(
        root.dump(2) + "\n");
}

Result<void> SceneSerializer::Save(
    const Scene& scene,
    const ReflectionRegistry& reflection,
    const std::filesystem::path& path)
{
    auto serialized = Serialize(scene, reflection);
    if (!serialized)
    {
        return Result<void>::Failure(
            serialized.GetError());
    }

    return FileSystem::WriteTextAtomic(
        path,
        serialized.Value());
}

} // namespace Janus
