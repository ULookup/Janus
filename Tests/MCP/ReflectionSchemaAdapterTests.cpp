#include "Schema/ReflectionSchemaAdapter.h"

#include "Scene/SceneReflection.h"

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{

Janus::PropertyValue DefaultValueFor(
    Janus::PropertyType type)
{
    using namespace Janus;

    switch (type)
    {
    case PropertyType::Bool:
        return PropertyValue{false};
    case PropertyType::Int32:
        return PropertyValue{i32{0}};
    case PropertyType::Float32:
        return PropertyValue{f32{0.0f}};
    case PropertyType::String:
        return PropertyValue{std::string{}};
    case PropertyType::Vector2:
        return PropertyValue{Vector2{}};
    case PropertyType::Color:
        return PropertyValue{ColorValue{}};
    case PropertyType::AssetReference:
        return PropertyValue{
            AssetReferenceValue{UUID{}}};
    case PropertyType::Unknown:
    default:
        return PropertyValue{false};
    }
}

Janus::PropertyDescriptor MakeProperty(
    std::string name,
    Janus::PropertyType type,
    bool editable = true,
    std::string referenceConstraint = {})
{
    using namespace Janus;

    const std::string canonical =
        "SchemaComponent."
        + name;

    return PropertyDescriptor{
        MakePropertyId(canonical),
        name,
        name,
        type,
        true,
        editable,
        editable,
        std::move(referenceConstraint),
        [type](const void*)
        {
            return Result<PropertyValue>::Success(
                DefaultValueFor(type));
        },
        editable
            ? PropertySetter{
                  [](void*,
                     const PropertyValue&)
                  {
                      return Result<void>::Success();
                  }}
            : PropertySetter{}};
}

Janus::ReflectionRegistry CreateSchemaRegistry()
{
    using namespace Janus;

    ReflectionRegistry registry;

    ComponentDescriptor component;
    component.id =
        MakeComponentTypeId(
            "SchemaComponent");
    component.name =
        "SchemaComponent";
    component.serializedName =
        "SchemaComponent";
    component.properties = {
        MakeProperty("enabled", PropertyType::Bool),
        MakeProperty("count", PropertyType::Int32),
        MakeProperty("weight", PropertyType::Float32),
        MakeProperty("label", PropertyType::String),
        MakeProperty("position", PropertyType::Vector2),
        MakeProperty("color", PropertyType::Color),
        MakeProperty(
            "texture",
            PropertyType::AssetReference,
            true,
            "texture"),
        MakeProperty(
            "runtimeHint",
            PropertyType::String,
            false)};

    const auto registered =
        registry.RegisterComponent(
            std::move(component));

    REQUIRE(registered);

    return registry;
}

const Janus::PropertyDescriptor& RequireProperty(
    const Janus::ReflectionRegistry& registry,
    std::string_view name)
{
    const auto* component =
        registry.FindComponent(
            "SchemaComponent");

    REQUIRE(component != nullptr);

    const auto* property =
        component->FindProperty(name);

    REQUIRE(property != nullptr);

    return *property;
}

} // namespace

TEST_CASE(
    "Reflection schema adapter maps every authoring PropertyType",
    "[mcp][schema][reflection][v0.8]")
{
    using namespace Janus;
    using namespace Janus::MCP;

    const ReflectionRegistry registry =
        CreateSchemaRegistry();

    const Json boolean =
        BuildPropertySchema(
            RequireProperty(
                registry,
                "enabled"));
    REQUIRE(boolean.at("type") == "boolean");
    REQUIRE(
        boolean.at("$schema")
        == std::string{McpJsonSchema202012});

    const Json integer =
        BuildPropertySchema(
            RequireProperty(
                registry,
                "count"));
    REQUIRE(integer.at("type") == "integer");
    REQUIRE(
        integer.at("minimum")
        == std::numeric_limits<i32>::min());
    REQUIRE(
        integer.at("maximum")
        == std::numeric_limits<i32>::max());

    const Json number =
        BuildPropertySchema(
            RequireProperty(
                registry,
                "weight"));
    REQUIRE(number.at("type") == "number");

    const Json string =
        BuildPropertySchema(
            RequireProperty(
                registry,
                "label"));
    REQUIRE(string.at("type") == "string");

    const Json vector =
        BuildPropertySchema(
            RequireProperty(
                registry,
                "position"));
    REQUIRE(vector.at("type") == "object");
    REQUIRE(
        vector.at("required")
        == Json::array({"x", "y"}));
    REQUIRE(
        vector.at("additionalProperties")
        == false);

    const Json color =
        BuildPropertySchema(
            RequireProperty(
                registry,
                "color"));
    REQUIRE(color.at("type") == "object");
    REQUIRE(
        color.at("required")
        == Json::array(
            {"r", "g", "b", "a"}));

    const Json asset =
        BuildPropertySchema(
            RequireProperty(
                registry,
                "texture"));
    REQUIRE(asset.at("oneOf").size() == 2);
    REQUIRE(
        asset.at("oneOf").at(0).at("type")
        == "string");
    REQUIRE(
        asset.at("oneOf").at(0).at("format")
        == "uuid");
    REQUIRE(
        asset.at("oneOf").at(1).at("type")
        == "null");
    REQUIRE(
        asset.at("x-janus-asset-type")
        == "texture");
    REQUIRE(
        asset.at("description")
            .get<std::string>()
            .find("texture")
        != std::string::npos);
}

