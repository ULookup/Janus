#include "Asset/Loader/TextureLoader.h"

#include "Core/FileSystem/FileSystem.h"
#include "Renderer/Renderer2D.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <limits>
#include <memory>
#include <string>

namespace Janus
{

Result<TextureHandle> TextureLoader::Load(
    const std::filesystem::path& path,
    Renderer2D& renderer)
{
    auto bytes = FileSystem::ReadBinary(path);
    if (!bytes)
    {
        return Result<TextureHandle>::Failure(bytes.GetError());
    }

    if (bytes.Value().empty()
        || bytes.Value().size() > static_cast<usize>(std::numeric_limits<int>::max()))
    {
        return Result<TextureHandle>::Failure(
            ErrorCode::AssetDecodeFailed,
            "Failed to decode texture asset '" + path.string()
                + "': file is empty or too large for the image decoder.");
    }

    int width = 0;
    int height = 0;
    int sourceChannels = 0;

    stbi_uc* rawPixels = stbi_load_from_memory(
        bytes.Value().data(),
        static_cast<int>(bytes.Value().size()),
        &width,
        &height,
        &sourceChannels,
        STBI_rgb_alpha);

    using PixelOwner = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
    PixelOwner pixels(rawPixels, &stbi_image_free);

    if (!pixels || width <= 0 || height <= 0)
    {
        const char* reason = stbi_failure_reason();
        return Result<TextureHandle>::Failure(
            ErrorCode::AssetDecodeFailed,
            "Failed to decode texture asset '" + path.string() + "': "
                + (reason == nullptr ? std::string("unknown decoder error") : std::string(reason))
                + ".");
    }

    constexpr usize Channels = 4;
    const usize widthValue = static_cast<usize>(width);
    const usize heightValue = static_cast<usize>(height);
    if (heightValue > std::numeric_limits<usize>::max() / widthValue
        || widthValue * heightValue > std::numeric_limits<usize>::max() / Channels)
    {
        return Result<TextureHandle>::Failure(
            ErrorCode::AssetDecodeFailed,
            "Failed to decode texture asset '" + path.string()
                + "': decoded dimensions overflow the texture size.");
    }

    TextureDesc desc;
    desc.width = static_cast<u32>(width);
    desc.height = static_cast<u32>(height);
    desc.data = pixels.get();
    desc.dataSize = widthValue * heightValue * Channels;

    auto texture = renderer.CreateTexture(desc);
    if (!texture)
    {
        return Result<TextureHandle>::Failure(
            texture.GetError().code,
            "Failed to create runtime texture for asset '" + path.string()
                + "': " + texture.GetError().message);
    }

    return texture;
}

} // namespace Janus
