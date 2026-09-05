#include "Scene/SceneDeserializer.h"

#include "Core/FileSystem/FileSystem.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Core/UUID/UUID.h"
#include "Scene/Scene.h"
#include "Scene/SceneMetadata.h"
#include "Scene/SceneReflection.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Janus
{
namespace
{

using Json = nlohmann::json;

struct EntityRecord
{
    UUID id;
    std::string name;
    std::optional<UUID> parent;
    usize siblingOrder = 0;
    Json components;
};

struct PendingParent
{
    ECS::Entity child;
    UUID childId;
    UUID parentId;
    usize siblingOrder = 0;
};

Result<UUID> ParseRequiredUUID(
    const Json& value,
    std::string_view field)
{
    if (!value.is_string())
    {
        return Result<UUID>::Failure(
            ErrorCode::InvalidArgument,
            "Scene field '" + std::string(field)
                + "' must be a UUID string.");
    }

    auto parsed =
        UUID::Parse(value.get<std::string>());
    if (!parsed || !parsed.Value().IsValid())
    {
        return Result<UUID>::Failure(
            ErrorCode::InvalidArgument,
            "Scene field '" + std::string(field)
                + "' contains an invalid UUID.");
    }

    return parsed;
}

Result<PropertyValue> ParsePropertyValue(
    const Json& value,
    PropertyType type,
    std::string_view field)
{
    const std::string label(field);

    switch (type)
    {
    case PropertyType::Bool:
        if (!value.is_boolean())
        {
            break;
        }
        return Result<PropertyValue>::Success(
            PropertyValue{value.get<bool>()});

    case PropertyType::Int32:
        if (value.is_number_integer())
        {
            const i64 integer = value.get<i64>();
            if (integer >= std::numeric_limits<i32>::min()
                && integer <= std::numeric_limits<i32>::max())
            {
                return Result<PropertyValue>::Success(
                    PropertyValue{
                        static_cast<i32>(integer)});
            }
        }
        break;

    case PropertyType::Float32:
        if (!value.is_number())
        {
            break;
        }
        return Result<PropertyValue>::Success(
            PropertyValue{value.get<f32>()});

    case PropertyType::String:
        if (!value.is_string())
        {
            break;
        }
        return Result<PropertyValue>::Success(
            PropertyValue{
                value.get<std::string>()});

    case PropertyType::Vector2:
        if (value.is_array()
            && value.size() == 2
            && value[0].is_number()
            && value[1].is_number())
        {
            return Result<PropertyValue>::Success(
                PropertyValue{
                    Vector2{
                        value[0].get<f32>(),
                        value[1].get<f32>()}});
        }
        break;

    case PropertyType::Color:
        if (value.is_array()
            && value.size() == 4
            && value[0].is_number()
            && value[1].is_number()
            && value[2].is_number()
            && value[3].is_number())
        {
            return Result<PropertyValue>::Success(
                PropertyValue{
                    ColorValue{
                        value[0].get<f32>(),
                        value[1].get<f32>(),
                        value[2].get<f32>(),
                        value[3].get<f32>()}});
        }
        break;

    case PropertyType::AssetReference:
        if (value.is_null())
        {
            return Result<PropertyValue>::Success(
                PropertyValue{
                    AssetReferenceValue{}});
        }

        if (value.is_string())
        {
            auto parsed =
                UUID::Parse(value.get<std::string>());
            if (parsed && parsed.Value().IsValid())
            {
                return Result<PropertyValue>::Success(
                    PropertyValue{
                        AssetReferenceValue{
                            parsed.Value()}});
            }
        }
        break;

    case PropertyType::Unknown:
    default:
        return Result<PropertyValue>::Failure(
            ErrorCode::InvalidState,
            "Scene field '" + label
                + "' uses an unsupported reflected property type.");
    }

    return Result<PropertyValue>::Failure(
        ErrorCode::InvalidArgument,
        "Scene field '" + label
            + "' has an invalid reflected value.");
}

Result<EntityRecord> ParseEntityRecord(
    const Json& entityJson)
{
    if (!entityJson.is_object())
    {
        return Result<EntityRecord>::Failure(
            ErrorCode::InvalidArgument,
            "Scene entities must be JSON objects.");
    }

    if (!entityJson.contains("id")
        || !entityJson.contains("name")
        || !entityJson.contains("parent")
        || !entityJson.contains("components"))
    {
        return Result<EntityRecord>::Failure(
            ErrorCode::InvalidArgument,
            "Scene entity is missing required fields.");
    }

    auto id =
        ParseRequiredUUID(
            entityJson["id"],
            "entity.id");
    if (!id)
    {
        return Result<EntityRecord>::Failure(
            id.GetError());
    }

    if (!entityJson["name"].is_string()
        || !entityJson["components"].is_object())
    {
        return Result<EntityRecord>::Failure(
            ErrorCode::InvalidArgument,
            "Scene entity name/components have invalid types.");
    }

    EntityRecord record;
    record.id = id.Value();
    record.name =
        entityJson["name"].get<std::string>();
    record.components =
        entityJson["components"];

    if (!entityJson["parent"].is_null())
    {
        auto parent =
            ParseRequiredUUID(
                entityJson["parent"],
                "entity.parent");
        if (!parent)
        {
            return Result<EntityRecord>::Failure(
                parent.GetError());
        }

        record.parent = parent.Value();
    }

    if (entityJson.contains("siblingOrder"))
    {
        if (!entityJson["siblingOrder"].is_number_integer())
        {
            return Result<EntityRecord>::Failure(
                ErrorCode::InvalidArgument,
                "Scene entity siblingOrder must be a non-negative integer.");
        }

        const i64 order =
            entityJson["siblingOrder"].get<i64>();
        if (order < 0)
        {
            return Result<EntityRecord>::Failure(
                ErrorCode::InvalidArgument,
                "Scene entity siblingOrder must be a non-negative integer.");
        }

        record.siblingOrder =
            static_cast<usize>(order);
    }

    return Result<EntityRecord>::Success(
        std::move(record));
}

Result<void> RestoreComponents(
    Scene& scene,
    ECS::Entity entity,
    UUID entityId,
    const Json& components,
    const ReflectionRegistry& reflectionRegistry)
{
    if (!components.is_object())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Scene components must be a JSON object.");
    }

    SceneReflection reflection(
        reflectionRegistry);

    for (auto item = components.begin();
         item != components.end();
         ++item)
    {
        const ComponentDescriptor* component =
            reflectionRegistry.FindComponentBySerializedName(
                item.key());

        if (component == nullptr
            || !component->serializable)
        {
            return Result<void>::Failure(
                ErrorCode::InvalidArgument,
                "Scene contains unknown reflected component '"
                    + item.key() + "'.");
        }
    }

    for (const ComponentDescriptor* component :
         reflectionRegistry.GetComponents())
    {
        if (component == nullptr
            || !component->serializable)
        {
            continue;
        }

        const auto componentJson =
            components.find(component->serializedName);

        if (componentJson == components.end())
        {
            if (!component->removable)
            {
                return Result<void>::Failure(
                    ErrorCode::InvalidArgument,
                    "Scene entity is missing required reflected component '"
                        + component->serializedName
                        + "'.");
            }

            continue;
        }

        if (!componentJson->is_object())
        {
            return Result<void>::Failure(
                ErrorCode::InvalidArgument,
                "Scene reflected component '"
                    + component->serializedName
                    + "' must be a JSON object.");
        }

        auto present = reflection.HasComponent(
            scene,
            entityId,
            component->id);
        if (!present)
        {
            return Result<void>::Failure(
                present.GetError());
        }

        if (!present.Value())
        {
            auto added = reflection.AddComponent(
                scene,
                entityId,
                component->id);
            if (!added)
            {
                return added;
            }
        }

        for (auto propertyJson =
                 componentJson->begin();
             propertyJson != componentJson->end();
             ++propertyJson)
        {
            const PropertyDescriptor* property =
                component->FindPropertyBySerializedName(
                    propertyJson.key());

            if (property == nullptr
                || !property->serializable)
            {
                return Result<void>::Failure(
                    ErrorCode::InvalidArgument,
                    "Scene component '"
                        + component->serializedName
                        + "' contains unknown reflected property '"
                        + propertyJson.key() + "'.");
            }
        }

        for (const PropertyDescriptor& property :
             component->properties)
        {
            if (!property.serializable)
            {
                continue;
            }

            const auto propertyJson =
                componentJson->find(
                    property.serializedName);
            if (propertyJson == componentJson->end())
            {
                return Result<void>::Failure(
                    ErrorCode::InvalidArgument,
                    "Scene component '"
                        + component->serializedName
                        + "' is missing reflected property '"
                        + property.serializedName + "'.");
            }

            const std::string field =
                component->serializedName + "."
                + property.serializedName;

            auto value = ParsePropertyValue(
                *propertyJson,
                property.type,
                field);
            if (!value)
            {
                return Result<void>::Failure(
                    value.GetError());
            }

            auto restored =
                reflection.RestoreProperty(
                    scene,
                    entityId,
                    component->id,
                    property.id,
                    value.Value());
            if (!restored)
            {
                return restored;
            }
        }

        auto valid = reflection.ValidateComponent(
            scene,
            entityId,
            component->id);
        if (!valid)
        {
            return valid;
        }
    }

    (void)entity;
    return Result<void>::Success();
}

} // namespace