TEST_CASE(
    "Reflection component schema is deterministic and marks read-only properties",
    "[mcp][schema][reflection][v0.8]")
{
    using namespace Janus;
    using namespace Janus::MCP;

    const ReflectionRegistry registry =
        CreateSchemaRegistry();

    const auto* component =
        registry.FindComponent(
            "SchemaComponent");

    REQUIRE(component != nullptr);

    const Json schema =
        BuildComponentSchema(
            *component);

    REQUIRE(
        schema.at("$schema")
        == std::string{McpJsonSchema202012});
    REQUIRE(
        schema.at("additionalProperties")
        == false);

    const Json& properties =
        schema.at("properties");

    REQUIRE(properties.contains("enabled"));
    REQUIRE(properties.contains("position"));
    REQUIRE(properties.contains("texture"));
    REQUIRE(properties.contains("runtimeHint"));
    REQUIRE(
        properties.at("runtimeHint")
            .at("readOnly")
        == true);
}

TEST_CASE(
    "Reflection catalog schema uses serialized component names",
    "[mcp][schema][reflection][v0.8]")
{
    using namespace Janus;
    using namespace Janus::MCP;

    auto builtinsResult =
        CreateBuiltinSceneReflectionRegistry();

    REQUIRE(builtinsResult);

    const Json schema =
        BuildReflectionCatalogSchema(
            builtinsResult.Value());

    const Json& components =
        schema.at("properties");

    REQUIRE(components.contains("Transform"));
    REQUIRE(
        components.contains(
            "SpriteRenderer"));
    REQUIRE(components.contains("Camera"));
    REQUIRE(components.contains("LuaScript"));

    const Json& transform =
        components.at("Transform")
            .at("properties");

    REQUIRE(transform.contains("position"));
    REQUIRE(transform.contains("rotation"));
    REQUIRE(transform.contains("scale"));
    REQUIRE_FALSE(transform.contains("worldMatrix"));
    REQUIRE_FALSE(transform.contains("dirty"));
}

TEST_CASE(
    "Reflection property mutation schema contains editable typed branches only",
    "[mcp][schema][reflection][v0.8]")
{
    using namespace Janus;
    using namespace Janus::MCP;

    const ReflectionRegistry registry =
        CreateSchemaRegistry();

    const Json schema =
        BuildPropertyMutationSchema(
            registry);

    REQUIRE(
        schema.at("$schema")
        == std::string{McpJsonSchema202012});

    const Json& branches =
        schema.at("oneOf");

    REQUIRE(branches.size() == 7);

    bool foundPosition = false;
    bool foundTexture = false;
    bool foundRuntimeHint = false;

    for (const Json& branch : branches)
    {
        const Json& properties =
            branch.at("properties");

        const std::string property =
            properties.at("property")
                .at("const")
                .get<std::string>();

        REQUIRE(
            properties.at("component")
                .at("const")
            == "SchemaComponent");

        if (property == "position")
        {
            foundPosition = true;
            REQUIRE(
                properties.at("value")
                    .at("type")
                == "object");
            REQUIRE(
                properties.at("value")
                    .at("properties")
                    .at("x")
                    .at("type")
                == "number");
        }
        else if (property == "texture")
        {
            foundTexture = true;
            REQUIRE(
                properties.at("value")
                    .at("oneOf")
                    .at(0)
                    .at("format")
                == "uuid");
            REQUIRE(
                properties.at("value")
                    .at("oneOf")
                    .at(1)
                    .at("type")
                == "null");
            REQUIRE(
                properties.at("value")
                    .at("x-janus-asset-type")
                == "texture");
        }
        else if (property == "runtimeHint")
        {
            foundRuntimeHint = true;
        }
    }

    REQUIRE(foundPosition);
    REQUIRE(foundTexture);
    REQUIRE_FALSE(foundRuntimeHint);
}
