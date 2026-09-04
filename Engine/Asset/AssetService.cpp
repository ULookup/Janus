#include "Asset/AssetService.h"

#include "Asset/AssetMetadata.h"
#include "Asset/AssetRegistry.h"
#include "Asset/Loader/LuaScriptSourceLoader.h"
#include "Asset/Loader/ShaderSourceLoader.h"
#include "Asset/Loader/TextureLoader.h"
#include "Renderer/Renderer2D.h"

#include <string>
#include <utility>

namespace Janus
{

AssetService::AssetService(
    std::filesystem::path projectRoot,
    const AssetRegistry& registry,
    Renderer2D& renderer)
    : m_ProjectRoot(std::move(projectRoot).lexically_normal()),
      m_Registry(registry),
      m_Renderer(renderer)
{
}

AssetService::~AssetService()
{
    Clear();
}

Result<TextureHandle> AssetService::LoadTexture(AssetHandle handle)
{
    if (const auto* cached = m_Cache.FindTexture(handle))
    {
        return Result<TextureHandle>::Success(*cached);
    }

    const AssetMetadata* metadata = m_Registry.Find(handle);
    if (metadata == nullptr)
    {
        return Result<TextureHandle>::Failure(
            ErrorCode::AssetNotFound,
            "Asset handle '" + handle.ToString() + "' is not registered.");
    }

    if (metadata->type != AssetType::Texture)
    {
        return Result<TextureHandle>::Failure(
            ErrorCode::AssetTypeMismatch,
            "Asset '" + metadata->relativePath.generic_string()
                + "' is not a texture.");
    }

    auto texture = TextureLoader::Load(
        ResolvePath(metadata->relativePath),
        m_Renderer);
    if (!texture)
    {
        return texture;
    }

    if (!m_Cache.StoreTexture(handle, texture.Value()))
    {
        m_Renderer.DestroyTexture(texture.Value());
        return Result<TextureHandle>::Failure(
            ErrorCode::InvalidState,
            "Failed to cache texture asset '"
                + metadata->relativePath.generic_string() + "'.");
    }

    return texture;
}

Result<std::string_view> AssetService::LoadShaderSource(AssetHandle handle)
{
    if (const auto* cached = m_Cache.FindShaderSource(handle))
    {
        return Result<std::string_view>::Success(std::string_view(*cached));
    }

    const AssetMetadata* metadata = m_Registry.Find(handle);
    if (metadata == nullptr)
    {
        return Result<std::string_view>::Failure(
            ErrorCode::AssetNotFound,
            "Asset handle '" + handle.ToString() + "' is not registered.");
    }

    if (metadata->type != AssetType::ShaderSource)
    {
        return Result<std::string_view>::Failure(
            ErrorCode::AssetTypeMismatch,
            "Asset '" + metadata->relativePath.generic_string()
                + "' is not shader source.");
    }

    auto source = ShaderSourceLoader::Load(
        ResolvePath(metadata->relativePath));
    if (!source)
    {
        return Result<std::string_view>::Failure(source.GetError());
    }

    if (!m_Cache.StoreShaderSource(
            handle,
            std::move(source).Value()))
    {
        return Result<std::string_view>::Failure(
            ErrorCode::InvalidState,
            "Failed to cache shader source asset '"
                + metadata->relativePath.generic_string() + "'.");
    }

    const auto* cached = m_Cache.FindShaderSource(handle);
    if (cached == nullptr)
    {
        return Result<std::string_view>::Failure(
            ErrorCode::InvalidState,
            "Shader source cache lost asset '"
                + metadata->relativePath.generic_string() + "'.");
    }

    return Result<std::string_view>::Success(std::string_view(*cached));
}

Result<std::string_view> AssetService::LoadLuaScriptSource(AssetHandle handle)
{
    if (const auto* cached = m_Cache.FindLuaScriptSource(handle))
    {
        return Result<std::string_view>::Success(std::string_view(*cached));
    }

    const AssetMetadata* metadata = m_Registry.Find(handle);
    if (metadata == nullptr)
    {
        return Result<std::string_view>::Failure(
            ErrorCode::AssetNotFound,
            "Asset handle '" + handle.ToString() + "' is not registered.");
    }

    if (metadata->type != AssetType::LuaScript)
    {
        return Result<std::string_view>::Failure(
            ErrorCode::AssetTypeMismatch,
            "Asset '" + metadata->relativePath.generic_string()
                + "' is not a Lua script.");
    }

    auto source = LuaScriptSourceLoader::Load(
        ResolvePath(metadata->relativePath));
    if (!source)
    {
        return Result<std::string_view>::Failure(source.GetError());
    }

    if (!m_Cache.StoreLuaScriptSource(
            handle,
            std::move(source).Value()))
    {
        return Result<std::string_view>::Failure(
            ErrorCode::InvalidState,
            "Failed to cache Lua script asset '"
                + metadata->relativePath.generic_string() + "'.");
    }

    const auto* cached = m_Cache.FindLuaScriptSource(handle);
    if (cached == nullptr)
    {
        return Result<std::string_view>::Failure(
            ErrorCode::InvalidState,
            "Lua script source cache lost asset '"
                + metadata->relativePath.generic_string() + "'.");
    }

    return Result<std::string_view>::Success(std::string_view(*cached));
}

Result<std::filesystem::file_time_type> AssetService::GetLastWriteTime(
    AssetHandle handle) const
{
    const AssetMetadata* metadata = m_Registry.Find(handle);
    if (metadata == nullptr)
    {
        return Result<std::filesystem::file_time_type>::Failure(
            ErrorCode::AssetNotFound,
            "Asset handle '" + handle.ToString() + "' is not registered.");
    }

    const std::filesystem::path path = ResolvePath(metadata->relativePath);
    std::error_code error;
    const auto time = std::filesystem::last_write_time(path, error);
    if (error)
    {
        const ErrorCode code = std::filesystem::exists(path)
            ? ErrorCode::FileReadFailed
            : ErrorCode::FileNotFound;
        return Result<std::filesystem::file_time_type>::Failure(
            code,
            "Failed to read last-write time for asset '"
                + metadata->relativePath.generic_string() + "': "
                + error.message());
    }

    return Result<std::filesystem::file_time_type>::Success(time);
}

bool AssetService::IsLoaded(AssetHandle handle) const noexcept
{
    return m_Cache.Contains(handle);
}

bool AssetService::Unload(AssetHandle handle) noexcept
{
    if (auto texture = m_Cache.RemoveTexture(handle))
    {
        m_Renderer.DestroyTexture(*texture);
        return true;
    }

    if (m_Cache.RemoveShaderSource(handle))
    {
        return true;
    }

    return m_Cache.RemoveLuaScriptSource(handle);
}

void AssetService::Clear() noexcept
{
    m_Cache.ForEachTexture(
        [this](AssetHandle, TextureHandle texture)
        {
            m_Renderer.DestroyTexture(texture);
        });

    m_Cache.Clear();
}

std::filesystem::path AssetService::ResolvePath(
    const std::filesystem::path& relativePath) const
{
    return (m_ProjectRoot / relativePath).lexically_normal();
}

} // namespace Janus
