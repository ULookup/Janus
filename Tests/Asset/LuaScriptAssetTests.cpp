#include "Asset/AssetCache.h"
#include "Asset/AssetRegistry.h"
#include "Asset/AssetService.h"
#include "Asset/Loader/LuaScriptSourceLoader.h"
#include "Core/FileSystem/FileSystem.h"
#include "Renderer/Renderer2D.h"

#include "AssetTestUtils.h"
#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

namespace
{

void PrepareScriptsDirectory(const std::filesystem::path& projectRoot)
{
    std::error_code error;
    std::filesystem::create_directories(projectRoot / "Scripts", error);
    REQUIRE_FALSE(error);
}

} // namespace

TEST_CASE("LuaScriptSourceLoader reads project text assets",
          "[asset][lua-script][v0.5]")
{
    Janus::Test::AssetTempDirectory temp;
    PrepareScriptsDirectory(temp.Path());
    const auto scriptPath = temp.Path() / "Scripts/Test.lua";
    REQUIRE(Janus::FileSystem::WriteText(
        scriptPath,
        "return { value = 42 }\n"));

    const auto loaded = Janus::LuaScriptSourceLoader::Load(scriptPath);
    REQUIRE(loaded);
    REQUIRE(loaded.Value() == "return { value = 42 }\n");

    const auto missing = Janus::LuaScriptSourceLoader::Load(
        temp.Path() / "Scripts/Missing.lua");
    REQUIRE_FALSE(missing);
    REQUIRE(missing.GetError().code == Janus::ErrorCode::FileNotFound);
}

TEST_CASE("AssetCache stores Lua source as its own runtime category",
          "[asset][cache][lua-script][v0.5]")
{
    Janus::AssetCache cache;
    const Janus::AssetHandle script = Janus::AssetHandle::Random();

    REQUIRE(cache.StoreLuaScriptSource(script, "return {}"));
    REQUIRE(cache.Contains(script));
    REQUIRE(cache.LuaScriptSourceCount() == 1);
    REQUIRE(cache.FindLuaScriptSource(script) != nullptr);
    REQUIRE(*cache.FindLuaScriptSource(script) == "return {}");

    REQUIRE_FALSE(cache.StoreShaderSource(script, "wrong category"));
    REQUIRE_FALSE(cache.StoreTexture(script, Janus::TextureHandle{99}));

    REQUIRE(cache.RemoveLuaScriptSource(script));
    REQUIRE_FALSE(cache.Contains(script));
    REQUIRE(cache.LuaScriptSourceCount() == 0);
    REQUIRE_FALSE(cache.StoreLuaScriptSource(
        Janus::AssetHandle{},
        "return {}"));
}

TEST_CASE("AssetService validates and caches registered Lua scripts",
          "[asset][service][lua-script][v0.5]")
{
    Janus::Test::AssetTempDirectory temp;
    PrepareScriptsDirectory(temp.Path());
    const auto scriptPath = temp.Path() / "Scripts/Player.lua";
    REQUIRE(Janus::FileSystem::WriteText(scriptPath, "return { version = 1 }"));

    Janus::AssetRegistry registry;
    const auto script = registry.Register(
        Janus::AssetType::LuaScript,
        "Scripts/Player.lua");
    const auto texture = registry.Register(
        Janus::AssetType::Texture,
        "Assets/player.png");
    REQUIRE(script);
    REQUIRE(texture);

    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::AssetService service(temp.Path(), registry, *renderer);

    const auto missing = service.LoadLuaScriptSource(
        Janus::AssetHandle::Random());
    const auto wrongType = service.LoadLuaScriptSource(texture.Value());
    REQUIRE_FALSE(missing);
    REQUIRE(missing.GetError().code == Janus::ErrorCode::AssetNotFound);
    REQUIRE_FALSE(wrongType);
    REQUIRE(wrongType.GetError().code == Janus::ErrorCode::AssetTypeMismatch);

    const auto first = service.LoadLuaScriptSource(script.Value());
    REQUIRE(first);
    REQUIRE(first.Value() == "return { version = 1 }");
    REQUIRE(service.IsLoaded(script.Value()));

    REQUIRE(Janus::FileSystem::WriteText(scriptPath, "return { version = 2 }"));
    const auto cached = service.LoadLuaScriptSource(script.Value());
    REQUIRE(cached);
    REQUIRE(cached.Value() == "return { version = 1 }");

    REQUIRE(service.Unload(script.Value()));
    REQUIRE_FALSE(service.IsLoaded(script.Value()));

    const auto reloaded = service.LoadLuaScriptSource(script.Value());
    REQUIRE(reloaded);
    REQUIRE(reloaded.Value() == "return { version = 2 }");
}

TEST_CASE("AssetService reports missing registered Lua source",
          "[asset][service][lua-script][v0.5]")
{
    Janus::Test::AssetTempDirectory temp;
    Janus::AssetRegistry registry;
    const auto script = registry.Register(
        Janus::AssetType::LuaScript,
        "Scripts/Missing.lua");
    REQUIRE(script);

    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::AssetService service(temp.Path(), registry, *renderer);

    const auto result = service.LoadLuaScriptSource(script.Value());
    REQUIRE_FALSE(result);
    REQUIRE(result.GetError().code == Janus::ErrorCode::FileNotFound);
    REQUIRE_FALSE(service.IsLoaded(script.Value()));
}
