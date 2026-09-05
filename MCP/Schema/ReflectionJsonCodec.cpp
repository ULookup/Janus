#include "Schema/ReflectionJsonCodec.h"

#include "Core/Math/Vector2.h"
#include "Core/UUID/UUID.h"

#include <string>
#include <variant>

namespace Janus::MCP
{

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
