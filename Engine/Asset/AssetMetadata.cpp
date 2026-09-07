#include "Asset/AssetMetadata.h"

#include <string>

namespace Janus
{

std::string_view AssetTypeName(AssetType type) noexcept
{
    switch (type)
    {
    case AssetType::Prefab:
        return "prefab";
    case AssetType::AudioClip:
        return "audio-clip";
    case AssetType::AnimationClip:
        return "animation-clip";
    case AssetType::Texture:
        return "texture";
    case AssetType::ShaderSource:
        return "shader_source";
    case AssetType::Font:
        return "font";
    case AssetType::LuaScript:
        return "lua-script";
    }

    return "unknown";
}

Result<AssetType> ParseAssetType(std::string_view name)
{
    if (name == "prefab")
        return Result<AssetType>::Success(AssetType::Prefab);
    if (name == "audio-clip")
        return Result<AssetType>::Success(AssetType::AudioClip);
    if (name == "animation-clip")
        return Result<AssetType>::Success(AssetType::AnimationClip);
    if (name == "font")
        return Result<AssetType>::Success(AssetType::Font);

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
