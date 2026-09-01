#include "Core/FileSystem/FileSystem.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

class TempDirectory final
{
public:
    TempDirectory()
    {
        const auto leaf =
            "janus-filesystem-"
            + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count());
        m_Path = std::filesystem::temp_directory_path() / leaf;

        std::error_code error;
        if (!std::filesystem::create_directory(m_Path, error) || error)
        {
            throw std::runtime_error("Failed to create test directory.");
        }
    }

    ~TempDirectory()
    {
        std::error_code error;
        // The path is constructed as one known child of the OS temp directory.
        std::filesystem::remove_all(m_Path, error);
    }

    [[nodiscard]]
    const std::filesystem::path& Path() const noexcept
    {
        return m_Path;
    }

private:
    std::filesystem::path m_Path;
};

TEST_CASE("FileSystem round trips text and binary", "[core][filesystem]")
{
    TempDirectory temp;
    const auto textPath = temp.Path() / "sample.txt";
    const auto binaryPath = temp.Path() / "sample.bin";
    const std::array<Janus::u8, 4> bytes{ 0, 1, 127, 255 };

    REQUIRE(Janus::FileSystem::WriteText(textPath, "Janus"));
    REQUIRE(Janus::FileSystem::WriteBinary(binaryPath, bytes));
    REQUIRE(Janus::FileSystem::Exists(textPath));
    REQUIRE(Janus::FileSystem::ReadText(textPath).Value() == "Janus");
    REQUIRE(Janus::FileSystem::ReadBinary(binaryPath).Value()
            == std::vector<Janus::u8>(bytes.begin(), bytes.end()));
}

TEST_CASE("FileSystem reports missing reads and invalid writes", "[core][filesystem]")
{
    TempDirectory temp;

    const auto missing = Janus::FileSystem::ReadText(temp.Path() / "missing.txt");
    REQUIRE_FALSE(missing);
    REQUIRE(missing.GetError().code == Janus::ErrorCode::FileNotFound);

    const auto invalid =
        Janus::FileSystem::WriteText(temp.Path() / "missing-parent" / "x.txt", "x");
    REQUIRE_FALSE(invalid);
    REQUIRE(invalid.GetError().code == Janus::ErrorCode::FileWriteFailed);
}
