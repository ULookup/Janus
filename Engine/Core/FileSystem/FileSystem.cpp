#include "Core/FileSystem/FileSystem.h"

#include <fstream>
#include <limits>

namespace Janus::FileSystem
{
namespace
{

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
    if (end < std::streampos{ 0 }
        || static_cast<std::uintmax_t>(end)
            > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
    {
        return Result<std::vector<u8>>::Failure(
            ErrorCode::FileReadFailed,
            "Failed to determine size of file '" + path.string() + "'.");
    }

    std::vector<u8> contents(static_cast<std::size_t>(end));
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
    return Result<std::string>::Success(
        std::string(reinterpret_cast<const char*>(value.data()), value.size()));
}

Result<void> WriteBinary(
    const std::filesystem::path& path,
    std::span<const u8> contents)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        return Result<void>::Failure(
            ErrorCode::FileWriteFailed,
            "Failed to open file for writing '" + path.string() + "'.");
    }

    stream.write(
        reinterpret_cast<const char*>(contents.data()),
        static_cast<std::streamsize>(contents.size()));
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

} // namespace Janus::FileSystem
