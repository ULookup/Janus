#include "Asset/AssetHandle.h"
#include "Asset/AssetMetadata.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("AssetHandle is a persistent UUID-backed identity", "[asset][types]")
{
    const Janus::AssetHandle handle = Janus::AssetHandle::Random();
    REQUIRE(handle.IsValid());

    auto parsed = Janus::AssetHandle::Parse(handle.ToString());
    REQUIRE(parsed);
    REQUIRE(parsed.Value() == handle);
    REQUIRE(Janus::AssetHandleHash{}(parsed.Value()) == Janus::AssetHandleHash{}(handle));
}

TEST_CASE("AssetHandle rejects nil UUID", "[asset][types]")
{
    const auto parsed =
        Janus::AssetHandle::Parse("00000000-0000-0000-0000-000000000000");

    REQUIRE_FALSE(parsed);
    REQUIRE(parsed.GetError().code == Janus::ErrorCode::InvalidArgument);
}

TEST_CASE("AssetType names round trip", "[asset][types]")
{
    REQUIRE(Janus::AssetTypeName(Janus::AssetType::Texture) == "texture");
    REQUIRE(Janus::AssetTypeName(Janus::AssetType::ShaderSource) == "shader_source");
    REQUIRE(Janus::AssetTypeName(Janus::AssetType::LuaScript) == "lua-script");

    const auto texture = Janus::ParseAssetType("texture");
    const auto shader = Janus::ParseAssetType("shader_source");
    const auto luaScript = Janus::ParseAssetType("lua-script");
    const auto invalid = Janus::ParseAssetType("audio");

    REQUIRE(texture);
    REQUIRE(shader);
    REQUIRE(luaScript);
    REQUIRE(texture.Value() == Janus::AssetType::Texture);
    REQUIRE(shader.Value() == Janus::AssetType::ShaderSource);
    REQUIRE(luaScript.Value() == Janus::AssetType::LuaScript);
    REQUIRE_FALSE(invalid);
}
