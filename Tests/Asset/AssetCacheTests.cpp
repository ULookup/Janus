#include "Asset/AssetCache.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("AssetCache stores one runtime category per handle", "[asset][cache]")
{
    Janus::AssetCache cache;
    const Janus::AssetHandle textureAsset = Janus::AssetHandle::Random();
    const Janus::AssetHandle shaderAsset = Janus::AssetHandle::Random();

    REQUIRE(cache.StoreTexture(textureAsset, Janus::TextureHandle{42}));
    REQUIRE(cache.StoreShaderSource(shaderAsset, "shader source"));

    REQUIRE(cache.Contains(textureAsset));
    REQUIRE(cache.Contains(shaderAsset));
    REQUIRE(cache.TextureCount() == 1);
    REQUIRE(cache.ShaderSourceCount() == 1);
    REQUIRE(cache.FindTexture(textureAsset) != nullptr);
    REQUIRE(cache.FindTexture(textureAsset)->value == 42);
    REQUIRE(cache.FindShaderSource(shaderAsset) != nullptr);
    REQUIRE(*cache.FindShaderSource(shaderAsset) == "shader source");

    REQUIRE_FALSE(cache.StoreShaderSource(textureAsset, "wrong category"));
    REQUIRE_FALSE(cache.StoreTexture(shaderAsset, Janus::TextureHandle{77}));
}

TEST_CASE("AssetCache removes and clears runtime entries", "[asset][cache]")
{
    Janus::AssetCache cache;
    const Janus::AssetHandle first = Janus::AssetHandle::Random();
    const Janus::AssetHandle second = Janus::AssetHandle::Random();

    REQUIRE(cache.StoreTexture(first, Janus::TextureHandle{11}));
    REQUIRE(cache.StoreShaderSource(second, "source"));

    auto removed = cache.RemoveTexture(first);
    REQUIRE(removed.has_value());
    REQUIRE(removed->value == 11);
    REQUIRE_FALSE(cache.Contains(first));
    REQUIRE(cache.RemoveShaderSource(second));
    REQUIRE_FALSE(cache.Contains(second));

    REQUIRE(cache.StoreTexture(first, Janus::TextureHandle{12}));
    REQUIRE(cache.StoreShaderSource(second, "source 2"));
    cache.Clear();

    REQUIRE(cache.TextureCount() == 0);
    REQUIRE(cache.ShaderSourceCount() == 0);
}

TEST_CASE("AssetCache rejects invalid runtime entries", "[asset][cache]")
{
    Janus::AssetCache cache;

    REQUIRE_FALSE(cache.StoreTexture(
        Janus::AssetHandle{},
        Janus::TextureHandle{1}));
    REQUIRE_FALSE(cache.StoreTexture(
        Janus::AssetHandle::Random(),
        Janus::TextureHandle{}));
    REQUIRE_FALSE(cache.StoreShaderSource(
        Janus::AssetHandle{},
        "source"));
}
