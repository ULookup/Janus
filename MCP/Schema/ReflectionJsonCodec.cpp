#include "Schema/ReflectionJsonCodec.h"

#include "Core/Math/Vector2.h"
#include "Core/UUID/UUID.h"

#include <cmath>
#include <limits>
#include <string>
#include <variant>

namespace Janus::MCP
{

namespace
{

Result<f32> ParseFiniteFloat(
    const Json& value,
    std::string_view field)
{
    if (!value.is_number())
    {
        return Result<f32>::Failure(
            ErrorCode::InvalidArgument,
            "MCP reflected field '" + std::string{field}
                + "' must be numeric.");
    }

    const double raw =
        value.get<double>();

    if (!std::isfinite(raw)
        || raw < -static_cast<double>(
            std::numeric_limits<f32>::max())
        || raw > static_cast<double>(
            std::numeric_limits<f32>::max()))
    {
        return Result<f32>::Failure(
            ErrorCode::InvalidArgument,
            "MCP reflected numeric value is outside Float32 range.");
    }

    return Result<f32>::Success(
        static_cast<f32>(raw));
}

} // namespace


Result<Json> PropertyValueToMcpJson(
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
        const Vector2 vector =
            std::get<Vector2>(value);

        return Result<Json>::Success(
            Json{
                {"x", vector.x},
                {"y", vector.y}});
    }

    case PropertyType::Color:
    {
        const ColorValue color =
            std::get<ColorValue>(value);

        return Result<Json>::Success(
            Json{
                {"r", color.r},
                {"g", color.g},
                {"b", color.b},
                {"a", color.a}});
    }

    case PropertyType::AssetReference:
    {
        const auto reference =
            std::get<AssetReferenceValue>(
                value);

        if (!reference.id.IsValid())
        {
            return Result<Json>::Success(
                Json(nullptr));
        }

        return Result<Json>::Success(
            Json(reference.id.ToString()));
    }

    case PropertyType::Unknown:
    default:
        return Result<Json>::Failure(
            ErrorCode::InvalidState,
            "Cannot encode unknown reflected PropertyValue for MCP.");
    }
}


Result<PropertyValue> McpJsonToPropertyValue(
    const Json& value,
    const PropertyDescriptor& property)
{
    switch (property.type)
    {
    case PropertyType::Bool:
        if (!value.is_boolean())
        {
            break;
        }
        return Result<PropertyValue>::Success(
            PropertyValue{
                value.get<bool>()});

    case PropertyType::Int32:
        if (value.is_number_unsigned())
        {
            const u64 raw =
                value.get<u64>();
            if (raw
                <= static_cast<u64>(
                    std::numeric_limits<i32>::max()))
            {
                return Result<PropertyValue>::Success(
                    PropertyValue{
                        static_cast<i32>(raw)});
            }
        }
        else if (value.is_number_integer())
        {
            const i64 raw =
                value.get<i64>();
            if (raw
                    >= static_cast<i64>(
                        std::numeric_limits<i32>::min())
                && raw
                    <= static_cast<i64>(
                        std::numeric_limits<i32>::max()))
            {
                return Result<PropertyValue>::Success(
                    PropertyValue{
                        static_cast<i32>(raw)});
            }
        }
        break;

    case PropertyType::Float32:
    {
        auto parsed =
            ParseFiniteFloat(
                value,
                property.serializedName);
        if (!parsed)
        {
            return Result<PropertyValue>::Failure(
                parsed.GetError());
        }

        return Result<PropertyValue>::Success(
            PropertyValue{
                parsed.Value()});
    }

    case PropertyType::String:
        if (!value.is_string())
        {
            break;
        }
        return Result<PropertyValue>::Success(
            PropertyValue{
                value.get<std::string>()});

    case PropertyType::Vector2:
    {
        if (!value.is_object()
            || value.size() != 2
            || !value.contains("x")
            || !value.contains("y"))
        {
            break;
        }

        auto x =
            ParseFiniteFloat(
                value.at("x"),
                "x");
        auto y =
            ParseFiniteFloat(
                value.at("y"),
                "y");

        if (!x)
        {
            return Result<PropertyValue>::Failure(
                x.GetError());
        }

        if (!y)
        {
            return Result<PropertyValue>::Failure(
                y.GetError());
        }

        return Result<PropertyValue>::Success(
            PropertyValue{
                Vector2{
                    x.Value(),
                    y.Value()}});
    }

    case PropertyType::Color:
    {
        if (!value.is_object()
            || value.size() != 4
            || !value.contains("r")
            || !value.contains("g")
            || !value.contains("b")
            || !value.contains("a"))
        {
            break;
        }

        auto r = ParseFiniteFloat(value.at("r"), "r");
        auto g = ParseFiniteFloat(value.at("g"), "g");
        auto b = ParseFiniteFloat(value.at("b"), "b");
        auto a = ParseFiniteFloat(value.at("a"), "a");

        if (!r)
        {
            return Result<PropertyValue>::Failure(r.GetError());
        }
        if (!g)
        {
            return Result<PropertyValue>::Failure(g.GetError());
        }
        if (!b)
        {
            return Result<PropertyValue>::Failure(b.GetError());
        }
        if (!a)
        {
            return Result<PropertyValue>::Failure(a.GetError());
        }

        return Result<PropertyValue>::Success(
            PropertyValue{
                ColorValue{
                    r.Value(),
                    g.Value(),
                    b.Value(),
                    a.Value()}});
    }

    case PropertyType::AssetReference:
        if (value.is_null())
        {
            return Result<PropertyValue>::Success(
                PropertyValue{
                    AssetReferenceValue{UUID{}}});
        }

        if (value.is_string())
        {
            auto id =
                UUID::Parse(
                    value.get_ref<const std::string&>());
            if (!id)
            {
                return Result<PropertyValue>::Failure(
                    id.GetError());
            }

            if (!id.Value().IsValid())
            {
                return Result<PropertyValue>::Failure(
                    ErrorCode::InvalidArgument,
                    "MCP AssetReference UUID cannot be nil; use null to clear the reference.");
            }

            return Result<PropertyValue>::Success(
                PropertyValue{
                    AssetReferenceValue{
                        id.Value()}});
        }
        break;

    case PropertyType::Unknown:
    default:
        return Result<PropertyValue>::Failure(
            ErrorCode::InvalidState,
            "Cannot decode unknown reflected PropertyType from MCP JSON.");
    }

    return Result<PropertyValue>::Failure(
        ErrorCode::InvalidArgument,
        "MCP value does not match reflected property '"
            + property.serializedName
            + "' type "
            + std::string{
                McpPropertyTypeName(
                    property.type)}
            + ".");
}

std::string_view McpPropertyTypeName(
    PropertyType type) noexcept
{
    switch (type)
    {
    case PropertyType::Bool:
        return "bool";
    case PropertyType::Int32:
        return "int32";
    case PropertyType::Float32:
        return "float32";
    case PropertyType::String:
        return "string";
    case PropertyType::Vector2:
        return "vector2";
    case PropertyType::Color:
        return "color";
    case PropertyType::AssetReference:
        return "asset-reference";
    case PropertyType::Unknown:
    default:
        return "unknown";
    }
}

} // namespace Janus::MCP
