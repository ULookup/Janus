#pragma once

#include "Core/Error/Result.h"
#include "Renderer/RendererTypes.h"

#include <filesystem>

namespace Janus
{

class Renderer2D;

class TextureLoader
{
public:
    [[nodiscard]] static Result<Viewport> ReadDimensions(const std::filesystem::path& path);
    [[nodiscard]] static Result<TextureHandle> Load(
        const std::filesystem::path& path,
        Renderer2D& renderer);
};

} // namespace Janus
