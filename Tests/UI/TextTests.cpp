#include "../Asset/AssetTestUtils.h"
#include "../Renderer/FakeRenderDevice.h"
#include "Asset/AssetRegistry.h"
#include "Asset/AssetService.h"
#include "Asset/Loader/FontLoader.h"
#include "Core/Command/CommandBus.h"
#include "Core/FileSystem/FileSystem.h"
#include "Renderer/Renderer2D.h"
#include "Runtime/RuntimeExecution.h"
#include "Runtime/RuntimeSession.h"
#include "Scene/Command/SceneCommands.h"
#include "Scene/Scene.h"
#include "Scene/SceneCloner.h"
#include "Scene/SceneDeserializer.h"
#include "Scene/SceneReflection.h"
#include "Scene/SceneRenderer.h"
#include "Scene/SceneSerializer.h"
#include "UI/TextLayout.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <limits>

using namespace Janus;

namespace
{
std::string FontJson()
{
    return R"({"version":1,"atlas":"11111111-1111-4111-8111-111111111111",
      "width":16,"height":16,"lineHeight":10,"baseline":8,"fallback":63,
      "glyphs":[{"codepoint":63,"x":0,"y":0,"width":5,"height":7,"offsetX":0,"offsetY":-7,"advance":6},
                {"codepoint":65,"x":8,"y":0,"width":5,"height":7,"offsetX":1,"offsetY":-7,"advance":6}]})";
}
} // namespace

TEST_CASE("Font metadata rejects malformed metrics and bounds", "[ui][text][asset]")
{
    REQUIRE(FontLoader::Parse(FontJson()));
    for (const auto& [from, to] : std::vector<std::pair<std::string, std::string>>{
             {"\"version\":1", "\"version\":2"},
             {"\"lineHeight\":10", "\"lineHeight\":0"},
             {"\"width\":16", "\"width\":1"},
             {"\"fallback\":63", "\"fallback\":66"},
             {"\"codepoint\":65", "\"codepoint\":63"},
             {"\"codepoint\":65", "\"codepoint\":55296"},
             {"\"advance\":6", "\"advance\":-1"},
             {"\"height\":16", "\"height\":1.5"}})
    {
        auto json = FontJson();
        json.replace(json.find(from), from.size(), to);
        INFO(json);
        CHECK_FALSE(FontLoader::Parse(json));
    }
    CHECK_FALSE(FontLoader::Parse("{}"));
    CHECK_FALSE(FontLoader::Parse("{"));
    CHECK_FALSE(FontLoader::Parse(std::string(1024 * 1024 + 1, ' ')));
}

TEST_CASE("Text validates UTF8 and bounded authoring values", "[ui][text]")
{
    CHECK(ValidateTextContent("A\n\r\n\xe4\xb8\xad\xf0\x9f\x98\x80"));
    for (auto invalid : {std::string("\xc0\xaf"), std::string("\xed\xa0\x80"),
                         std::string("\xf4\x90\x80\x80"), std::string("\xe4\xb8"),
                         std::string("\x80"), std::string("A\0B", 3), std::string(4097, 'A')})
        CHECK_FALSE(ValidateTextContent(invalid));
    TextComponent text;
    text.fontSize = std::numeric_limits<f32>::infinity();
    CHECK_FALSE(ValidateText(text));
    text.fontSize = 0;
    CHECK_FALSE(ValidateText(text));
}

