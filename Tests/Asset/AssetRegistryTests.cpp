#include "Asset/AssetRegistry.h"
#include "Core/FileSystem/FileSystem.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace
{

class AssetTempDirectory final
{
public:
    AssetTempDirectory()
    {
        const auto leaf =
            "janus-asset-registry-"
            + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count());
        m_Path = std::filesystem::temp_directory_path() / leaf;

        std::error_code error;
        if (!std::filesystem::create_directory(m_Path, error) || error)
        {
            throw std::runtime_error("Failed to create asset test directory.");
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

} // namespace

TEST_CASE("AssetRegistry registers and resolves normalized paths", "[asset][registry]")
{
    Janus::AssetRegistry registry;

    auto handle = registry.Register(
        Janus::AssetType::Texture,
        std::filesystem::path("Assets/Characters/../player.png"));

    REQUIRE(handle);
    REQUIRE(registry.Size() == 1);
    REQUIRE(registry.Contains(handle.Value()));

    const auto* byHandle = registry.Find(handle.Value());
    REQUIRE(byHandle != nullptr);
    REQUIRE(byHandle->type == Janus::AssetType::Texture);
    REQUIRE(byHandle->relativePath == std::filesystem::path("Assets/player.png"));

    const auto* byPath = registry.FindByPath("Assets/./player.png");
    REQUIRE(byPath != nullptr);
    REQUIRE(byPath->handle == handle.Value());
}

TEST_CASE("AssetRegistry rejects duplicate handles and paths", "[asset][registry]")
{
    Janus::AssetRegistry registry;
    const Janus::AssetHandle handle = Janus::AssetHandle::Random();

    REQUIRE(registry.Register(Janus::AssetMetadata{
        handle,
        Janus::AssetType::Texture,
        std::filesystem::path("Assets/player.png")}));

    const auto duplicateHandle = registry.Register(Janus::AssetMetadata{
        handle,
        Janus::AssetType::ShaderSource,
        std::filesystem::path("Assets/player.glsl")});
    REQUIRE_FALSE(duplicateHandle);

    const auto duplicatePath = registry.Register(Janus::AssetMetadata{
        Janus::AssetHandle::Random(),
        Janus::AssetType::Texture,
        std::filesystem::path("Assets/./player.png")});
    REQUIRE_FALSE(duplicatePath);

    REQUIRE(registry.Size() == 1);
}

TEST_CASE("AssetRegistry rejects absolute and escaping paths", "[asset][registry]")
{
    Janus::AssetRegistry registry;

    const auto absolute = registry.Register(
        Janus::AssetType::Texture,
        std::filesystem::temp_directory_path() / "outside.png");
    const auto escaping = registry.Register(
        Janus::AssetType::Texture,
        std::filesystem::path("../outside.png"));
    const auto empty = registry.Register(
        Janus::AssetType::Texture,
        std::filesystem::path{});

    REQUIRE_FALSE(absolute);
    REQUIRE_FALSE(escaping);
    REQUIRE_FALSE(empty);
    REQUIRE(registry.Size() == 0);
}

TEST_CASE("AssetRegistry persists deterministic project-relative metadata", "[asset][registry]")
{
    AssetTempDirectory temp;
    const auto registryPath = temp.Path() / "AssetRegistry.json";

    Janus::AssetRegistry registry;
    const auto shader = registry.Register(
        Janus::AssetType::ShaderSource,
        std::filesystem::path("Assets/Shaders/sprite.glsl"));
    const auto texture = registry.Register(
        Janus::AssetType::Texture,
        std::filesystem::path("Assets/Textures/player.png"));

    REQUIRE(shader);
    REQUIRE(texture);
    REQUIRE(registry.Save(registryPath));

    auto loaded = Janus::AssetRegistry::Load(registryPath);
    REQUIRE(loaded);
    REQUIRE(loaded.Value().Size() == 2);

    const auto* loadedShader = loaded.Value().Find(shader.Value());
    const auto* loadedTexture = loaded.Value().Find(texture.Value());
    REQUIRE(loadedShader != nullptr);
    REQUIRE(loadedTexture != nullptr);
    REQUIRE(loadedShader->type == Janus::AssetType::ShaderSource);
    REQUIRE(loadedTexture->type == Janus::AssetType::Texture);
    REQUIRE(
        loadedTexture->relativePath
        == std::filesystem::path("Assets/Textures/player.png"));

    const auto serialized = Janus::FileSystem::ReadText(registryPath);
    REQUIRE(serialized);
    REQUIRE(serialized.Value().find("janus.asset-registry") != std::string::npos);
    REQUIRE(serialized.Value().find("\\\\") == std::string::npos);
}

TEST_CASE("AssetRegistry rejects malformed and unsupported registry files", "[asset][registry]")
{
    AssetTempDirectory temp;
    const auto malformedPath = temp.Path() / "malformed.json";
    const auto versionPath = temp.Path() / "version.json";
    const auto duplicatePath = temp.Path() / "duplicate.json";

    REQUIRE(Janus::FileSystem::WriteText(malformedPath, "{broken"));
    REQUIRE(Janus::FileSystem::WriteText(
        versionPath,
        R"({"schema":"janus.asset-registry","version":2,"assets":[]})"));

    const Janus::AssetHandle handle = Janus::AssetHandle::Random();
    const std::string duplicateDocument =
        std::string("{\"schema\":\"janus.asset-registry\",\"version\":1,\"assets\":[")
        + "{\"handle\":\"" + handle.ToString()
        + "\",\"type\":\"texture\",\"path\":\"Assets/a.png\"},"
        + "{\"handle\":\"" + handle.ToString()
        + "\",\"type\":\"texture\",\"path\":\"Assets/b.png\"}]}";
    REQUIRE(Janus::FileSystem::WriteText(duplicatePath, duplicateDocument));

    REQUIRE_FALSE(Janus::AssetRegistry::Load(malformedPath));
    REQUIRE_FALSE(Janus::AssetRegistry::Load(versionPath));
    REQUIRE_FALSE(Janus::AssetRegistry::Load(duplicatePath));
}
