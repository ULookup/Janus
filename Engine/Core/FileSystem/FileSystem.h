#pragma once

#include "Core/Error/Result.h"
#include "Core/Types.h"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Janus::FileSystem
{

[[nodiscard]] bool Exists(const std::filesystem::path& path) noexcept;
[[nodiscard]] Result<std::string> ReadText(const std::filesystem::path& path);
[[nodiscard]] Result<std::vector<u8>> ReadBinary(const std::filesystem::path& path);
[[nodiscard]] Result<void> WriteText(
    const std::filesystem::path& path,
    std::string_view contents);
[[nodiscard]] Result<void> WriteBinary(
    const std::filesystem::path& path,
    std::span<const u8> contents);

} // namespace Janus::FileSystem
