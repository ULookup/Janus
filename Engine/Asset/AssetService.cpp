#include "Asset/AssetService.h"

#include "Asset/AssetMetadata.h"
#include "Asset/AssetRegistry.h"
#include "Asset/Loader/FontLoader.h"
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

Result<const FontAsset*> AssetService::LoadFont(AssetHandle handle)
{
    using Output = Result<const FontAsset*>;
    if (const auto* cached = m_Cache.FindFont(handle))
        return Output::Success(cached);
    const auto* metadata = m_Registry.Find(handle);
    if (!metadata)
        return Output::Failure(ErrorCode::AssetNotFound, "Font asset is not registered.");
    if (metadata->type != AssetType::Font)
        return Output::Failure(ErrorCode::AssetTypeMismatch, "Asset is not a Font.");
    auto font = FontLoader::Load(ResolvePath(metadata->relativePath));
    if (!font)
        return Output::Failure(font.GetError());
    const auto* atlas = m_Registry.Find(font.Value().atlas);
    if (!atlas)
        return Output::Failure(ErrorCode::AssetNotFound, "Font atlas is not registered.");
    if (atlas->type != AssetType::Texture)
        return Output::Failure(ErrorCode::AssetTypeMismatch, "Font atlas must be a Texture.");
    auto size = TextureLoader::ReadDimensions(ResolvePath(atlas->relativePath));
    if (!size)
        return Output::Failure(size.GetError());
    if (size.Value().width != font.Value().width || size.Value().height != font.Value().height)
        return Output::Failure(ErrorCode::AssetDecodeFailed,
                               "Font atlas dimensions do not match metadata.");
    if (!m_Cache.StoreFont(handle, std::move(font).Value()))
        return Output::Failure(ErrorCode::InvalidState, "Failed to cache Font asset.");
    return Output::Success(m_Cache.FindFont(handle));
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
    const bool removedFont = m_Cache.RemoveFont(handle);
    // Invalidating an atlas also invalidates dependent metrics/dimension validation.
    const bool invalidated = m_Cache.RemoveFontsForAtlas(handle) != 0;
    if (auto texture = m_Cache.RemoveTexture(handle))
    {
        m_Renderer.DestroyTexture(*texture);
        return true;
    }

    if (m_Cache.RemoveShaderSource(handle))
    {
        return true;
    }

    return m_Cache.RemoveLuaScriptSource(handle) || removedFont || invalidated;
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
