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

// UTF-8 at JSON/UI boundaries, independent of the Windows ANSI code page.
[[nodiscard]] inline std::string PathToUtf8(const std::filesystem::path& path)
{
    const auto text = path.generic_u8string();
    return std::string(text.begin(), text.end());
}

enum class AtomicWriteMode
{
    Replace,
    CreateNew
};

[[nodiscard]] bool Exists(const std::filesystem::path& path) noexcept;
[[nodiscard]] Result<std::string> ReadText(const std::filesystem::path& path);
[[nodiscard]] Result<std::vector<u8>> ReadBinary(const std::filesystem::path& path);
[[nodiscard]] Result<void> WriteText(
    const std::filesystem::path& path,
    std::string_view contents);
[[nodiscard]] Result<void> WriteBinary(
    const std::filesystem::path& path,
    std::span<const u8> contents);
[[nodiscard]] Result<void> WriteTextAtomic(const std::filesystem::path& path,
                                           std::string_view contents,
                                           AtomicWriteMode mode = AtomicWriteMode::Replace);
[[nodiscard]] Result<void> WriteBinaryAtomic(const std::filesystem::path& path,
                                             std::span<const u8> contents,
                                             AtomicWriteMode mode = AtomicWriteMode::Replace);

} // namespace Janus::FileSystem
