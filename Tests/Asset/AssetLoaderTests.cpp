#include "Asset/Loader/ShaderSourceLoader.h"
#include "Asset/Loader/TextureLoader.h"
#include "Core/FileSystem/FileSystem.h"
#include "Renderer/Renderer2D.h"

#include "AssetTestUtils.h"
#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

TEST_CASE("TextureLoader decodes RGBA8 through Renderer2D", "[asset][loader]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);

    auto texture = Janus::TextureLoader::Load(
        Janus::Test::AssetFixturePath("test_rgba.png"),
        *renderer);

    REQUIRE(texture);
    REQUIRE(texture.Value().value != 0);
    REQUIRE(device.createdTextures.size() == 1);

    const auto& created = device.createdTextures.front();
    REQUIRE(created.handle.value == texture.Value().value);
    REQUIRE(created.width == 2);
    REQUIRE(created.height == 2);
    REQUIRE(created.dataSize == 16);
    REQUIRE(created.pixels.size() == 16);

    REQUIRE(created.pixels[0] == 255);
    REQUIRE(created.pixels[1] == 0);
    REQUIRE(created.pixels[2] == 0);
    REQUIRE(created.pixels[3] == 255);
    REQUIRE(created.pixels[15] == 128);
}

TEST_CASE("TextureLoader reports missing and corrupt images", "[asset][loader]")
{
    Janus::Test::AssetTempDirectory temp;
    const auto corruptPath = temp.Path() / "corrupt.png";
    REQUIRE(Janus::FileSystem::WriteText(corruptPath, "not a png"));

    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);

    const auto missing = Janus::TextureLoader::Load(
        temp.Path() / "missing.png",
        *renderer);
    const auto corrupt = Janus::TextureLoader::Load(
        corruptPath,
        *renderer);

    REQUIRE_FALSE(missing);
    REQUIRE(missing.GetError().code == Janus::ErrorCode::FileNotFound);
    REQUIRE_FALSE(corrupt);
    REQUIRE(corrupt.GetError().code == Janus::ErrorCode::AssetDecodeFailed);
    REQUIRE(device.createdTextures.empty());
}

TEST_CASE("ShaderSourceLoader reads text assets", "[asset][loader]")
{
    Janus::Test::AssetTempDirectory temp;
    const auto shaderPath = temp.Path() / "sprite.glsl";
    REQUIRE(Janus::FileSystem::WriteText(shaderPath, "void main() {}\n"));

    auto source = Janus::ShaderSourceLoader::Load(shaderPath);
    REQUIRE(source);
    REQUIRE(source.Value() == "void main() {}\n");

    const auto missing = Janus::ShaderSourceLoader::Load(
        temp.Path() / "missing.glsl");
    REQUIRE_FALSE(missing);
    REQUIRE(missing.GetError().code == Janus::ErrorCode::FileNotFound);
}
