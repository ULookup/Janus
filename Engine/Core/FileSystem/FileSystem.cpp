#include "Core/FileSystem/FileSystem.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <limits>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace Janus::FileSystem
{
namespace
{

std::atomic<u64> g_AtomicWriteCounter{0};

Result<std::vector<u8>> ReadBytes(const std::filesystem::path& path)
{
    std::error_code error;
    if (!std::filesystem::exists(path, error))
    {
        if (error)
        {
            return Result<std::vector<u8>>::Failure(
                ErrorCode::FileReadFailed,
                "Failed to check file '" + path.string() + "': " + error.message() + ".");
        }

        return Result<std::vector<u8>>::Failure(
            ErrorCode::FileNotFound,
            "File not found '" + path.string() + "'.");
    }

    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        return Result<std::vector<u8>>::Failure(
            ErrorCode::FileReadFailed,
            "Failed to open file for reading '" + path.string() + "'.");
    }

    const auto end = stream.tellg();
    if (end < std::streampos{0})
    {
        return Result<std::vector<u8>>::Failure(
            ErrorCode::FileReadFailed,
            "Failed to determine size of file '" + path.string() + "'.");
    }

    const auto size = static_cast<std::uintmax_t>(end);
    const auto maxStreamSize =
        static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max());
    const auto maxSize = static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max());
    const auto maxVectorSize = static_cast<std::uintmax_t>(std::vector<u8>{}.max_size());
    if (size > maxStreamSize || size > maxSize || size > maxVectorSize)
    {
        return Result<std::vector<u8>>::Failure(
            ErrorCode::FileReadFailed,
            "Failed to determine size of file '" + path.string() + "'.");
    }

    std::vector<u8> contents(static_cast<std::size_t>(size));
    stream.seekg(0);
    if (!stream)
    {
        return Result<std::vector<u8>>::Failure(
            ErrorCode::FileReadFailed,
            "Failed to seek file '" + path.string() + "'.");
    }

    if (!contents.empty())
    {
        stream.read(
            reinterpret_cast<char*>(contents.data()),
            static_cast<std::streamsize>(contents.size()));
    }
    if (!stream)
    {
        return Result<std::vector<u8>>::Failure(
            ErrorCode::FileReadFailed,
            "Failed to read file '" + path.string() + "'.");
    }

    return Result<std::vector<u8>>::Success(std::move(contents));
}

[[nodiscard]] std::filesystem::path MakeAtomicTemporaryPath(
    const std::filesystem::path& path)
{
    const auto timestamp = static_cast<u64>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const u64 counter = g_AtomicWriteCounter.fetch_add(1, std::memory_order_relaxed);

    std::filesystem::path temporary = path;
    temporary += ".tmp.";
    temporary += std::to_string(timestamp);
    temporary += ".";
    temporary += std::to_string(counter);
    return temporary;
}

void RemoveBestEffort(const std::filesystem::path& path) noexcept
{
    std::error_code error;
    std::filesystem::remove(path, error);
}

Result<void> ReplaceAtomically(
    const std::filesystem::path& temporary,
    const std::filesystem::path& target)
{
#if defined(_WIN32)
    const DWORD flags = MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH;
    if (::MoveFileExW(temporary.c_str(), target.c_str(), flags) == 0)
    {
        const DWORD error = ::GetLastError();
        return Result<void>::Failure(
            ErrorCode::FileWriteFailed,
            "Failed to atomically replace file '" + target.string()
                + "' (Win32 error "
                + std::to_string(static_cast<unsigned long>(error)) + ").");
    }
#else
    std::error_code error;
    std::filesystem::rename(temporary, target, error);
    if (error)
    {
        return Result<void>::Failure(
            ErrorCode::FileWriteFailed,
            "Failed to atomically replace file '" + target.string()
                + "': " + error.message() + ".");
    }
#endif

    return Result<void>::Success();
}

} // namespace

bool Exists(const std::filesystem::path& path) noexcept
{
    std::error_code error;
    return std::filesystem::exists(path, error);
}

Result<std::vector<u8>> ReadBinary(const std::filesystem::path& path)
{
    return ReadBytes(path);
}

Result<std::string> ReadText(const std::filesystem::path& path)
{
    auto bytes = ReadBytes(path);
    if (!bytes)
    {
        return Result<std::string>::Failure(bytes.GetError());
    }

    const auto& value = bytes.Value();
    if (value.empty())
    {
        return Result<std::string>::Success(std::string{});
    }

    return Result<std::string>::Success(
        std::string(reinterpret_cast<const char*>(value.data()), value.size()));
}

Result<void> WriteBinary(
    const std::filesystem::path& path,
    std::span<const u8> contents)
{
    if (contents.size()
        > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
    {
        return Result<void>::Failure(
            ErrorCode::FileWriteFailed,
            "Failed to write file '" + path.string() + "': contents are too large.");
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        return Result<void>::Failure(
            ErrorCode::FileWriteFailed,
            "Failed to open file for writing '" + path.string() + "'.");
    }

    if (!contents.empty())
    {
        stream.write(
            reinterpret_cast<const char*>(contents.data()),
            static_cast<std::streamsize>(contents.size()));
    }
    stream.flush();

    if (!stream)
    {
        return Result<void>::Failure(
            ErrorCode::FileWriteFailed,
            "Failed to write file '" + path.string() + "'.");
    }

    return Result<void>::Success();
}

Result<void> WriteText(
    const std::filesystem::path& path,
    std::string_view contents)
{
    return WriteBinary(
        path,
        std::span<const u8>(
            reinterpret_cast<const u8*>(contents.data()), contents.size()));
}

Result<void> WriteBinaryAtomic(
    const std::filesystem::path& path,
    std::span<const u8> contents)
{
    const std::filesystem::path temporary = MakeAtomicTemporaryPath(path);

    auto write = WriteBinary(temporary, contents);
    if (!write)
    {
        RemoveBestEffort(temporary);
        return Result<void>::Failure(write.GetError());
    }

    auto replace = ReplaceAtomically(temporary, path);
    if (!replace)
    {
        RemoveBestEffort(temporary);
        return replace;
    }

    return Result<void>::Success();
}

Result<void> WriteTextAtomic(
    const std::filesystem::path& path,
    std::string_view contents)
{
    return WriteBinaryAtomic(
        path,
        std::span<const u8>(
            reinterpret_cast<const u8*>(contents.data()), contents.size()));
}

} // namespace Janus::FileSystem
