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
    const std::array<Janus::u8, 4> bytes{0, 1, 127, 255};

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

TEST_CASE("FileSystem round trips empty text and binary", "[core][filesystem]")
{
    TempDirectory temp;
    const auto textPath = temp.Path() / "empty.txt";
    const auto binaryPath = temp.Path() / "empty.bin";
    const std::array<Janus::u8, 0> bytes{};

    REQUIRE(Janus::FileSystem::WriteText(textPath, ""));
    REQUIRE(Janus::FileSystem::WriteBinary(binaryPath, bytes));
    REQUIRE(Janus::FileSystem::ReadText(textPath).Value().empty());
    REQUIRE(Janus::FileSystem::ReadBinary(binaryPath).Value().empty());
}

TEST_CASE("FileSystem atomically replaces persistent text and binary", "[core][filesystem]")
{
    TempDirectory temp;
    const auto textPath = temp.Path() / "registry.json";
    const auto binaryPath = temp.Path() / "state.bin";
    const std::array<Janus::u8, 3> firstBytes{1, 2, 3};
    const std::array<Janus::u8, 4> secondBytes{4, 5, 6, 7};

    REQUIRE(Janus::FileSystem::WriteText(textPath, "before"));
    REQUIRE(Janus::FileSystem::WriteTextAtomic(textPath, "after"));
    REQUIRE(Janus::FileSystem::ReadText(textPath).Value() == "after");

    REQUIRE(Janus::FileSystem::WriteBinary(binaryPath, firstBytes));
    REQUIRE(Janus::FileSystem::WriteBinaryAtomic(binaryPath, secondBytes));
    REQUIRE(Janus::FileSystem::ReadBinary(binaryPath).Value()
            == std::vector<Janus::u8>(secondBytes.begin(), secondBytes.end()));

    for (const auto& entry : std::filesystem::directory_iterator(temp.Path()))
    {
        REQUIRE(entry.path().filename().string().find(".tmp.") == std::string::npos);
    }
}

TEST_CASE("FileSystem atomic write reports invalid destination", "[core][filesystem]")
{
    TempDirectory temp;
    const auto invalidPath = temp.Path() / "missing-parent" / "registry.json";

    const auto result = Janus::FileSystem::WriteTextAtomic(invalidPath, "data");

    REQUIRE_FALSE(result);
    REQUIRE(result.GetError().code == Janus::ErrorCode::FileWriteFailed);
    REQUIRE_FALSE(Janus::FileSystem::Exists(invalidPath));
}
