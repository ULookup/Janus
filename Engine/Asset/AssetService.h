#pragma once

#include "Asset/AssetCache.h"
#include "Asset/AssetHandle.h"
#include "Core/Error/Result.h"
#include "Renderer/RendererTypes.h"

#include <filesystem>
#include <string_view>

namespace Janus
{

class AssetRegistry;
class Renderer2D;

class AssetService
{
public:
    AssetService(
        std::filesystem::path projectRoot,
        const AssetRegistry& registry,
        Renderer2D& renderer);
    ~AssetService();

    AssetService(const AssetService&) = delete;
    AssetService& operator=(const AssetService&) = delete;
    AssetService(AssetService&&) = delete;
    AssetService& operator=(AssetService&&) = delete;

    [[nodiscard]] Result<TextureHandle> LoadTexture(AssetHandle handle);
    [[nodiscard]] Result<std::string_view> LoadShaderSource(AssetHandle handle);
    [[nodiscard]] Result<std::string_view> LoadLuaScriptSource(AssetHandle handle);

    [[nodiscard]] bool IsLoaded(AssetHandle handle) const noexcept;
    bool Unload(AssetHandle handle) noexcept;
    void Clear() noexcept;

private:
    [[nodiscard]] std::filesystem::path ResolvePath(
        const std::filesystem::path& relativePath) const;

    std::filesystem::path m_ProjectRoot;
    const AssetRegistry& m_Registry;
    Renderer2D& m_Renderer;
    AssetCache m_Cache;
};

} // namespace Janus