TEST_CASE("Text measures newline fallback alignment and clips glyph UVs", "[ui][text]")
{
    auto font = FontLoader::Parse(FontJson());
    REQUIRE(font);
    TextComponent text;
    text.content = "A\r\n\xe4\xb8\xad";
    text.fontSize = 20;
    UILayoutItem item{{}, UIDrawKind::Text, {{0, 0}, {30, 40}}, {{4, 0}, {30, 40}}};
    auto layout = TextLayout::Build(text, font.Value(), item);
    REQUIRE(layout);
    REQUIRE(layout.Value().size() == 2);
    CHECK(layout.Value()[0].size.x == 8);
    CHECK(layout.Value()[0].position.x == 8);
    CHECK(layout.Value()[0].uv.min.x == Catch::Approx(9.0f / 16));
    CHECK(layout.Value()[1].position.y == 29);
    text.content = "AA\nA";
    text.alignment = "right";
    item.visible = item.rect;
    auto right = TextLayout::Build(text, font.Value(), item);
    REQUIRE(right);
    REQUIRE(right.Value().size() == 3);
    CHECK(right.Value()[0].position.x == 13);
    CHECK(right.Value()[2].position.x == 25);
}

TEST_CASE("Font cache validates atlas type size and restores after unload", "[ui][text][asset]")
{
    Test::AssetTempDirectory temp;
    REQUIRE(FileSystem::WriteText(temp.Path() / "font.json", FontJson()));
    AssetRegistry registry;
    auto font = registry.Register(AssetType::Font, "font.json");
    REQUIRE(font);
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    AssetService assets(temp.Path(), registry, *renderer);
    CHECK_FALSE(assets.LoadFont(font.Value()));
    CHECK_FALSE(assets.IsLoaded(font.Value()));
    auto atlas = UUID::Parse("11111111-1111-4111-8111-111111111111");
    REQUIRE(atlas);
    REQUIRE(registry.Register({AssetHandle{atlas.Value()}, AssetType::Texture, "atlas.png"}));
    std::filesystem::copy_file(Test::AssetFixturePath("test_rgba.png"), temp.Path() / "atlas.png");
    CHECK_FALSE(assets.LoadFont(font.Value())); // fixture is 2x2, metadata claims 16x16
    // Loading valid CPU metadata still requires an atlas of matching dimensions.
    CHECK_FALSE(assets.LoadFont(AssetHandle{atlas.Value()}));
}

TEST_CASE("Asset search is typed ordered paginated and bounded", "[ui][text][asset]")
{
    AssetRegistry registry;
    REQUIRE(registry.Register(AssetType::Font, "Fonts/Z.font.json"));
    REQUIRE(registry.Register(AssetType::Font, "Fonts/A.font.json"));
    REQUIRE(registry.Register(AssetType::Texture, "Fonts/atlas.png"));
    auto first = registry.Search("Fonts/", AssetType::Font, 0, 1);
    REQUIRE(first);
    CHECK(first.Value().total == 2);
    REQUIRE(first.Value().assets.size() == 1);
    CHECK(first.Value().assets[0].relativePath.generic_string() == "Fonts/A.font.json");
    CHECK(registry.Search("Fonts/", AssetType::Font, 1, 1)
              .Value()
              .assets[0]
              .relativePath.generic_string() == "Fonts/Z.font.json");
    CHECK(registry.Search("", {}, 100, 1).Value().assets.empty());
    CHECK_FALSE(registry.Search("", {}, 0, 0));
    CHECK_FALSE(registry.Search("", {}, 0, 101));
    CHECK_FALSE(registry.Search(std::string(257, 'x'), {}, 0, 10));
    REQUIRE(registry.Register(AssetType::Font, std::string(12000, 'a') + ".json"));
    CHECK_FALSE(registry.Search("", AssetType::Font, 2, 1));
}

