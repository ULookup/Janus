#pragma once

#include "Asset/AssetHandle.h"
#include "Asset/AssetMetadata.h"
#include "Core/Error/Result.h"
#include "Core/Types.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Janus
{

struct AssetSearchResult
{
    std::vector<AssetMetadata> assets;
    usize total = 0;
};

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
    [[nodiscard]] std::vector<AssetMetadata> GetAssets() const;

    // Case-sensitive substring of the normalized relative path, ordered by path.
    [[nodiscard]] Result<AssetSearchResult> Search(std::string_view name = {},
                                                   std::optional<AssetType> type = {},
                                                   usize offset = 0, usize limit = 50) const;

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
