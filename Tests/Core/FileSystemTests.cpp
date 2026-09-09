#include "Core/FileSystem/FileSystem.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
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

TEST_CASE("Atomic create never replaces an existing destination",
          "[core][filesystem][asset-workflow]")
{
    TempDirectory temp;
    const auto path = temp.Path() / "new.txt";
    REQUIRE(Janus::FileSystem::WriteTextAtomic(path, "original",
                                               Janus::FileSystem::AtomicWriteMode::CreateNew));
    REQUIRE_FALSE(Janus::FileSystem::WriteTextAtomic(
        path, "replacement", Janus::FileSystem::AtomicWriteMode::CreateNew));
    REQUIRE(Janus::FileSystem::ReadText(path).Value() == "original");
    REQUIRE(std::distance(std::filesystem::directory_iterator(temp.Path()),
                          std::filesystem::directory_iterator{}) == 1);
}

TEST_CASE("FileSystem errors preserve UTF8 paths and clean failed atomic writes",
          "[filesystem][e1]")
{
    TempDirectory temp;
    const auto path = temp.Path() / std::filesystem::path(u8"新场景.scene");
    const auto utf8 = Janus::FileSystem::PathToUtf8(path);
    const auto missing = Janus::FileSystem::ReadText(path);
    REQUIRE_FALSE(missing);
    CHECK(missing.GetError().message.find(utf8) != std::string::npos);
    REQUIRE(Janus::FileSystem::WriteTextAtomic(path, "original"));
    const auto conflict = Janus::FileSystem::WriteTextAtomic(
        path, "replacement", Janus::FileSystem::AtomicWriteMode::CreateNew);
    REQUIRE_FALSE(conflict);
    CHECK(conflict.GetError().message.find(utf8) != std::string::npos);
    CHECK(Janus::FileSystem::ReadText(path).Value() == "original");
    CHECK(std::distance(std::filesystem::directory_iterator(temp.Path()),
                        std::filesystem::directory_iterator{}) == 1);
}

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

TEST_CASE("Concurrent atomic creators publish exactly one complete file",
          "[filesystem][asset-workflow]")
{
    TempDirectory temp;
    const auto path = temp.Path() / "contended.txt";
    std::array<bool, 8> succeeded{};
    std::vector<std::thread> writers;
    for (int i = 0; i < 8; ++i)
        writers.emplace_back(
            [&, i]
            {
                succeeded[i] = static_cast<bool>(Janus::FileSystem::WriteTextAtomic(
                    path, std::string(4096, static_cast<char>('a' + i)),
                    Janus::FileSystem::AtomicWriteMode::CreateNew));
            });
    for (auto& writer : writers)
        writer.join();
    int winners = 0;
    for (int i = 0; i < 8; ++i)
        if (succeeded[i])
        {
            ++winners;
            REQUIRE(Janus::FileSystem::ReadText(path).Value() ==
                    std::string(4096, static_cast<char>('a' + i)));
        }
    REQUIRE(winners == 1);
    REQUIRE(std::distance(std::filesystem::directory_iterator(temp.Path()),
                          std::filesystem::directory_iterator{}) == 1);
}
