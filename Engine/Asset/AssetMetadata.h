#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Error/Result.h"

#include <filesystem>
#include <string_view>

namespace Janus
{

enum class AssetType
{
    Texture,
    ShaderSource
};

[[nodiscard]] std::string_view AssetTypeName(AssetType type) noexcept;
[[nodiscard]] Result<AssetType> ParseAssetType(std::string_view name);

struct AssetMetadata
{
    AssetHandle handle;
    AssetType type = AssetType::Texture;
    std::filesystem::path relativePath;
};

} // namespace Janus
