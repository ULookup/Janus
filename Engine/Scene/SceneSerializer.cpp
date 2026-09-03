#include "Scene/SceneSerializer.h"

#include "Core/FileSystem/FileSystem.h"
#include "Scene/Components.h"
#include "Scene/Hierarchy.h"
#include "Scene/Scene.h"

#include <nlohmann/json.hpp>

namespace Janus
{
namespace
{

using Json = nlohmann::json;

Json Vector2Json(Vector2 value)
{
    return Json::array({value.x, value.y});
}

Json ColorJson(Color value)
{
    return Json::array({value.r, value.g, value.b, value.a});
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
        scene.GetComponent<HierarchyComponent>(hierarchy.parent);
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

Result<std::string> SceneSerializer::Serialize(const Scene& scene)
{
    const SceneMetadata& metadata = scene.GetMetadata();
    if (!metadata.id.IsValid())
    {
        return Result<std::string>::Failure(
            ErrorCode::InvalidState,
            "Cannot serialize a Scene with a nil UUID.");
    }

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
            scene.GetComponent<EntityIdentityComponent>(entity);
        const auto* hierarchy =
            scene.GetComponent<HierarchyComponent>(entity);
        const auto* transform =
            scene.GetComponent<TransformComponent>(entity);

        if (identity == nullptr || hierarchy == nullptr || transform == nullptr)
        {
            return Result<std::string>::Failure(
                ErrorCode::InvalidState,
                "Scene entity is missing required persistent components.");
        }

        Json entityJson;
        entityJson["id"] = identity->id.ToString();
        entityJson["name"] = identity->name;
        entityJson["parent"] = nullptr;
        entityJson["siblingOrder"] = SiblingOrder(scene, entity, *hierarchy);

        if (hierarchy->parent.IsValid())
        {
            const auto* parentIdentity =
                scene.GetComponent<EntityIdentityComponent>(hierarchy->parent);
            if (parentIdentity == nullptr)
            {
                return Result<std::string>::Failure(
                    ErrorCode::InvalidState,
                    "Scene hierarchy references an entity without persistent identity.");
            }
            entityJson["parent"] = parentIdentity->id.ToString();
        }

        Json components;
        components["Transform"] = Json{
            {"position", Vector2Json(transform->position)},
            {"rotation", transform->rotationRadians},
            {"scale", Vector2Json(transform->scale)}};

        if (const auto* sprite =
                scene.GetComponent<SpriteRendererComponent>(entity);
            sprite != nullptr)
        {
            Json spriteJson{
                {"size", Vector2Json(sprite->size)},
                {"color", ColorJson(sprite->color)},
                {"layer", sprite->layer},
                {"uvMin", Vector2Json(sprite->uv.min)},
                {"uvMax", Vector2Json(sprite->uv.max)},
                {"enabled", sprite->enabled}};

            spriteJson["texture"] = sprite->texture.IsValid()
                ? Json(sprite->texture.ToString())
                : Json(nullptr);
            components["SpriteRenderer"] = std::move(spriteJson);
        }

        if (const auto* camera = scene.GetComponent<CameraComponent>(entity);
            camera != nullptr)
        {
            components["Camera"] = Json{
                {"zoom", camera->zoom},
                {"primary", camera->primary}};
        }

        if (const auto* script = scene.GetComponent<LuaScriptComponent>(entity);
            script != nullptr)
        {
            if (script->enabled && !script->script.IsValid())
            {
                return Result<std::string>::Failure(
                    ErrorCode::InvalidState,
                    "Enabled LuaScript component requires a valid Script AssetHandle.");
            }

            Json scriptJson{{"enabled", script->enabled}};
            scriptJson["script"] = script->script.IsValid()
                ? Json(script->script.ToString())
                : Json(nullptr);
            components["LuaScript"] = std::move(scriptJson);
        }

        entityJson["components"] = std::move(components);
        root["entities"].push_back(std::move(entityJson));
    }

    return Result<std::string>::Success(root.dump(2) + "\n");
}

Result<void> SceneSerializer::Save(
    const Scene& scene,
    const std::filesystem::path& path)
{
    auto serialized = Serialize(scene);
    if (!serialized)
    {
        return Result<void>::Failure(serialized.GetError());
    }

    return FileSystem::WriteTextAtomic(path, serialized.Value());
}

} // namespace Janus