TEST_CASE("Text reflection persists clones and rejects invalid edits atomically", "[ui][text]")
{
    auto registry = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(registry);
    SceneReflection reflection(registry.Value());
    Scene scene;
    const auto entity = scene.CreateEntity("Label");
    const auto id = scene.GetComponent<EntityIdentityComponent>(entity)->id;
    REQUIRE(reflection.AddComponent(scene, id, MakeComponentTypeId("Text")));
    REQUIRE(reflection.ApplyPropertyMutation(scene, id, MakeComponentTypeId("Text"),
                                             MakePropertyId("Text.content"), std::string("HP 12")));
    CHECK_FALSE(reflection.ApplyPropertyMutation(scene, id, MakeComponentTypeId("Text"),
                                                 MakePropertyId("Text.fontSize"), f32{-1}));
    CHECK(scene.GetComponent<TextComponent>(entity)->fontSize == 24);
    auto saved = SceneSerializer::Serialize(scene, registry.Value());
    REQUIRE(saved);
    auto loaded = SceneDeserializer::Deserialize(saved.Value(), registry.Value());
    REQUIRE(loaded);
    auto clone = SceneCloner::Clone(*loaded.Value(), registry.Value());
    REQUIRE(clone);
    CHECK(clone.Value()->GetComponent<TextComponent>(clone.Value()->FindEntity(id))->content ==
          "HP 12");
}

TEST_CASE("Text showcase renders ordered glyphs and font cache releases atlas exactly once",
          "[ui][text][render]")
{
    Test::FakeRenderDevice device;
    const auto project =
        std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path() / "SandboxProject";
    auto reflection = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(reflection);
    auto registry = AssetRegistry::Load(project / "Config/AssetRegistry.json");
    REQUIRE(registry);
    auto fontHandle = registry.Value().FindByPath("Fonts/JanusPixel.font.json")->handle;
    {
        auto renderer = Detail::Renderer2DTestAccess::Create(device);
        AssetService assets(project, registry.Value(), *renderer);
        auto font = assets.LoadFont(fontHandle);
        REQUIRE(font);
        const auto* first = font.Value();
        CHECK(assets.LoadFont(fontHandle).Value() == first);
        auto atlas = first->atlas;
        REQUIRE(assets.LoadTexture(atlas));
        REQUIRE(assets.Unload(atlas));
        CHECK_FALSE(assets.IsLoaded(fontHandle));
        REQUIRE(assets.LoadFont(fontHandle));
        auto scene =
            SceneDeserializer::Load(project / "Scenes/TextShowcase.scene", reflection.Value());
        REQUIRE(scene);
        SceneRenderer sceneRenderer;
        REQUIRE(sceneRenderer.Render(*scene.Value(), assets, *renderer, {640, 360}, {1280, 720}));
        CHECK(renderer->GetStatistics().spriteCount > 200);
        REQUIRE(device.drawCommands.size() >= 6);
        const auto* health = scene.Value()->GetComponent<TextComponent>(
            scene.Value()->FindEntity(UUID::Parse("fa200000-0000-4000-8000-000000000006").Value()));
        REQUIRE(health);
        CHECK(health->content == "HP 12 / 12\nAUTHORING VALUE");
        REQUIRE(assets.Unload(fontHandle));
        CHECK(assets.IsLoaded(atlas)); // Font does not own the shared texture.
        assets.Clear();
        CHECK_FALSE(assets.IsLoaded(atlas));
    }
    CHECK(device.createdTextures.size() == device.destroyedTextures.size());
}

