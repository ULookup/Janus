#pragma once

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace Janus::Test
{

class AssetTempDirectory final
{
public:
    AssetTempDirectory()
    {
        const auto leaf =
            "janus-asset-runtime-"
            + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count());
        m_Path = std::filesystem::temp_directory_path() / leaf;

        std::error_code error;
        if (!std::filesystem::create_directory(m_Path, error) || error)
        {
            throw std::runtime_error("Failed to create asset runtime test directory.");
        }
    }

    ~AssetTempDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(m_Path, error);
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept
    {
        return m_Path;
    }

private:
    std::filesystem::path m_Path;
};

[[nodiscard]] inline std::filesystem::path AssetFixturePath(
    const std::filesystem::path& relativePath)
{
    return std::filesystem::path(JANUS_TEST_SOURCE_DIR)
        / "Fixtures"
        / "Assets"
        / relativePath;
}

} // namespace Janus::Test
