#include "Resources/SceneResources.h"

#include "Asset/AssetRegistry.h"
#include "Registry/ResourceRegistry.h"
#include "Schema/ReflectionJsonCodec.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneReflection.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <variant>

namespace
{

Janus::MCP::Json RequireJsonResult(
    const Janus::MCP::McpDispatchResult& result)
{
    REQUIRE(std::holds_alternative<Janus::MCP::Json>(result));
    return std::get<Janus::MCP::Json>(result);
}

Janus::MCP::McpDispatchError RequireErrorResult(
    const Janus::MCP::McpDispatchResult& result)
{
    REQUIRE(std::holds_alternative<Janus::MCP::McpDispatchError>(result));
    return std::get<Janus::MCP::McpDispatchError>(result);
}

Janus::MCP::Json ReadPayload(
    Janus::MCP::ResourceRegistry& registry,
    std::string uri,
    Janus::MCP::McpProtocolEra era =
        Janus::MCP::McpProtocolEra::Modern2026)
{
    const Janus::MCP::Json result =
        RequireJsonResult(
            registry.HandleRead(
                Janus::MCP::Json{
                    {"uri", std::move(uri)}},
                era));

    REQUIRE(result.at("contents").size() == 1);
    REQUIRE(
        result.at("contents").at(0).at("mimeType")
        == "application/json");

    return Janus::MCP::Json::parse(
        result.at("contents")
            .at(0)
            .at("text")
            .get<std::string>());
}

const Janus::MCP::Json* FindHierarchyEntity(
    const Janus::MCP::Json& hierarchy,
    std::string_view uuid)
{
    for (const Janus::MCP::Json& entity
         : hierarchy.at("entities"))
    {
        if (entity.at("uuid") == std::string{uuid})
        {
            return &entity;
        }
    }

    return nullptr;
}

} // namespace

TEST_CASE(
    "Reflection JSON codec uses MCP object shapes and nullable asset references",
    "[mcp][resource][codec][v0.8]")
{
    using namespace Janus;
    using namespace Janus::MCP;

    const auto vector =
        PropertyValueToMcpJson(
            PropertyValue{
                Vector2{2.0f, 3.0f}});
    REQUIRE(vector);
    REQUIRE(vector.Value().at("x") == 2.0f);
    REQUIRE(vector.Value().at("y") == 3.0f);

    const auto color =
        PropertyValueToMcpJson(
            PropertyValue{
                ColorValue{
                    0.1f,
                    0.2f,
                    0.3f,
                    0.4f}});
    REQUIRE(color);
    REQUIRE(color.Value().at("a") == 0.4f);

    const auto nilReference =
        PropertyValueToMcpJson(
            PropertyValue{
                AssetReferenceValue{UUID{}}});
    REQUIRE(nilReference);
    REQUIRE(nilReference.Value().is_null());

    const UUID id = UUID::Random();
    const auto reference =
        PropertyValueToMcpJson(
            PropertyValue{
                AssetReferenceValue{id}});
    REQUIRE(reference);
    REQUIRE(reference.Value() == id.ToString());

    REQUIRE(
        McpPropertyTypeName(
            PropertyType::AssetReference)
        == "asset-reference");
}

