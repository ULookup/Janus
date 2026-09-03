#include "Asset/AssetRegistry.h"

#include "Core/FileSystem/FileSystem.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace Janus
{
namespace
{

constexpr std::string_view RegistrySchema = "janus.asset-registry";
constexpr i32 RegistryVersion = 1;

[[nodiscard]] Error InvalidRegistry(std::string message)
{
    return Error(
        ErrorCode::InvalidArgument,
        "Invalid asset registry: " + std::move(message));
}

} // namespace

Result<AssetHandle> AssetRegistry::Register(
    AssetType type,
    const std::filesystem::path& relativePath)
{
    const AssetHandle handle = AssetHandle::Random();
    auto result = Register(AssetMetadata{handle, type, relativePath});
    if (!result)
    {
        return Result<AssetHandle>::Failure(result.GetError());
    }

    return Result<AssetHandle>::Success(handle);
}

Result<void> AssetRegistry::Register(AssetMetadata metadata)
{
    if (!metadata.handle.IsValid())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Cannot register an asset with an invalid handle.");
    }

    auto normalized = NormalizeRelativePath(metadata.relativePath);
    if (!normalized)
    {
        return Result<void>::Failure(normalized.GetError());
    }

    metadata.relativePath = std::move(normalized).Value();
    const std::string pathKey = PathKey(metadata.relativePath);

    if (m_Metadata.contains(metadata.handle))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Asset handle '" + metadata.handle.ToString() + "' is already registered.");
    }

    if (m_PathIndex.contains(pathKey))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Asset path '" + pathKey + "' is already registered.");
    }

    const AssetHandle handle = metadata.handle;
    m_Metadata.emplace(handle, std::move(metadata));
    m_PathIndex.emplace(pathKey, handle);

    return Result<void>::Success();
}

const AssetMetadata* AssetRegistry::Find(AssetHandle handle) const noexcept
{
    const auto iterator = m_Metadata.find(handle);
    return iterator == m_Metadata.end() ? nullptr : &iterator->second;
}

const AssetMetadata* AssetRegistry::FindByPath(
    const std::filesystem::path& relativePath) const
{
    auto normalized = NormalizeRelativePath(relativePath);
    if (!normalized)
    {
        return nullptr;
    }

    const auto pathIterator = m_PathIndex.find(PathKey(normalized.Value()));
    if (pathIterator == m_PathIndex.end())
    {
        return nullptr;
    }

    return Find(pathIterator->second);
}

bool AssetRegistry::Contains(AssetHandle handle) const noexcept
{
    return m_Metadata.contains(handle);
}

usize AssetRegistry::Size() const noexcept
{
    return m_Metadata.size();
}

Result<void> AssetRegistry::Save(const std::filesystem::path& registryPath) const
{
    nlohmann::json document = {
        {"schema", std::string(RegistrySchema)},
        {"version", RegistryVersion},
        {"assets", nlohmann::json::array()}};

    std::vector<const AssetMetadata*> assets;
    assets.reserve(m_Metadata.size());
    for (const auto& [handle, metadata] : m_Metadata)
    {
        static_cast<void>(handle);
        assets.push_back(&metadata);
    }

    std::sort(
        assets.begin(),
        assets.end(),
        [](const AssetMetadata* left, const AssetMetadata* right)
        {
            return left->relativePath.generic_string()
                < right->relativePath.generic_string();
        });

    for (const AssetMetadata* metadata : assets)
    {
        document["assets"].push_back({
            {"handle", metadata->handle.ToString()},
            {"type", std::string(AssetTypeName(metadata->type))},
            {"path", metadata->relativePath.generic_string()}});
    }

    return FileSystem::WriteTextAtomic(
        registryPath,
        document.dump(2) + "\n");
}

Result<AssetRegistry> AssetRegistry::Load(
    const std::filesystem::path& registryPath)
{
    auto contents = FileSystem::ReadText(registryPath);
    if (!contents)
    {
        return Result<AssetRegistry>::Failure(contents.GetError());
    }

    try
    {
        const nlohmann::json document = nlohmann::json::parse(contents.Value());

        if (!document.is_object()
            || !document.contains("schema")
            || !document["schema"].is_string()
            || document["schema"].get<std::string>() != RegistrySchema)
        {
            return Result<AssetRegistry>::Failure(
                InvalidRegistry("unsupported or missing schema."));
        }

        if (!document.contains("version")
            || !document["version"].is_number_integer()
            || document["version"].get<i32>() != RegistryVersion)
        {
            return Result<AssetRegistry>::Failure(
                InvalidRegistry("unsupported or missing version."));
        }

        if (!document.contains("assets") || !document["assets"].is_array())
        {
            return Result<AssetRegistry>::Failure(
                InvalidRegistry("'assets' must be an array."));
        }

        AssetRegistry registry;

        for (const auto& entry : document["assets"])
        {
            if (!entry.is_object()
                || !entry.contains("handle")
                || !entry["handle"].is_string()
                || !entry.contains("type")
                || !entry["type"].is_string()
                || !entry.contains("path")
                || !entry["path"].is_string())
            {
                return Result<AssetRegistry>::Failure(
                    InvalidRegistry(
                        "each asset must contain string handle, type, and path fields."));
            }

            auto handle = AssetHandle::Parse(entry["handle"].get<std::string>());
            if (!handle)
            {
                return Result<AssetRegistry>::Failure(handle.GetError());
            }

            auto type = ParseAssetType(entry["type"].get<std::string>());
            if (!type)
            {
                return Result<AssetRegistry>::Failure(type.GetError());
            }

            AssetMetadata metadata{
                std::move(handle).Value(),
                std::move(type).Value(),
                std::filesystem::path(entry["path"].get<std::string>())};

            auto registered = registry.Register(std::move(metadata));
            if (!registered)
            {
                return Result<AssetRegistry>::Failure(registered.GetError());
            }
        }

        return Result<AssetRegistry>::Success(std::move(registry));
    }
    catch (const nlohmann::json::exception& exception)
    {
        return Result<AssetRegistry>::Failure(
            ErrorCode::InvalidArgument,
            "Failed to parse asset registry '" + registryPath.string()
                + "': " + exception.what());
    }
}

Result<std::filesystem::path> AssetRegistry::NormalizeRelativePath(
    const std::filesystem::path& relativePath)
{
    if (relativePath.empty()
        || relativePath.is_absolute()
        || relativePath.has_root_name()
        || relativePath.has_root_directory())
    {
        return Result<std::filesystem::path>::Failure(
            ErrorCode::InvalidArgument,
            "Asset path must be project-relative.");
    }

    const std::filesystem::path normalized = relativePath.lexically_normal();
    if (normalized.empty() || normalized == std::filesystem::path("."))
    {
        return Result<std::filesystem::path>::Failure(
            ErrorCode::InvalidArgument,
            "Asset path must identify a file inside the project.");
    }

    for (const auto& part : normalized)
    {
        if (part == std::filesystem::path(".."))
        {
            return Result<std::filesystem::path>::Failure(
                ErrorCode::InvalidArgument,
                "Asset path cannot escape the project root.");
        }
    }

    return Result<std::filesystem::path>::Success(normalized);
}

std::string AssetRegistry::PathKey(
    const std::filesystem::path& normalizedPath)
{
    return normalizedPath.generic_string();
}

} // namespace Janus