Result<std::unique_ptr<Scene>>
SceneDeserializer::Deserialize(
    std::string_view text,
    const ReflectionRegistry& reflection)
{
    const Json root =
        Json::parse(
            text.begin(),
            text.end(),
            nullptr,
            false);
    if (root.is_discarded()
        || !root.is_object())
    {
        return Result<std::unique_ptr<Scene>>::Failure(
            ErrorCode::InvalidArgument,
            "Failed to parse Scene JSON.");
    }

    if (!root.contains("schema")
        || !root["schema"].is_string()
        || root["schema"].get<std::string>()
            != "janus.scene")
    {
        return Result<std::unique_ptr<Scene>>::Failure(
            ErrorCode::InvalidArgument,
            "Unsupported Scene schema.");
    }

    if (!root.contains("version")
        || !root["version"].is_number_integer()
        || root["version"].get<i32>() != 1)
    {
        return Result<std::unique_ptr<Scene>>::Failure(
            ErrorCode::InvalidArgument,
            "Unsupported Scene schema version.");
    }

    if (!root.contains("scene")
        || !root["scene"].is_object()
        || !root["scene"].contains("id")
        || !root["scene"].contains("name")
        || !root["scene"]["name"].is_string()
        || !root.contains("entities")
        || !root["entities"].is_array())
    {
        return Result<std::unique_ptr<Scene>>::Failure(
            ErrorCode::InvalidArgument,
            "Scene metadata or entity array is missing or invalid.");
    }

    auto sceneId =
        ParseRequiredUUID(
            root["scene"]["id"],
            "scene.id");
    if (!sceneId)
    {
        return Result<std::unique_ptr<Scene>>::Failure(
            sceneId.GetError());
    }

    std::vector<EntityRecord> records;
    records.reserve(root["entities"].size());

    std::unordered_set<UUID, UUIDHash> ids;

    for (const Json& entityJson :
         root["entities"])
    {
        auto record =
            ParseEntityRecord(entityJson);
        if (!record)
        {
            return Result<std::unique_ptr<Scene>>::Failure(
                record.GetError());
        }

        if (!ids.insert(record.Value().id).second)
        {
            return Result<std::unique_ptr<Scene>>::Failure(
                ErrorCode::InvalidArgument,
                "Scene contains duplicate entity UUID "
                    + record.Value().id.ToString()
                    + ".");
        }

        records.push_back(
            std::move(record).Value());
    }

    auto scene = std::make_unique<Scene>(
        SceneMetadata{
            sceneId.Value(),
            root["scene"]["name"]
                .get<std::string>()});

    std::vector<ECS::Entity> runtimeEntities;
    runtimeEntities.reserve(records.size());

    for (const EntityRecord& record : records)
    {
        auto created =
            scene->CreateEntityWithUUID(
                record.id,
                record.name);
        if (!created)
        {
            return Result<std::unique_ptr<Scene>>::Failure(
                created.GetError());
        }

        runtimeEntities.push_back(
            created.Value());
    }

    std::vector<PendingParent> pendingParents;

    for (usize index = 0;
         index < records.size();
         ++index)
    {
        auto restored = RestoreComponents(
            *scene,
            runtimeEntities[index],
            records[index].id,
            records[index].components,
            reflection);
        if (!restored)
        {
            return Result<std::unique_ptr<Scene>>::Failure(
                restored.GetError());
        }

        if (records[index].parent.has_value())
        {
            pendingParents.push_back(
                PendingParent{
                    runtimeEntities[index],
                    records[index].id,
                    *records[index].parent,
                    records[index].siblingOrder});
        }
    }

    std::sort(
        pendingParents.begin(),
        pendingParents.end(),
        [](const PendingParent& left,
           const PendingParent& right)
        {
            if (left.parentId != right.parentId)
            {
                return left.parentId
                    < right.parentId;
            }

            if (left.siblingOrder
                != right.siblingOrder)
            {
                return left.siblingOrder
                    > right.siblingOrder;
            }

            return left.childId
                > right.childId;
        });

    for (const PendingParent& pending :
         pendingParents)
    {
        const ECS::Entity parent =
            scene->FindEntity(
                pending.parentId);
        if (!parent.IsValid())
        {
            return Result<std::unique_ptr<Scene>>::Failure(
                ErrorCode::EntityNotFound,
                "Scene references missing parent UUID "
                    + pending.parentId.ToString()
                    + ".");
        }

        auto parentResult =
            scene->SetParent(
                pending.child,
                parent);
        if (!parentResult)
        {
            return Result<std::unique_ptr<Scene>>::Failure(
                parentResult.GetError());
        }
    }

    return Result<std::unique_ptr<Scene>>::Success(
        std::move(scene));
}

Result<std::unique_ptr<Scene>>
SceneDeserializer::Load(
    const std::filesystem::path& path,
    const ReflectionRegistry& reflection)
{
    auto text = FileSystem::ReadText(path);
    if (!text)
    {
        return Result<std::unique_ptr<Scene>>::Failure(
            text.GetError());
    }

    return Deserialize(
        text.Value(),
        reflection);
}

} // namespace Janus
