#include "Asset/AssetMetadata.h"

#include <string>

namespace Janus
{

std::string_view AssetTypeName(AssetType type) noexcept
{
    switch (type)
    {
    case AssetType::Texture:
        return "texture";
    case AssetType::ShaderSource:
        return "shader_source";
    case AssetType::LuaScript:
        return "lua-script";
    }

    return "unknown";
}

Result<AssetType> ParseAssetType(std::string_view name)
{
    if (name == "texture")
    {
        return Result<AssetType>::Success(AssetType::Texture);
    }

    if (name == "shader_source")
    {
        return Result<AssetType>::Success(AssetType::ShaderSource);
    }

    if (name == "lua-script")
    {
        return Result<AssetType>::Success(AssetType::LuaScript);
    }

    return Result<AssetType>::Failure(
        ErrorCode::InvalidArgument,
        "Unknown asset type '" + std::string(name) + "'.");
}

} // namespace Janus
