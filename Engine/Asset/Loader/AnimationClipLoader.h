#pragma once
#include "Asset/AnimationClip.h"
#include "Core/Error/Result.h"
#include <filesystem>
#include <string_view>

namespace Janus
{
class AnimationClipLoader
{
  public:
    [[nodiscard]] static Result<AnimationClip> Parse(std::string_view source);
    [[nodiscard]] static Result<AnimationClip> Load(const std::filesystem::path& path);
};
} // namespace Janus
