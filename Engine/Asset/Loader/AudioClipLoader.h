#pragma once
#include "Asset/AudioClip.h"
#include "Core/Error/Result.h"
#include <filesystem>
#include <span>

namespace Janus
{
class AudioClipLoader
{
  public:
    static constexpr usize MaxFileBytes = 32 * 1024 * 1024;
    [[nodiscard]] static Result<AudioClip> Parse(std::span<const u8> bytes);
    [[nodiscard]] static Result<AudioClip> Load(const std::filesystem::path& path);
};
} // namespace Janus
