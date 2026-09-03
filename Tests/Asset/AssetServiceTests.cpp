#include "Asset/AssetRegistry.h"
#include "Asset/AssetService.h"
#include "Core/FileSystem/FileSystem.h"
#include "Renderer/Renderer2D.h"

#include "Asset/AssetTestUtils.h"
#include "Renderer/FakeRenderDevice.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

namespace
{

void PrepareAssetDirectory(const std::filesystem::path& projectRoot)
{
    std::error_code error;
    REQUIRE(std::filesystem::create_directories(projectRoot / "Assets", error));
    REQUIRE_FALSE(error);
}

void CopyTextureFixture(
    const std::filesystem::path& projectRoot,
    const std::filesystem::path& relativePath)
{
    const auto destination = projectRoot / relativePath;
    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);
    REQUIRE_FALSE(error);

    REQUIRE(std::filesystem::copy_file(
        Janus::Test::AssetFixturePath("test_rgba.png"),
        destination,
        std::filesystem::copy_options::overwrite_existing,
        error));
    REQUIRE_FALSE(error);
}

} // namespace

TEST_CASE("AssetService validates handles and asset types", "[asset][service]")
{
    Janus::Test::AssetTempDirectory temp;
    Janus::AssetRegistry registry;

    const auto texture = registry.Register(
        Janus::AssetType::Texture,
        "Assets/test.png");
    const auto shader = registry.Register(
        Janus::AssetType::ShaderSource,
        "Assets/test.glsl");
    REQUIRE(texture);
    REQUIRE(shader);

    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::AssetService service(temp.Path(), registry, *renderer);

    const auto missing = service.LoadTexture(Janus::AssetHandle::Random());
    const auto wrongTextureType = service.LoadTexture(shader.Value());
    const auto wrongShaderType = service.LoadShaderSource(texture.Value());

    REQUIRE_FALSE(missing);
    REQUIRE(missing.GetError().code == Janus::ErrorCode::AssetNotFound);
    REQUIRE_FALSE(wrongTextureType);
    REQUIRE(wrongTextureType.GetError().code == Janus::ErrorCode::AssetTypeMismatch);
    REQUIRE_FALSE(wrongShaderType);
    REQUIRE(wrongShaderType.GetError().code == Janus::ErrorCode::AssetTypeMismatch);
    REQUIRE(device.createdTextures.empty());
}

TEST_CASE("AssetService caches textures and unload destroys runtime resources", "[asset][service]")
{
    Janus::Test::AssetTempDirectory temp;
    PrepareAssetDirectory(temp.Path());
    CopyTextureFixture(temp.Path(), "Assets/test.png");

    Janus::AssetRegistry registry;
    const auto texture = registry.Register(
        Janus::AssetType::Texture,
        "Assets/test.png");
    REQUIRE(texture);

    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::AssetService service(temp.Path(), registry, *renderer);

    const auto first = service.LoadTexture(texture.Value());
    const auto second = service.LoadTexture(texture.Value());

    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(first.Value().value == second.Value().value);
    REQUIRE(device.createdTextures.size() == 1);
    REQUIRE(service.IsLoaded(texture.Value()));

    REQUIRE(service.Unload(texture.Value()));
    REQUIRE_FALSE(service.IsLoaded(texture.Value()));
    REQUIRE(device.destroyedTextures.size() == 1);
    REQUIRE(device.destroyedTextures.front().value == first.Value().value);
    REQUIRE_FALSE(service.Unload(texture.Value()));

    const auto reloaded = service.LoadTexture(texture.Value());
    REQUIRE(reloaded);
    REQUIRE(reloaded.Value().value != first.Value().value);
    REQUIRE(device.createdTextures.size() == 2);
}

TEST_CASE("AssetService caches shader source until unload", "[asset][service]")
{
    Janus::Test::AssetTempDirectory temp;
    PrepareAssetDirectory(temp.Path());
    const auto shaderPath = temp.Path() / "Assets/test.glsl";
    REQUIRE(Janus::FileSystem::WriteText(shaderPath, "first"));

    Janus::AssetRegistry registry;
    const auto shader = registry.Register(
        Janus::AssetType::ShaderSource,
        "Assets/test.glsl");
    REQUIRE(shader);

    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::AssetService service(temp.Path(), registry, *renderer);

    const auto first = service.LoadShaderSource(shader.Value());
    REQUIRE(first);
    REQUIRE(first.Value() == "first");

    REQUIRE(Janus::FileSystem::WriteText(shaderPath, "second"));
    const auto cached = service.LoadShaderSource(shader.Value());
    REQUIRE(cached);
    REQUIRE(cached.Value() == "first");

    REQUIRE(service.Unload(shader.Value()));
    const auto reloaded = service.LoadShaderSource(shader.Value());
    REQUIRE(reloaded);
    REQUIRE(reloaded.Value() == "second");
}

TEST_CASE("AssetService clear and destructor release textures exactly once", "[asset][service]")
{
    Janus::Test::AssetTempDirectory temp;
    PrepareAssetDirectory(temp.Path());
    CopyTextureFixture(temp.Path(), "Assets/a.png");
    CopyTextureFixture(temp.Path(), "Assets/b.png");

    Janus::AssetRegistry registry;
    const auto first = registry.Register(
        Janus::AssetType::Texture,
        "Assets/a.png");
    const auto second = registry.Register(
        Janus::AssetType::Texture,
        "Assets/b.png");
    REQUIRE(first);
    REQUIRE(second);

    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);

    {
        Janus::AssetService service(temp.Path(), registry, *renderer);
        REQUIRE(service.LoadTexture(first.Value()));
        REQUIRE(service.LoadTexture(second.Value()));
        REQUIRE(device.createdTextures.size() == 2);
        REQUIRE(device.destroyedTextures.empty());

        service.Clear();
        REQUIRE(device.destroyedTextures.size() == 2);
        REQUIRE_FALSE(service.IsLoaded(first.Value()));
        REQUIRE_FALSE(service.IsLoaded(second.Value()));
    }

    REQUIRE(device.destroyedTextures.size() == 2);
}
