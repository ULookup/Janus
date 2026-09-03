#include "Scene/SceneDeserializer.h"

#include "Asset/AssetHandle.h"
#include "Core/FileSystem/FileSystem.h"
#include "Core/UUID/UUID.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneMetadata.h"

#include <nlohmann/json.hpp>

#include <algorithm>
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

Result<Vector2> ParseVector2(
    const Json& value,
    std::string_view field)
{
    if (!value.is_array()
        || value.size() != 2
        || !value[0].is_number()
        || !value[1].is_number())
    {
        return Result<Vector2>::Failure(
            ErrorCode::InvalidArgument,
            "Scene field '" + std::string(field)
                + "' must be a two-number array.");
    }

    return Result<Vector2>::Success(Vector2{
        value[0].get<f32>(),
        value[1].get<f32>()});
}

Result<Color> ParseColor(const Json& value)
{
    if (!value.is_array() || value.size() != 4)
    {
        return Result<Color>::Failure(
            ErrorCode::InvalidArgument,
            "SpriteRenderer color must contain four numbers.");
    }

    for (const auto& channel : value)
    {
        if (!channel.is_number())
        {
            return Result<Color>::Failure(
                ErrorCode::InvalidArgument,
                "SpriteRenderer color must contain four numbers.");
        }
    }

    return Result<Color>::Success(Color{
        value[0].get<f32>(),
        value[1].get<f32>(),
        value[2].get<f32>(),
        value[3].get<f32>()});
}

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

    auto parsed = UUID::Parse(value.get<std::string>());
    if (!parsed || !parsed.Value().IsValid())
    {
        return Result<UUID>::Failure(
            ErrorCode::InvalidArgument,
            "Scene field '" + std::string(field)
                + "' contains an invalid UUID.");
    }

    return parsed;
}

Result<EntityRecord> ParseEntityRecord(const Json& entityJson)
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

    auto id = ParseRequiredUUID(entityJson["id"], "entity.id");
    if (!id)
    {
        return Result<EntityRecord>::Failure(id.GetError());
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
    record.name = entityJson["name"].get<std::string>();
    record.components = entityJson["components"];

    if (!entityJson["parent"].is_null())
    {
        auto parent = ParseRequiredUUID(entityJson["parent"], "entity.parent");
        if (!parent)
        {
            return Result<EntityRecord>::Failure(parent.GetError());
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

        const i64 order = entityJson["siblingOrder"].get<i64>();
        if (order < 0)
        {
            return Result<EntityRecord>::Failure(
                ErrorCode::InvalidArgument,
                "Scene entity siblingOrder must be a non-negative integer.");
        }
        record.siblingOrder = static_cast<usize>(order);
    }

    return Result<EntityRecord>::Success(std::move(record));
}

Result<void> RestoreComponents(
    Scene& scene,
    ECS::Entity entity,
    const Json& components)
{
    if (!components.contains("Transform")
        || !components["Transform"].is_object())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Scene entity is missing Transform authoring data.");
    }

    const Json& transformJson = components["Transform"];
    if (!transformJson.contains("position")
        || !transformJson.contains("rotation")
        || !transformJson.contains("scale")
        || !transformJson["rotation"].is_number())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Transform authoring data is incomplete or invalid.");
    }

    auto position = ParseVector2(transformJson["position"], "Transform.position");
    if (!position)
    {
        return Result<void>::Failure(position.GetError());
    }

    auto scale = ParseVector2(transformJson["scale"], "Transform.scale");
    if (!scale)
    {
        return Result<void>::Failure(scale.GetError());
    }

    auto* transform = scene.GetComponent<TransformComponent>(entity);
    transform->position = position.Value();
    transform->rotationRadians = transformJson["rotation"].get<f32>();
    transform->scale = scale.Value();
    transform->dirty = true;

    if (components.contains("Camera"))
    {
        const Json& cameraJson = components["Camera"];
        if (!cameraJson.is_object()
            || !cameraJson.contains("zoom")
            || !cameraJson.contains("primary")
            || !cameraJson["zoom"].is_number()
            || !cameraJson["primary"].is_boolean())
        {
            return Result<void>::Failure(
                ErrorCode::InvalidArgument,
                "Camera authoring data is incomplete or invalid.");
        }

        scene.AddComponent<CameraComponent>(
            entity,
            CameraComponent{
                cameraJson["zoom"].get<f32>(),
                cameraJson["primary"].get<bool>()});
    }

    if (components.contains("SpriteRenderer"))
    {
        const Json& spriteJson = components["SpriteRenderer"];
        if (!spriteJson.is_object()
            || !spriteJson.contains("texture")
            || !spriteJson.contains("size")
            || !spriteJson.contains("color")
            || !spriteJson.contains("layer")
            || !spriteJson.contains("uvMin")
            || !spriteJson.contains("uvMax")
            || !spriteJson.contains("enabled")
            || !spriteJson["layer"].is_number_integer()
            || !spriteJson["enabled"].is_boolean())
        {
            return Result<void>::Failure(
                ErrorCode::InvalidArgument,
                "SpriteRenderer authoring data is incomplete or invalid.");
        }

        AssetHandle texture;
        if (!spriteJson["texture"].is_null())
        {
            if (!spriteJson["texture"].is_string())
            {
                return Result<void>::Failure(
                    ErrorCode::InvalidArgument,
                    "SpriteRenderer texture must be an AssetHandle string or null.");
            }

            auto parsedTexture =
                AssetHandle::Parse(spriteJson["texture"].get<std::string>());
            if (!parsedTexture)
            {
                return Result<void>::Failure(
                    ErrorCode::InvalidArgument,
                    "SpriteRenderer texture contains an invalid AssetHandle.");
            }
            texture = parsedTexture.Value();
        }

        auto size = ParseVector2(spriteJson["size"], "SpriteRenderer.size");
        if (!size)
        {
            return Result<void>::Failure(size.GetError());
        }

        auto color = ParseColor(spriteJson["color"]);
        if (!color)
        {
            return Result<void>::Failure(color.GetError());
        }

        auto uvMin = ParseVector2(spriteJson["uvMin"], "SpriteRenderer.uvMin");
        if (!uvMin)
        {
            return Result<void>::Failure(uvMin.GetError());
        }

        auto uvMax = ParseVector2(spriteJson["uvMax"], "SpriteRenderer.uvMax");
        if (!uvMax)
        {
            return Result<void>::Failure(uvMax.GetError());
        }

        scene.AddComponent<SpriteRendererComponent>(
            entity,
            SpriteRendererComponent{
                texture,
                size.Value(),
                color.Value(),
                spriteJson["layer"].get<i32>(),
                TextureRegion{uvMin.Value(), uvMax.Value()},
                spriteJson["enabled"].get<bool>()});
    }

    return Result<void>::Success();
}

} // namespace