TEST_CASE(
    "Scene MCP resources expose project Scene hierarchy entity and asset authoring state",
    "[mcp][resource][scene][v0.8]")
{
    using namespace Janus;
    using namespace Janus::MCP;

    auto reflectionResult =
        CreateBuiltinSceneReflectionRegistry();
    REQUIRE(reflectionResult);
    ReflectionRegistry reflection =
        std::move(reflectionResult).Value();

    Scene scene{
        SceneMetadata{
            UUID::Random(),
            "Battle"}};

    const ECS::Entity root =
        scene.CreateEntity("Root");
    const ECS::Entity childA =
        scene.CreateEntity("ChildA");
    const ECS::Entity childB =
        scene.CreateEntity("ChildB");

    REQUIRE(
        scene.SetParent(
            childA,
            root));
    REQUIRE(
        scene.SetParent(
            childB,
            root));

    const auto* rootIdentity =
        scene.GetComponent<EntityIdentityComponent>(
            root);
    const auto* childAIdentity =
        scene.GetComponent<EntityIdentityComponent>(
            childA);
    const auto* childBIdentity =
        scene.GetComponent<EntityIdentityComponent>(
            childB);

    REQUIRE(rootIdentity != nullptr);
    REQUIRE(childAIdentity != nullptr);
    REQUIRE(childBIdentity != nullptr);

    AssetRegistry assets;
    const AssetHandle texture =
        AssetHandle::Random();

    REQUIRE(
        assets.Register(
            AssetMetadata{
                texture,
                AssetType::Texture,
                "Textures/player.png"}));

    auto* sprite =
        scene.AddComponent<SpriteRendererComponent>(
            root,
            SpriteRendererComponent{});
    REQUIRE(sprite != nullptr);
    sprite->texture = texture;
    sprite->size = Vector2{2.0f, 3.0f};

    ResourceRegistry resources;

    REQUIRE(
        RegisterSceneResources(
            resources,
            McpSceneResourceContext{
                &scene,
                &reflection,
                &assets,
                []()
                {
                    return Result<McpProjectReadState>::Success(
                        McpProjectReadState{
                            "SandboxProject",
                            true,
                            false});
                }}));

    REQUIRE(resources.GetResourceCount() == 3);
    REQUIRE(resources.GetTemplateCount() == 2);

    const Json project =
        ReadPayload(
            resources,
            "engine://project/info");

    REQUIRE(
        project.at("project")
            .at("displayPath")
        == "SandboxProject");
    REQUIRE(
        project.at("scene")
            .at("uuid")
        == scene.GetMetadata().id.ToString());
    REQUIRE(
        project.at("authoring")
            .at("dirty")
        == true);
    REQUIRE(
        project.at("authoring")
            .at("readOnly")
        == false);
    REQUIRE(
        project.at("assets")
            .at("count")
        == 1);

    const Json current =
        ReadPayload(
            resources,
            "engine://scene/current");

    REQUIRE(current.at("name") == "Battle");
    REQUIRE(current.at("entityCount") == 3);

    bool foundTransform = false;
    bool foundSprite = false;

    for (const Json& component
         : current.at("components"))
    {
        const std::string name =
            component.at("name")
                .get<std::string>();

        foundTransform =
            foundTransform
            || name == "Transform";
        foundSprite =
            foundSprite
            || name == "SpriteRenderer";
    }

    REQUIRE(foundTransform);
    REQUIRE(foundSprite);

    const Json hierarchy =
        ReadPayload(
            resources,
            "engine://scene/hierarchy");

    REQUIRE(hierarchy.at("roots").size() == 1);
    REQUIRE(
        hierarchy.at("roots").at(0)
        == rootIdentity->id.ToString());

    const Json* rootNode =
        FindHierarchyEntity(
            hierarchy,
            rootIdentity->id.ToString());
    const Json* childANode =
        FindHierarchyEntity(
            hierarchy,
            childAIdentity->id.ToString());
    const Json* childBNode =
        FindHierarchyEntity(
            hierarchy,
            childBIdentity->id.ToString());

    REQUIRE(rootNode != nullptr);
    REQUIRE(childANode != nullptr);
    REQUIRE(childBNode != nullptr);

    REQUIRE(rootNode->at("parent").is_null());
    REQUIRE(rootNode->at("children").size() == 2);

    REQUIRE(
        rootNode->at("children").at(0)
        == childBIdentity->id.ToString());
    REQUIRE(
        rootNode->at("children").at(1)
        == childAIdentity->id.ToString());

    REQUIRE(
        childBNode->at("siblingOrder")
        == 0);
    REQUIRE(
        childANode->at("siblingOrder")
        == 1);

    const Json entity =
        ReadPayload(
            resources,
            "engine://entity/"
                + rootIdentity->id.ToString());

    REQUIRE(entity.at("name") == "Root");
    REQUIRE(
        entity.at("components")
            .at("Transform")
            .at("position")
            .at("x")
        == 0.0f);
    REQUIRE_FALSE(
        entity.at("components")
            .at("Transform")
            .contains("worldPosition"));
    REQUIRE_FALSE(
        entity.at("components")
            .at("Transform")
            .contains("dirty"));
    REQUIRE(
        entity.at("components")
            .at("SpriteRenderer")
            .at("texture")
        == texture.ToString());
    REQUIRE(
        entity.at("components")
            .at("SpriteRenderer")
            .at("size")
            .at("x")
        == 2.0f);

    const Json asset =
        ReadPayload(
            resources,
            "engine://asset/"
                + texture.ToString());

    REQUIRE(asset.at("uuid") == texture.ToString());
    REQUIRE(asset.at("type") == "texture");
    REQUIRE(
        asset.at("path")
        == "Textures/player.png");
}

TEST_CASE(
    "Scene MCP resources reject malformed and missing persistent UUIDs",
    "[mcp][resource][scene][v0.8]")
{
    using namespace Janus;
    using namespace Janus::MCP;

    auto reflectionResult =
        CreateBuiltinSceneReflectionRegistry();
    REQUIRE(reflectionResult);
    ReflectionRegistry reflection =
        std::move(reflectionResult).Value();

    Scene scene;
    AssetRegistry assets;
    ResourceRegistry resources;

    REQUIRE(
        RegisterSceneResources(
            resources,
            McpSceneResourceContext{
                &scene,
                &reflection,
                &assets,
                []()
                {
                    return Result<McpProjectReadState>::Success(
                        McpProjectReadState{
                            "Project",
                            false,
                            false});
                }}));

    const auto malformed =
        resources.HandleRead(
            Json{
                {"uri",
                 "engine://entity/not-a-uuid"}},
            McpProtocolEra::Modern2026);

    REQUIRE(
        RequireErrorResult(malformed).code
        == JsonRpcInvalidParams);

    const UUID missingEntity =
        UUID::Random();

    const auto missing =
        resources.HandleRead(
            Json{
                {"uri",
                 "engine://entity/"
                     + missingEntity.ToString()}},
            McpProtocolEra::Modern2026);

    REQUIRE(
        RequireErrorResult(missing).code
        == JsonRpcInvalidParams);

    const UUID missingAsset =
        UUID::Random();

    const auto asset =
        resources.HandleRead(
            Json{
                {"uri",
                 "engine://asset/"
                     + missingAsset.ToString()}},
            McpProtocolEra::Modern2026);

    REQUIRE(
        RequireErrorResult(asset).code
        == JsonRpcInvalidParams);
}

TEST_CASE(
    "Scene MCP resource registration validates injected read capabilities",
    "[mcp][resource][scene][v0.8]")
{
    using namespace Janus::MCP;

    ResourceRegistry resources;

    const auto result =
        RegisterSceneResources(
            resources,
            McpSceneResourceContext{});

    REQUIRE_FALSE(result);
    REQUIRE(
        result.GetError().code
        == Janus::ErrorCode::InvalidArgument);
}
