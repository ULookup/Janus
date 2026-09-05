#include "Resources/SceneResources.h"

#include "Asset/AssetMetadata.h"
#include "Asset/AssetRegistry.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Protocol/McpProtocol.h"
#include "Schema/ReflectionJsonCodec.h"
#include "Scene/Components.h"
#include "Scene/Hierarchy.h"
#include "Scene/Scene.h"
#include "Scene/SceneReflection.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace Janus::MCP
{
namespace
{

inline constexpr std::string_view ProjectInfoUri =
    "engine://project/info";
inline constexpr std::string_view SceneCurrentUri =
    "engine://scene/current";
inline constexpr std::string_view SceneHierarchyUri =
    "engine://scene/hierarchy";
inline constexpr std::string_view EntityUriPrefix =
    "engine://entity/";
inline constexpr std::string_view AssetUriPrefix =
    "engine://asset/";

McpDispatchError DispatchError(
    const Error& error)
{
    const i32 code =
        error.code == ErrorCode::InvalidArgument
            || error.code == ErrorCode::EntityNotFound
            || error.code == ErrorCode::AssetNotFound
            ? JsonRpcInvalidParams
            : JsonRpcInternalError;

    return McpDispatchError{
        code,
        error.message,
        nullptr};
}

McpDispatchResult JsonResourceResult(
    std::string_view uri,
    Json payload)
{
    return Json{
        {"contents",
         Json::array(
             {Json{
                 {"uri", std::string{uri}},
                 {"mimeType", "application/json"},
                 {"text", payload.dump()}}})}};
}

Result<const EntityIdentityComponent*> RequireIdentity(
    const Scene& scene,
    ECS::Entity entity)
{
    const auto* identity =
        scene.GetComponent<EntityIdentityComponent>(
            entity);

    if (identity == nullptr
        || !identity->id.IsValid())
    {
        return Result<const EntityIdentityComponent*>::Failure(
            ErrorCode::InvalidState,
            "Scene entity is missing a valid persistent identity.");
    }

    return Result<const EntityIdentityComponent*>::Success(
        identity);
}

Result<const HierarchyComponent*> RequireHierarchy(
    const Scene& scene,
    ECS::Entity entity)
{
    const auto* hierarchy =
        scene.GetComponent<HierarchyComponent>(
            entity);

    if (hierarchy == nullptr)
    {
        return Result<const HierarchyComponent*>::Failure(
            ErrorCode::InvalidState,
            "Scene entity is missing hierarchy state.");
    }

    return Result<const HierarchyComponent*>::Success(
        hierarchy);
}

Result<Json> ChildUuidArray(
    const Scene& scene,
    const HierarchyComponent& hierarchy)
{
    Json children =
        Json::array();

    ECS::Entity child =
        hierarchy.firstChild;

    usize visited = 0;
    const usize maxVisited =
        scene.GetEntities().size();

    while (child.IsValid())
    {
        if (++visited > maxVisited)
        {
            return Result<Json>::Failure(
                ErrorCode::InvalidState,
                "Scene hierarchy child chain contains a cycle.");
        }

        auto identity =
            RequireIdentity(
                scene,
                child);
        if (!identity)
        {
            return Result<Json>::Failure(
                identity.GetError());
        }

        children.push_back(
            identity.Value()->id.ToString());

        auto childHierarchy =
            RequireHierarchy(
                scene,
                child);
        if (!childHierarchy)
        {
            return Result<Json>::Failure(
                childHierarchy.GetError());
        }

        child =
            childHierarchy.Value()->nextSibling;
    }

    return Result<Json>::Success(
        std::move(children));
}

Result<usize> ChildSiblingOrder(
    const Scene& scene,
    ECS::Entity entity,
    const HierarchyComponent& hierarchy)
{
    if (!hierarchy.parent.IsValid())
    {
        return Result<usize>::Success(0);
    }

    auto parentHierarchy =
        RequireHierarchy(
            scene,
            hierarchy.parent);
    if (!parentHierarchy)
    {
        return Result<usize>::Failure(
            parentHierarchy.GetError());
    }

    ECS::Entity child =
        parentHierarchy.Value()->firstChild;

    usize order = 0;
    usize visited = 0;
    const usize maxVisited =
        scene.GetEntities().size();

    while (child.IsValid())
    {
        if (++visited > maxVisited)
        {
            return Result<usize>::Failure(
                ErrorCode::InvalidState,
                "Scene hierarchy sibling chain contains a cycle.");
        }

        if (child == entity)
        {
            return Result<usize>::Success(
                order);
        }

        auto childHierarchy =
            RequireHierarchy(
                scene,
                child);
        if (!childHierarchy)
        {
            return Result<usize>::Failure(
                childHierarchy.GetError());
        }

        child =
            childHierarchy.Value()->nextSibling;
        ++order;
    }

    return Result<usize>::Failure(
        ErrorCode::InvalidState,
        "Scene hierarchy parent does not reference its child.");
}

Result<Json> EntityStructure(
    const Scene& scene,
    ECS::Entity entity,
    usize rootOrder)
{
    auto identity =
        RequireIdentity(
            scene,
            entity);
    if (!identity)
    {
        return Result<Json>::Failure(
            identity.GetError());
    }

    auto hierarchy =
        RequireHierarchy(
            scene,
            entity);
    if (!hierarchy)
    {
        return Result<Json>::Failure(
            hierarchy.GetError());
    }

    Json parent = nullptr;
    usize siblingOrder = rootOrder;

    if (hierarchy.Value()->parent.IsValid())
    {
        auto parentIdentity =
            RequireIdentity(
                scene,
                hierarchy.Value()->parent);
        if (!parentIdentity)
        {
            return Result<Json>::Failure(
                parentIdentity.GetError());
        }

        parent =
            parentIdentity.Value()->id.ToString();

        auto order =
            ChildSiblingOrder(
                scene,
                entity,
                *hierarchy.Value());
        if (!order)
        {
            return Result<Json>::Failure(
                order.GetError());
        }

        siblingOrder =
            order.Value();
    }

    auto children =
        ChildUuidArray(
            scene,
            *hierarchy.Value());
    if (!children)
    {
        return Result<Json>::Failure(
            children.GetError());
    }

    return Result<Json>::Success(
        Json{
            {"uuid", identity.Value()->id.ToString()},
            {"name", identity.Value()->name},
            {"parent", std::move(parent)},
            {"siblingOrder", siblingOrder},
            {"children", std::move(children).Value()}});
}

Result<Json> BuildProjectInfo(
    const McpSceneResourceContext& context)
{
    auto projectState =
        context.projectState();
    if (!projectState)
    {
        return Result<Json>::Failure(
            projectState.GetError());
    }

    const SceneMetadata& metadata =
        context.scene->GetMetadata();

    if (!metadata.id.IsValid())
    {
        return Result<Json>::Failure(
            ErrorCode::InvalidState,
            "MCP project resource requires a Scene with a persistent UUID.");
    }

    return Result<Json>::Success(
        Json{
            {"engine",
             Json{
                 {"name", "Janus"},
                 {"agentAuthoring", true}}},
            {"protocol",
             Json{
                 {"supportedVersions",
                  Json::array(
                      {std::string{McpModernProtocolVersion},
                       std::string{McpLegacyProtocolVersion}})},
                 {"transport", "stdio"}}},
            {"project",
             Json{
                 {"displayPath",
                  projectState.Value().projectDisplayPath}}},
            {"scene",
             Json{
                 {"uuid", metadata.id.ToString()},
                 {"name", metadata.name}}},
            {"authoring",
             Json{
                 {"dirty", projectState.Value().dirty},
                 {"readOnly",
                  projectState.Value().authoringReadOnly}}},
            {"assets",
             Json{
                 {"count", context.assets->Size()}}}});
}

Result<Json> BuildSceneCurrent(
    const McpSceneResourceContext& context)
{
    const SceneMetadata& metadata =
        context.scene->GetMetadata();

    if (!metadata.id.IsValid())
    {
        return Result<Json>::Failure(
            ErrorCode::InvalidState,
            "MCP Scene resource requires a persistent Scene UUID.");
    }

    Json components =
        Json::array();

    std::vector<const ComponentDescriptor*> descriptors =
        context.reflection->GetComponents();

    std::sort(
        descriptors.begin(),
        descriptors.end(),
        [](const ComponentDescriptor* left,
           const ComponentDescriptor* right)
        {
            return left->serializedName
                < right->serializedName;
        });

    for (const ComponentDescriptor* component : descriptors)
    {
        Json properties =
            Json::array();

        std::vector<const PropertyDescriptor*> propertyDescriptors;
        propertyDescriptors.reserve(
            component->properties.size());

        for (const PropertyDescriptor& property
             : component->properties)
        {
            propertyDescriptors.push_back(
                &property);
        }

        std::sort(
            propertyDescriptors.begin(),
            propertyDescriptors.end(),
            [](const PropertyDescriptor* left,
               const PropertyDescriptor* right)
            {
                return left->serializedName
                    < right->serializedName;
            });

        for (const PropertyDescriptor* property
             : propertyDescriptors)
        {
            Json propertyJson = {
                {"name", property->serializedName},
                {"type",
                 std::string{
                     McpPropertyTypeName(
                         property->type)}},
                {"editable", property->editable},
                {"serializable", property->serializable}};

            if (!property->referenceConstraint.empty())
            {
                propertyJson["assetType"] =
                    property->referenceConstraint;
            }

            properties.push_back(
                std::move(propertyJson));
        }

        components.push_back(
            Json{
                {"name", component->serializedName},
                {"removable", component->removable},
                {"serializable", component->serializable},
                {"properties", std::move(properties)}});
    }

    return Result<Json>::Success(
        Json{
            {"uuid", metadata.id.ToString()},
            {"name", metadata.name},
            {"entityCount",
             context.scene->GetEntities().size()},
            {"components", std::move(components)}});
}

Result<Json> BuildSceneHierarchy(
    const McpSceneResourceContext& context)
{
    struct EntityRecord
    {
        ECS::Entity entity;
        std::string uuid;
        bool root = false;
    };

    std::vector<EntityRecord> records;
    records.reserve(
        context.scene->GetEntities().size());

    for (const ECS::Entity entity
         : context.scene->GetEntities())
    {
        auto identity =
            RequireIdentity(
                *context.scene,
                entity);
        if (!identity)
        {
            return Result<Json>::Failure(
                identity.GetError());
        }

        auto hierarchy =
            RequireHierarchy(
                *context.scene,
                entity);
        if (!hierarchy)
        {
            return Result<Json>::Failure(
                hierarchy.GetError());
        }

        records.push_back(
            EntityRecord{
                entity,
                identity.Value()->id.ToString(),
                !hierarchy.Value()->parent.IsValid()});
    }

    std::sort(
        records.begin(),
        records.end(),
        [](const EntityRecord& left,
           const EntityRecord& right)
        {
            return left.uuid < right.uuid;
        });

    std::vector<std::string> rootUuids;

    for (const EntityRecord& record : records)
    {
        if (record.root)
        {
            rootUuids.push_back(
                record.uuid);
        }
    }

    Json entities =
        Json::array();

    for (const EntityRecord& record : records)
    {
        usize order = 0;

        if (record.root)
        {
            const auto rootIt =
                std::find(
                    rootUuids.begin(),
                    rootUuids.end(),
                    record.uuid);

            if (rootIt == rootUuids.end())
            {
                return Result<Json>::Failure(
                    ErrorCode::InvalidState,
                    "MCP hierarchy failed to resolve deterministic root order.");
            }

            order =
                static_cast<usize>(
                    std::distance(
                        rootUuids.begin(),
                        rootIt));
        }

        auto structure =
            EntityStructure(
                *context.scene,
                record.entity,
                order);
        if (!structure)
        {
            return Result<Json>::Failure(
                structure.GetError());
        }

        entities.push_back(
            std::move(structure).Value());

    }

    return Result<Json>::Success(
        Json{
            {"roots", rootUuids},
            {"entities", std::move(entities)}});
}

Result<UUID> ParseResourceUuid(
    std::string_view uri,
    std::string_view prefix)
{
    if (!uri.starts_with(prefix))
    {
        return Result<UUID>::Failure(
            ErrorCode::InvalidArgument,
            "MCP resource URI has an unexpected prefix.");
    }

    const std::string_view text =
        uri.substr(prefix.size());

    if (text.empty()
        || text.find('/') != std::string_view::npos)
    {
        return Result<UUID>::Failure(
            ErrorCode::InvalidArgument,
            "MCP resource URI requires one UUID path segment.");
    }

    auto parsed =
        UUID::Parse(text);
    if (!parsed)
    {
        return parsed;
    }

    if (!parsed.Value().IsValid())
    {
        return Result<UUID>::Failure(
            ErrorCode::InvalidArgument,
            "MCP resource UUID cannot be nil.");
    }

    return parsed;
}

Result<Json> BuildEntity(
    const McpSceneResourceContext& context,
    std::string_view uri)
{
    auto id =
        ParseResourceUuid(
            uri,
            EntityUriPrefix);
    if (!id)
    {
        return Result<Json>::Failure(
            id.GetError());
    }

    const ECS::Entity entity =
        context.scene->FindEntity(
            id.Value());

    if (!entity.IsValid())
    {
        return Result<Json>::Failure(
            ErrorCode::EntityNotFound,
            "Requested MCP entity does not exist.");
    }

    auto hierarchy =
        RequireHierarchy(
            *context.scene,
            entity);
    if (!hierarchy)
    {
        return Result<Json>::Failure(
            hierarchy.GetError());
    }

    auto structure =
        EntityStructure(
            *context.scene,
            entity,
            0);
    if (!structure)
    {
        return Result<Json>::Failure(
            structure.GetError());
    }

    SceneReflection reflection(
        *context.reflection,
        context.assets);

    Json components =
        Json::object();

    std::vector<const ComponentDescriptor*> descriptors =
        context.reflection->GetComponents();

    std::sort(
        descriptors.begin(),
        descriptors.end(),
        [](const ComponentDescriptor* left,
           const ComponentDescriptor* right)
        {
            return left->serializedName
                < right->serializedName;
        });

    for (const ComponentDescriptor* component : descriptors)
    {
        auto present =
            reflection.HasComponent(
                *context.scene,
                id.Value(),
                component->id);
        if (!present)
        {
            return Result<Json>::Failure(
                present.GetError());
        }

        if (!present.Value())
        {
            continue;
        }

        Json values =
            Json::object();

        std::vector<const PropertyDescriptor*> properties;
        properties.reserve(
            component->properties.size());

        for (const PropertyDescriptor& property
             : component->properties)
        {
            properties.push_back(
                &property);
        }

        std::sort(
            properties.begin(),
            properties.end(),
            [](const PropertyDescriptor* left,
               const PropertyDescriptor* right)
            {
                return left->serializedName
                    < right->serializedName;
            });

        for (const PropertyDescriptor* property : properties)
        {
            auto value =
                reflection.GetProperty(
                    *context.scene,
                    id.Value(),
                    component->id,
                    property->id);
            if (!value)
            {
                return Result<Json>::Failure(
                    value.GetError());
            }

            auto json =
                PropertyValueToMcpJson(
                    value.Value());
            if (!json)
            {
                return Result<Json>::Failure(
                    json.GetError());
            }

            values[property->serializedName] =
                std::move(json).Value();
        }

        components[component->serializedName] =
            std::move(values);
    }

    Json result =
        std::move(structure).Value();

    result["components"] =
        std::move(components);

    return Result<Json>::Success(
        std::move(result));
}

Result<Json> BuildAsset(
    const McpSceneResourceContext& context,
    std::string_view uri)
{
    auto id =
        ParseResourceUuid(
            uri,
            AssetUriPrefix);
    if (!id)
    {
        return Result<Json>::Failure(
            id.GetError());
    }

    const AssetHandle handle{
        id.Value()};

    const AssetMetadata* metadata =
        context.assets->Find(
            handle);

    if (metadata == nullptr)
    {
        return Result<Json>::Failure(
            ErrorCode::AssetNotFound,
            "Requested MCP asset does not exist.");
    }

    return Result<Json>::Success(
        Json{
            {"uuid", metadata->handle.ToString()},
            {"type",
             std::string{
                 AssetTypeName(
                     metadata->type)}},
            {"path",
             metadata->relativePath.generic_string()}});
}

template <typename Builder>
McpResourceReadHandler MakeReadHandler(
    McpSceneResourceContext context,
    Builder builder)
{
    return [context = std::move(context),
            builder = std::move(builder)](
               std::string_view uri,
               McpProtocolEra) -> McpDispatchResult
    {
        auto payload =
            builder(
                context,
                uri);

        if (!payload)
        {
            return DispatchError(
                payload.GetError());
        }

        return JsonResourceResult(
            uri,
            std::move(payload).Value());
    };
}

} // namespace

Result<void> RegisterSceneResources(
    ResourceRegistry& registry,
    McpSceneResourceContext context)
{
    if (context.scene == nullptr
        || context.reflection == nullptr
        || context.assets == nullptr
        || !context.projectState)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "MCP Scene resources require Scene, Reflection, AssetRegistry, and project-state provider.");
    }

    auto project = registry.RegisterResource(
        McpResourceDescriptor{
            std::string{ProjectInfoUri},
            "Janus Project Info",
            "Janus Project Info",
            "Current Janus authoring project and protocol summary.",
            "application/json",
            MakeReadHandler(
                context,
                [](const McpSceneResourceContext& value,
                   std::string_view)
                {
                    return BuildProjectInfo(
                        value);
                })});
    if (!project)
    {
        return project;
    }

    auto scene = registry.RegisterResource(
        McpResourceDescriptor{
            std::string{SceneCurrentUri},
            "Current Janus Scene",
            "Current Janus Scene",
            "Current authoring Scene summary.",
            "application/json",
            MakeReadHandler(
                context,
                [](const McpSceneResourceContext& value,
                   std::string_view)
                {
                    return BuildSceneCurrent(
                        value);
                })});
    if (!scene)
    {
        return scene;
    }

    auto hierarchy = registry.RegisterResource(
        McpResourceDescriptor{
            std::string{SceneHierarchyUri},
            "Janus Scene Hierarchy",
            "Janus Scene Hierarchy",
            "Persistent UUID-backed authoring hierarchy.",
            "application/json",
            MakeReadHandler(
                context,
                [](const McpSceneResourceContext& value,
                   std::string_view)
                {
                    return BuildSceneHierarchy(
                        value);
                })});
    if (!hierarchy)
    {
        return hierarchy;
    }

    auto entity = registry.RegisterTemplate(
        McpResourceTemplateDescriptor{
            "engine://entity/{uuid}",
            "Janus Entity",
            "Janus Entity",
            "Reflected authoring state for one persistent entity UUID.",
            "application/json",
            MakeReadHandler(
                context,
                [](const McpSceneResourceContext& value,
                   std::string_view uri)
                {
                    return BuildEntity(
                        value,
                        uri);
                })});
    if (!entity)
    {
        return entity;
    }

    auto asset = registry.RegisterTemplate(
        McpResourceTemplateDescriptor{
            "engine://asset/{uuid}",
            "Janus Asset",
            "Janus Asset",
            "Registered Janus asset metadata by persistent AssetHandle UUID.",
            "application/json",
            MakeReadHandler(
                std::move(context),
                [](const McpSceneResourceContext& value,
                   std::string_view uri)
                {
                    return BuildAsset(
                        value,
                        uri);
                })});

    return asset;
}

} // namespace Janus::MCP