Result<std::unique_ptr<Scene>> SceneDeserializer::Deserialize(
    std::string_view text)
{
    const Json root = Json::parse(text.begin(), text.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object())
    {
        return Result<std::unique_ptr<Scene>>::Failure(
            ErrorCode::InvalidArgument,
            "Failed to parse Scene JSON.");
    }

    if (!root.contains("schema")
        || !root["schema"].is_string()
        || root["schema"].get<std::string>() != "janus.scene")
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

    auto sceneId = ParseRequiredUUID(root["scene"]["id"], "scene.id");
    if (!sceneId)
    {
        return Result<std::unique_ptr<Scene>>::Failure(sceneId.GetError());
    }

    std::vector<EntityRecord> records;
    records.reserve(root["entities"].size());
    std::unordered_set<UUID, UUIDHash> ids;

    for (const Json& entityJson : root["entities"])
    {
        auto record = ParseEntityRecord(entityJson);
        if (!record)
        {
            return Result<std::unique_ptr<Scene>>::Failure(record.GetError());
        }

        if (!ids.insert(record.Value().id).second)
        {
            return Result<std::unique_ptr<Scene>>::Failure(
                ErrorCode::InvalidArgument,
                "Scene contains duplicate entity UUID "
                    + record.Value().id.ToString() + ".");
        }

        records.push_back(std::move(record).Value());
    }

    auto scene = std::make_unique<Scene>(SceneMetadata{
        sceneId.Value(),
        root["scene"]["name"].get<std::string>()});

    std::vector<ECS::Entity> runtimeEntities;
    runtimeEntities.reserve(records.size());

    for (const EntityRecord& record : records)
    {
        auto created = scene->CreateEntityWithUUID(record.id, record.name);
        if (!created)
        {
            return Result<std::unique_ptr<Scene>>::Failure(created.GetError());
        }
        runtimeEntities.push_back(created.Value());
    }

    std::vector<PendingParent> pendingParents;

    for (usize index = 0; index < records.size(); ++index)
    {
        auto restored = RestoreComponents(
            *scene,
            runtimeEntities[index],
            records[index].components);
        if (!restored)
        {
            return Result<std::unique_ptr<Scene>>::Failure(restored.GetError());
        }

        if (records[index].parent.has_value())
        {
            pendingParents.push_back(PendingParent{
                runtimeEntities[index],
                records[index].id,
                *records[index].parent,
                records[index].siblingOrder});
        }
    }

    std::sort(
        pendingParents.begin(),
        pendingParents.end(),
        [](const PendingParent& left, const PendingParent& right)
        {
            if (left.parentId != right.parentId)
            {
                return left.parentId < right.parentId;
            }
            if (left.siblingOrder != right.siblingOrder)
            {
                return left.siblingOrder > right.siblingOrder;
            }
            return left.childId > right.childId;
        });

    for (const PendingParent& pending : pendingParents)
    {
        const ECS::Entity parent = scene->FindEntity(pending.parentId);
        if (!parent.IsValid())
        {
            return Result<std::unique_ptr<Scene>>::Failure(
                ErrorCode::EntityNotFound,
                "Scene references missing parent UUID "
                    + pending.parentId.ToString() + ".");
        }

        auto parentResult = scene->SetParent(pending.child, parent);
        if (!parentResult)
        {
            return Result<std::unique_ptr<Scene>>::Failure(
                parentResult.GetError());
        }
    }

    return Result<std::unique_ptr<Scene>>::Success(std::move(scene));
}

Result<std::unique_ptr<Scene>> SceneDeserializer::Load(
    const std::filesystem::path& path)
{
    auto text = FileSystem::ReadText(path);
    if (!text)
    {
        return Result<std::unique_ptr<Scene>>::Failure(text.GetError());
    }

    return Deserialize(text.Value());
}

} // namespace Janus
