#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Types.h"
#include "Renderer/RendererTypes.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace Janus
{

class AssetCache
{
public:
    [[nodiscard]] const TextureHandle* FindTexture(
        AssetHandle handle) const noexcept;
    [[nodiscard]] const std::string* FindShaderSource(
        AssetHandle handle) const noexcept;

    bool StoreTexture(AssetHandle handle, TextureHandle texture);
    bool StoreShaderSource(AssetHandle handle, std::string source);

    [[nodiscard]] std::optional<TextureHandle> RemoveTexture(
        AssetHandle handle) noexcept;
    bool RemoveShaderSource(AssetHandle handle) noexcept;

    [[nodiscard]] bool Contains(AssetHandle handle) const noexcept;
    [[nodiscard]] usize TextureCount() const noexcept;
    [[nodiscard]] usize ShaderSourceCount() const noexcept;

    template <typename Function>
    void ForEachTexture(Function&& function) const
    {
        for (const auto& [handle, texture] : m_Textures)
        {
            function(handle, texture);
        }
    }

    void Clear() noexcept;

private:
    std::unordered_map<AssetHandle, TextureHandle, AssetHandleHash> m_Textures;
    std::unordered_map<AssetHandle, std::string, AssetHandleHash> m_ShaderSources;
};

} // namespace Janus
