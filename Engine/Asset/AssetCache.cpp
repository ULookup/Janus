#include "Asset/AssetCache.h"

#include <utility>

namespace Janus
{
const AnimationClip* AssetCache::FindAnimationClip(AssetHandle handle) const noexcept
{
    const auto found = m_Animations.find(handle);
    return found == m_Animations.end() ? nullptr : &found->second;
}
bool AssetCache::StoreAnimationClip(AssetHandle handle, AnimationClip clip)
{
    if (!handle.IsValid() || Contains(handle) || !clip.texture.IsValid() || clip.frames.empty())
        return false;
    return m_Animations.emplace(handle, std::move(clip)).second;
}
bool AssetCache::RemoveAnimationClip(AssetHandle handle) noexcept
{
    return m_Animations.erase(handle) != 0;
}
usize AssetCache::RemoveAnimationsForTexture(AssetHandle texture) noexcept
{
    return std::erase_if(m_Animations,
                         [texture](const auto& entry) { return entry.second.texture == texture; });
}

const FontAsset* AssetCache::FindFont(AssetHandle handle) const noexcept
{
    const auto found = m_Fonts.find(handle);
    return found == m_Fonts.end() ? nullptr : &found->second;
}
bool AssetCache::StoreFont(AssetHandle handle, FontAsset font)
{
    if (!handle.IsValid() || Contains(handle) || !font.atlas.IsValid())
        return false;
    return m_Fonts.emplace(handle, std::move(font)).second;
}
bool AssetCache::RemoveFont(AssetHandle handle) noexcept
{
    return m_Fonts.erase(handle) != 0;
}
usize AssetCache::RemoveFontsForAtlas(AssetHandle atlas) noexcept
{
    return std::erase_if(m_Fonts,
                         [atlas](const auto& entry) { return entry.second.atlas == atlas; });
}

const TextureHandle* AssetCache::FindTexture(
    AssetHandle handle) const noexcept
{
    const auto iterator = m_Textures.find(handle);
    return iterator == m_Textures.end() ? nullptr : &iterator->second;
}

const std::string* AssetCache::FindShaderSource(
    AssetHandle handle) const noexcept
{
    const auto iterator = m_ShaderSources.find(handle);
    return iterator == m_ShaderSources.end() ? nullptr : &iterator->second;
}

const std::string* AssetCache::FindLuaScriptSource(
    AssetHandle handle) const noexcept
{
    const auto iterator = m_LuaScriptSources.find(handle);
    return iterator == m_LuaScriptSources.end() ? nullptr : &iterator->second;
}

bool AssetCache::StoreTexture(
    AssetHandle handle,
    TextureHandle texture)
{
    if (!handle.IsValid()
        || texture.value == 0
        || Contains(handle))
    {
        return false;
    }

    return m_Textures.emplace(handle, texture).second;
}

bool AssetCache::StoreShaderSource(
    AssetHandle handle,
    std::string source)
{
    if (!handle.IsValid() || Contains(handle))
    {
        return false;
    }

    return m_ShaderSources.emplace(handle, std::move(source)).second;
}

bool AssetCache::StoreLuaScriptSource(
    AssetHandle handle,
    std::string source)
{
    if (!handle.IsValid() || Contains(handle))
    {
        return false;
    }

    return m_LuaScriptSources.emplace(handle, std::move(source)).second;
}

std::optional<TextureHandle> AssetCache::RemoveTexture(
    AssetHandle handle) noexcept
{
    const auto iterator = m_Textures.find(handle);
    if (iterator == m_Textures.end())
    {
        return std::nullopt;
    }

    const TextureHandle texture = iterator->second;
    m_Textures.erase(iterator);
    return texture;
}

bool AssetCache::RemoveShaderSource(AssetHandle handle) noexcept
{
    return m_ShaderSources.erase(handle) != 0;
}

bool AssetCache::RemoveLuaScriptSource(AssetHandle handle) noexcept
{
    return m_LuaScriptSources.erase(handle) != 0;
}

bool AssetCache::Contains(AssetHandle handle) const noexcept
{
    return m_Textures.contains(handle) || m_ShaderSources.contains(handle) ||
           m_LuaScriptSources.contains(handle) || m_Fonts.contains(handle) ||
           m_Animations.contains(handle);
}

usize AssetCache::TextureCount() const noexcept
{
    return m_Textures.size();
}

usize AssetCache::ShaderSourceCount() const noexcept
{
    return m_ShaderSources.size();
}

usize AssetCache::LuaScriptSourceCount() const noexcept
{
    return m_LuaScriptSources.size();
}

void AssetCache::Clear() noexcept
{
    m_Animations.clear();
    m_Textures.clear();
    m_Fonts.clear();
    m_ShaderSources.clear();
    m_LuaScriptSources.clear();
}

} // namespace Janus
