#include "Schema/ReflectionSchemaAdapter.h"

#include "Schema/JsonSchema.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace Janus::MCP
{
namespace
{

Json PropertySchemaFragment(
    const PropertyDescriptor& property)
{
    Json schema;

    switch (property.type)
    {
    case PropertyType::Bool:
        schema = {
            {"type", "boolean"}};
        break;

    case PropertyType::Int32:
        schema = {
            {"type", "integer"},
            {"minimum", std::numeric_limits<i32>::min()},
            {"maximum", std::numeric_limits<i32>::max()}};
        break;

    case PropertyType::Float32:
        schema = {
            {"type", "number"}};
        break;

    case PropertyType::String:
        schema = {
            {"type", "string"}};
        break;

    case PropertyType::Vector2:
        schema = {
            {"type", "object"},
            {"properties",
             Json{
                 {"x", Json{{"type", "number"}}},
                 {"y", Json{{"type", "number"}}}}},
            {"required", Json::array({"x", "y"})},
            {"additionalProperties", false}};
        break;

    case PropertyType::Color:
        schema = {
            {"type", "object"},
            {"properties",
             Json{
                 {"r", Json{{"type", "number"}}},
                 {"g", Json{{"type", "number"}}},
                 {"b", Json{{"type", "number"}}},
                 {"a", Json{{"type", "number"}}}}},
            {"required", Json::array({"r", "g", "b", "a"})},
            {"additionalProperties", false}};
        break;

    case PropertyType::AssetReference:
        schema = {
            {"oneOf",
             Json::array(
                 {Json{
                      {"type", "string"},
                      {"format", "uuid"}},
                  Json{
                      {"type", "null"}}})}};

        if (!property.referenceConstraint.empty())
        {
            schema["description"] =
                "Janus asset UUID. Expected asset type: "
                + property.referenceConstraint
                + ".";
            schema["x-janus-asset-type"] =
                property.referenceConstraint;
        }
        break;

    case PropertyType::Unknown:
    default:
        schema = Json::object();
        schema["not"] =
            Json::object();
        break;
    }

    if (!property.editable)
    {
        schema["readOnly"] = true;
    }

    return schema;
}

Json ComponentSchemaFragment(
    const ComponentDescriptor& component)
{
    Json properties =
        Json::object();

    std::vector<const PropertyDescriptor*> descriptors;
    descriptors.reserve(
        component.properties.size());

    for (const auto& property : component.properties)
    {
        descriptors.push_back(
            &property);
    }

    std::sort(
        descriptors.begin(),
        descriptors.end(),
        [](const PropertyDescriptor* left,
           const PropertyDescriptor* right)
        {
            return left->serializedName
                < right->serializedName;
        });

    for (const PropertyDescriptor* property : descriptors)
    {
        properties[property->serializedName] =
            PropertySchemaFragment(
                *property);
    }

    return Json{
        {"type", "object"},
        {"properties", std::move(properties)},
        {"additionalProperties", false}};
}

std::vector<const ComponentDescriptor*> SortedComponents(
    const ReflectionRegistry& registry)
{
    auto components =
        registry.GetComponents();

    std::sort(
        components.begin(),
        components.end(),
        [](const ComponentDescriptor* left,
           const ComponentDescriptor* right)
        {
            return left->serializedName
                < right->serializedName;
        });

    return components;
}

} // namespace

Json BuildPropertySchema(
    const PropertyDescriptor& property)
{
    Json schema =
        PropertySchemaFragment(property);

    schema["$schema"] =
        std::string{McpJsonSchema202012};

    return schema;
}

Json BuildComponentSchema(
    const ComponentDescriptor& component)
{
    Json schema =
        ComponentSchemaFragment(component);

    schema["$schema"] =
        std::string{McpJsonSchema202012};

    return schema;
}

Json BuildReflectionCatalogSchema(
    const ReflectionRegistry& registry)
{
    Json properties =
        Json::object();

    for (const ComponentDescriptor* component
         : SortedComponents(registry))
    {
        properties[component->serializedName] =
            ComponentSchemaFragment(
                *component);
    }

    return Json{
        {"$schema", std::string{McpJsonSchema202012}},
        {"type", "object"},
        {"properties", std::move(properties)},
        {"additionalProperties", false}};
}

Json BuildPropertyMutationSchema(
    const ReflectionRegistry& registry)
{
    Json branches =
        Json::array();

    for (const ComponentDescriptor* component
         : SortedComponents(registry))
    {
        std::vector<const PropertyDescriptor*> properties;
        properties.reserve(
            component->properties.size());

        for (const auto& property : component->properties)
        {
            if (property.editable)
            {
                properties.push_back(
                    &property);
            }
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
            branches.push_back(
                Json{
                    {"type", "object"},
                    {"properties",
                     Json{
                         {"component",
                          Json{{"const", component->serializedName}}},
                         {"property",
                          Json{{"const", property->serializedName}}},
                         {"value",
                          PropertySchemaFragment(*property)}}},
                    {"required",
                     Json::array(
                         {"component", "property", "value"})},
                    {"additionalProperties", false}});
        }
    }

    return Json{
        {"$schema", std::string{McpJsonSchema202012}},
        {"type", "object"},
        {"oneOf", std::move(branches)}};
}

} // namespace Janus::MCP
