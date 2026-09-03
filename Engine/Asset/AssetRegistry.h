#pragma once

#include "Asset/AssetHandle.h"
#include "Asset/AssetMetadata.h"
#include "Core/Error/Result.h"
#include "Core/Types.h"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace Janus
{

class AssetRegistry
{
public:
    [[nodiscard]] Result<AssetHandle> Register(
        AssetType type,
        const std::filesystem::path& relativePath);

    [[nodiscard]] Result<void> Register(AssetMetadata metadata);

    [[nodiscard]] const AssetMetadata* Find(AssetHandle handle) const noexcept;
    [[nodiscard]] const AssetMetadata* FindByPath(
        const std::filesystem::path& relativePath) const;

    [[nodiscard]] bool Contains(AssetHandle handle) const noexcept;
    [[nodiscard]] usize Size() const noexcept;

    [[nodiscard]] Result<void> Save(
        const std::filesystem::path& registryPath) const;
    [[nodiscard]] static Result<AssetRegistry> Load(
        const std::filesystem::path& registryPath);

private:
    [[nodiscard]] static Result<std::filesystem::path> NormalizeRelativePath(
        const std::filesystem::path& relativePath);
    [[nodiscard]] static std::string PathKey(
        const std::filesystem::path& normalizedPath);

    std::unordered_map<AssetHandle, AssetMetadata, AssetHandleHash> m_Metadata;
    std::unordered_map<std::string, AssetHandle> m_PathIndex;
};

} // namespace Janus
