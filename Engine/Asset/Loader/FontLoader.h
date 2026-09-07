#pragma once
#include "Asset/FontAsset.h"
#include <filesystem>
#include <string_view>

namespace Janus
{
class FontLoader final
{
  public:
    [[nodiscard]] static Result<FontAsset> Parse(std::string_view source);
    [[nodiscard]] static Result<FontAsset> Load(const std::filesystem::path& path);
};
} // namespace Janus