TEST_CASE("Runtime text uses shared execution and Stop preserves serialized authoring value",
          "[ui][text][runtime]")
{
    const auto project =
        std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path() / "SandboxProject";
    auto reflection = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(reflection);
    auto registry = AssetRegistry::Load(project / "Config/AssetRegistry.json");
    REQUIRE(registry);
    auto authoring =
        SceneDeserializer::Load(project / "Scenes/TextShowcase.scene", reflection.Value());
    REQUIRE(authoring);
    auto independent = SceneCloner::Clone(*authoring.Value(), reflection.Value());
    REQUIRE(independent);
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    AssetService assets(project, registry.Value(), *renderer);
    InputState input;
    auto execution = RuntimeExecution::Create(*independent.Value(), assets, input);
    REQUIRE(execution);
    REQUIRE(execution.Value()->Start());
    auto runtime =
        RuntimeSession::Start(*authoring.Value(), reflection.Value(), assets, input, true);
    REQUIRE(runtime);
    const auto id = UUID::Parse("fa200000-0000-4000-8000-000000000006").Value();
    auto value = [&](const Scene& scene)
    { return scene.GetComponent<TextComponent>(scene.FindEntity(id))->content; };
    for (int frame = 0; frame < 125; ++frame)
    {
        REQUIRE(execution.Value()->Advance(TimeStep::FromSeconds(1.0 / 60), input,
                                           ScriptReloadPolicy::Skip));
        REQUIRE(runtime.Value()->Step());
        REQUIRE(value(*independent.Value()) == value(runtime.Value()->GetScene()));
    }
    CHECK(value(runtime.Value()->GetScene()) == "HP 4 / 12\nRUNTIME VALUE");
    REQUIRE(runtime.Value()->Stop());
    REQUIRE(execution.Value()->Stop());
    CHECK(value(*authoring.Value()) == "HP 12 / 12\nAUTHORING VALUE");
    SceneReflection reflected(reflection.Value());
    CommandBus commands;
    REQUIRE(commands.Execute(std::make_unique<SetPropertyCommand>(
        *authoring.Value(), reflected, id, MakeComponentTypeId("Text"),
        MakePropertyId("Text.content"), std::string("HP 9"))));
    REQUIRE(commands.Undo());
    CHECK(value(*authoring.Value()) == "HP 12 / 12\nAUTHORING VALUE");
    REQUIRE(commands.Redo());
    auto saved = SceneSerializer::Serialize(*authoring.Value(), reflection.Value());
    REQUIRE(saved);
    auto reopened = SceneDeserializer::Deserialize(saved.Value(), reflection.Value());
    REQUIRE(reopened);
    CHECK(value(*reopened.Value()) == "HP 9");
}

TEST_CASE("Text render failure occurs before drawing and disabled text does not load missing fonts",
          "[ui][text][render]")
{
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    AssetRegistry registry;
    AssetService assets(".", registry, *renderer);
    Scene scene;
    auto canvas = scene.CreateEntity();
    scene.AddComponent<CanvasComponent>(canvas, {});
    TextComponent text;
    text.content = "HP 12";
    scene.AddComponent<TextComponent>(canvas, text);
    SceneRenderer sceneRenderer;
    CHECK_FALSE(sceneRenderer.Render(scene, assets, *renderer, {800, 600}));
    CHECK(device.drawCommands.empty());
    scene.GetComponent<TextComponent>(canvas)->enabled = false;
    REQUIRE(sceneRenderer.Render(scene, assets, *renderer, {800, 600}));
    CHECK(device.createdTextures.empty());
}

TEST_CASE("Font cache preserves unique asset categories and invalidates atlas dependents",
          "[ui][text][asset]")
{
    AssetCache cache;
    auto font = FontLoader::Parse(FontJson());
    REQUIRE(font);
    const auto handle = AssetHandle::Random();
    const auto atlas = font.Value().atlas;
    CHECK_FALSE(cache.StoreFont({}, font.Value()));
    REQUIRE(cache.StoreFont(handle, font.Value()));
    CHECK_FALSE(cache.StoreTexture(handle, TextureHandle{1}));
    CHECK_FALSE(cache.StoreLuaScriptSource(handle, "return {}"));
    CHECK(cache.Contains(handle));
    REQUIRE(cache.FindFont(handle));
    CHECK(cache.FindFont(handle)->atlas == atlas);
    CHECK(cache.RemoveFontsForAtlas(atlas) == 1);
    CHECK_FALSE(cache.Contains(handle));
    REQUIRE(cache.StoreTexture(handle, TextureHandle{1}));
    CHECK_FALSE(cache.StoreFont(handle, font.Value()));
    cache.Clear();
    CHECK_FALSE(cache.FindFont(handle));
}
