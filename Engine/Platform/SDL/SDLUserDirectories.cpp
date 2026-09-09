#include "Platform/UserDirectories.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include <memory>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Janus::Platform
{
Result<std::filesystem::path> GetUserDataDirectory()
{
    const std::unique_ptr<char, decltype(&SDL_free)> path(SDL_GetPrefPath("Janus", "JanusEditor"),
                                                          &SDL_free);
    if (!path)
        return Result<std::filesystem::path>::Failure(
            ErrorCode::FileWriteFailed,
            std::string("User data directory unavailable: ") + SDL_GetError());
    const std::string text(path.get());
    return Result<std::filesystem::path>::Success(
        std::filesystem::path(std::u8string(text.begin(), text.end())));
}

Result<std::filesystem::path> GetSystemFontDirectory()
{
#ifdef _WIN32
    std::wstring windowsDirectory(32768, L'\0');
    const auto count =
        GetWindowsDirectoryW(windowsDirectory.data(), static_cast<UINT>(windowsDirectory.size()));
    if (count == 0 || count >= windowsDirectory.size())
        return Result<std::filesystem::path>::Failure(ErrorCode::FileNotFound,
                                                      "Windows font directory unavailable.");
    windowsDirectory.resize(count);
    return Result<std::filesystem::path>::Success(std::filesystem::path(windowsDirectory) /
                                                  "Fonts");
#else
    return Result<std::filesystem::path>::Failure(
        ErrorCode::FileNotFound, "System font discovery is unavailable on this platform.");
#endif
}
} // namespace Janus::Platform
