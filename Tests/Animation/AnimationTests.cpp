#include "../Asset/AssetTestUtils.h"
#include "../Renderer/FakeRenderDevice.h"
#include "Animation/AnimationSystem.h"
#include "Animation/AnimatorComponent.h"
#include "Asset/AssetRegistry.h"
#include "Asset/AssetService.h"
#include "Asset/Loader/AnimationClipLoader.h"
#include "Core/Command/CommandBus.h"
#include "Core/FileSystem/FileSystem.h"
#include "Renderer/Renderer2D.h"
#include "Runtime/RuntimeExecution.h"
#include "Runtime/RuntimeSession.h"
#include "Scene/Command/SceneCommands.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneCloner.h"
#include "Scene/SceneDeserializer.h"
#include "Scene/SceneReflection.h"
#include "Scene/SceneRenderer.h"
#include "Scene/SceneSerializer.h"
#include "UI/UIComponents.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <limits>
#include <nlohmann/json.hpp>

namespace
{
using namespace Janus;
constexpr const char* TextureId = "ad100000-0000-4000-8000-000000000001";
const std::string ClipSource = R"({"version":1,"texture":"ad100000-0000-4000-8000-000000000001",
 "loop":true,"frames":[{"uvMin":[0,0],"uvMax":[0.5,1],"duration":0.125},
 {"uvMin":[0.5,0],"uvMax":[1,1],"duration":0.375}]})";
struct Fixture
{
    Test::AssetTempDirectory temp;
    AssetRegistry registry;
    Test::FakeRenderDevice device;
    std::unique_ptr<Renderer2D> renderer = Detail::Renderer2DTestAccess::Create(device);
    AssetService assets{temp.Path(), registry, *renderer};
    Scene scene;
    ECS::Entity entity = scene.CreateEntity("Animated");
    UUID id = scene.GetComponent<EntityIdentityComponent>(entity)->id;
    AssetHandle clip;
    AssetHandle texture = AssetHandle::Parse(TextureId).Value();

    Fixture()
    {
        REQUIRE(registry.Register(AssetMetadata{texture, AssetType::Texture, "atlas.png"}));
        REQUIRE(FileSystem::WriteText(temp.Path() / "clip.json", ClipSource));
        auto registered = registry.Register(AssetType::AnimationClip, "clip.json");
        REQUIRE(registered);
        clip = registered.Value();
        REQUIRE(scene.AddComponent<SpriteRendererComponent>(entity, SpriteRendererComponent{}));
        REQUIRE(scene.AddComponent<AnimatorComponent>(
            entity, AnimatorComponent{AssetReferenceValue{clip.id}, true, true, 1}));
    }
};
} // namespace

TEST_CASE("Animation Clip validates frame bounds and malformed documents", "[animation]")
{
    using Json = nlohmann::json;
    REQUIRE(AnimationClipLoader::Parse(ClipSource));
    for (const auto source : {"{}", "[]", "invalid"})
        REQUIRE_FALSE(AnimationClipLoader::Parse(source));
    for (const auto duration : {0.0, -1.0, 60.1})
    {
        auto json = Json::parse(ClipSource);
        json["frames"][0]["duration"] = duration;
        REQUIRE_FALSE(AnimationClipLoader::Parse(json.dump()));
    }
    for (int kind = 0; kind < 7; ++kind)
    {
        auto json = Json::parse(ClipSource);
        if (kind == 0)
            json["frames"] = Json::array();
        if (kind == 1)
            json["version"] = 2;
        if (kind == 2)
            json["loop"] = 1;
        if (kind == 3)
            json["frames"][0]["uvMin"] = {0.5, 0};
        if (kind == 4)
            json["frames"][0]["uvMax"] = {2, 1};
        if (kind == 5)
            json["texture"] = "bad";
        if (kind == 6)
            json["frames"] = std::vector<Json>(1025, json["frames"][0]);
        REQUIRE_FALSE(AnimationClipLoader::Parse(json.dump()));
    }
}

TEST_CASE("Animation runtime loops skips large time and keeps authoring unchanged", "[animation]")
{
    Fixture f;
    AnimationSystem animation(f.scene, f.assets);
    REQUIRE(animation.Start());
    REQUIRE(animation.GetPose(f.id)->frameIndex == 0);
    REQUIRE(animation.Advance(TimeStep::FromSeconds(0.125)));
    REQUIRE(animation.GetPose(f.id)->frameIndex == 1);
    REQUIRE(animation.Advance(TimeStep::FromSeconds(1000000.375)));
    REQUIRE(animation.GetPose(f.id)->frameIndex == 0);
    REQUIRE_FALSE(f.scene.GetComponent<SpriteRendererComponent>(f.entity)->texture.IsValid());
    REQUIRE_FALSE(
        animation.Advance(TimeStep::FromSeconds(std::numeric_limits<double>::infinity())));
    REQUIRE(animation.GetPose(f.id)->frameIndex == 0);
    animation.Stop();
    REQUIRE_FALSE(animation.GetPose(f.id));
}

TEST_CASE("Animation switch validates before replacing playback and one-shot holds final pose",
          "[animation]")
{
    Fixture f;
    AnimationSystem animation(f.scene, f.assets);
    REQUIRE(animation.Start());
    REQUIRE(animation.Advance(TimeStep::FromSeconds(0.2)));
    REQUIRE_FALSE(animation.Play(f.id, f.texture));
    REQUIRE(animation.GetPose(f.id)->frameIndex == 1);
    auto json = nlohmann::json::parse(ClipSource);
    json["loop"] = false;
    REQUIRE(FileSystem::WriteText(f.temp.Path() / "once.json", json.dump()));
    auto once = f.registry.Register(AssetType::AnimationClip, "once.json");
    REQUIRE(once);
    REQUIRE(animation.Play(f.id, once.Value()));
    REQUIRE(animation.GetPose(f.id)->frameIndex == 0);
    REQUIRE(animation.Advance(TimeStep::FromSeconds(1))); // First visible frame is not skipped.
    REQUIRE(animation.GetPose(f.id)->frameIndex == 0);
    REQUIRE(animation.Advance(TimeStep::FromSeconds(1)));
    REQUIRE(animation.GetPose(f.id)->frameIndex == 1);
    REQUIRE_FALSE(animation.GetPose(f.id)->playing);
    REQUIRE(animation.Stop(f.id));
    REQUIRE_FALSE(animation.GetPose(f.id));
    REQUIRE(animation.Advance(TimeStep::FromSeconds(1)));
    REQUIRE_FALSE(animation.GetPose(f.id));
}

TEST_CASE("Animation releases removed disabled and destroyed targets and caches immutable playback",
          "[animation]")
{
    Fixture f;
    AnimationSystem animation(f.scene, f.assets);
    REQUIRE(animation.Start());
    REQUIRE(f.assets.Unload(f.texture)); // Dependent clip is invalidated, playback owns a copy.
    REQUIRE(FileSystem::WriteText(f.temp.Path() / "clip.json", "invalid"));
    REQUIRE(animation.Advance(TimeStep::FromSeconds(0.2)));
    REQUIRE(animation.GetPose(f.id)->frameIndex == 1);
    REQUIRE_FALSE(animation.Play(f.id));
    f.scene.GetComponent<AnimatorComponent>(f.entity)->enabled = false;
    REQUIRE(animation.Advance(TimeStep{}));
    REQUIRE_FALSE(animation.GetPose(f.id));
    REQUIRE(FileSystem::WriteText(f.temp.Path() / "clip.json", ClipSource));
    f.scene.GetComponent<AnimatorComponent>(f.entity)->enabled = true;
    REQUIRE(animation.Advance(TimeStep{}));
    REQUIRE(animation.GetPose(f.id));
    REQUIRE(f.scene.RemoveComponent<SpriteRendererComponent>(f.entity));
    REQUIRE_FALSE(animation.Advance(TimeStep{}));
    REQUIRE_FALSE(animation.GetPose(f.id));
    f.scene.DestroyEntity(f.entity);
    REQUIRE(animation.Advance(TimeStep{}));
    REQUIRE_FALSE(animation.GetPose(f.id));
}

TEST_CASE("Animator uses shared reflection persistence cloning and undo", "[animation][reflection]")
{
    Fixture f;
    auto registry = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(registry);
    SceneReflection reflection(registry.Value());
    const auto type = MakeComponentTypeId("Animator");
    REQUIRE(registry.Value().FindComponent(type)->FindProperty("clip")->referenceConstraint ==
            "animation-clip");
    auto saved = SceneSerializer::Serialize(f.scene, registry.Value());
    REQUIRE(saved);
    REQUIRE(saved.Value().find("frameIndex") == std::string::npos);
    auto loaded = SceneDeserializer::Deserialize(saved.Value(), registry.Value());
    REQUIRE(loaded);
    auto cloned = SceneCloner::Clone(*loaded.Value(), registry.Value());
    REQUIRE(cloned);
    const auto* animator =
        cloned.Value()->GetComponent<AnimatorComponent>(cloned.Value()->FindEntity(f.id));
    REQUIRE(animator);
    CHECK(animator->clip.id == f.clip.id);
    CHECK(animator->enabled);
    CommandBus commands;
    REQUIRE(commands.Execute(std::make_unique<SetPropertyCommand>(
        f.scene, reflection, f.id, type, MakePropertyId("Animator.speed"), f32{2})));
    CHECK(f.scene.GetComponent<AnimatorComponent>(f.entity)->speed == 2);
    REQUIRE(commands.Undo());
    CHECK(f.scene.GetComponent<AnimatorComponent>(f.entity)->speed == 1);
    REQUIRE(commands.Redo());
    CHECK(f.scene.GetComponent<AnimatorComponent>(f.entity)->speed == 2);
    CHECK_FALSE(commands.Execute(std::make_unique<SetPropertyCommand>(
        f.scene, reflection, f.id, type, MakePropertyId("Animator.speed"), f32{-1})));
    REQUIRE(commands.Execute(
        std::make_unique<RemoveComponentCommand>(f.scene, reflection, f.id, type)));
    REQUIRE(commands.Undo());
    CHECK(f.scene.GetComponent<AnimatorComponent>(f.entity)->clip.id == f.clip.id);
}

TEST_CASE("Animation Lua controls share Runtime stepping and preserve paused playback assets",
          "[animation][runtime]")
{
    Fixture f;
    f.scene.GetComponent<AnimatorComponent>(f.entity)->playOnStart = false;
    REQUIRE(FileSystem::WriteText(f.temp.Path() / "control.lua", R"lua(
return {
 OnCreate = function(self)
   assert(not self.entity:animation_state())
   assert(not pcall(function() self.entity:play_animation('bad') end))
   self.entity:play_animation()
   local playing, frame, elapsed = self.entity:animation_state()
   assert(playing and frame == 0 and elapsed == 0)
   self.tick = 0
 end,
 OnUpdate = function(self)
   self.tick = self.tick + 1
   if self.tick == 12 then self.entity:stop_animation() end
 end
})lua"));
    auto script = f.registry.Register(AssetType::LuaScript, "control.lua");
    REQUIRE(script);
    REQUIRE(f.scene.AddComponent<LuaScriptComponent>(f.entity, {script.Value(), true}));
    auto reflection = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(reflection);
    auto clone = SceneCloner::Clone(f.scene, reflection.Value());
    REQUIRE(clone);
    InputState input;
    auto execution = RuntimeExecution::Create(*clone.Value(), f.assets, input);
    REQUIRE(execution);
    REQUIRE(execution.Value()->Start());
    auto session = RuntimeSession::Start(f.scene, reflection.Value(), f.assets, input, true);
    REQUIRE(session);
    REQUIRE(FileSystem::WriteText(f.temp.Path() / "clip.json", "invalid"));
    REQUIRE(f.assets.Unload(f.clip));
    for (int frame = 0; frame < 12; ++frame)
    {
        REQUIRE(session.Value()->Step());
        REQUIRE(execution.Value()->Advance(TimeStep::FromSeconds(1.0 / 60), input,
                                           ScriptReloadPolicy::Skip));
        const auto* editor = session.Value()->GetAnimations().GetPose(f.id);
        const auto* standalone = execution.Value()->GetAnimations().GetPose(f.id);
        REQUIRE((editor != nullptr) == (standalone != nullptr));
        if (editor)
        {
            CHECK(editor->frameIndex == standalone->frameIndex);
            CHECK(editor->elapsedSeconds == standalone->elapsedSeconds);
        }
    }
    REQUIRE_FALSE(session.Value()->GetAnimations().GetPose(f.id));
    REQUIRE(session.Value()->Stop());
    REQUIRE(execution.Value()->Stop());
    CHECK_FALSE(execution.Value()->Start()); // A new play session reads changed disk assets.
    CHECK_FALSE(f.scene.GetComponent<SpriteRendererComponent>(f.entity)->texture.IsValid());
}

TEST_CASE("Animation renders sprite and clipped Image poses without editing base properties",
          "[animation][render]")
{
    Fixture f;
    std::filesystem::copy_file(std::filesystem::path(JANUS_TEST_SOURCE_DIR) /
                                   "Fixtures/Assets/test_rgba.png",
                               f.temp.Path() / "atlas.png");
    auto camera = f.scene.CreateEntity("Camera");
    REQUIRE(f.scene.AddComponent<CameraComponent>(camera, {1, true}));
    AnimationSystem animation(f.scene, f.assets);
    REQUIRE(animation.Start());
    REQUIRE(animation.Advance(TimeStep::FromSeconds(0.125)));
    SceneRenderer renderer;
    REQUIRE(renderer.Render(f.scene, f.assets, *f.renderer, {800, 600}, {800, 600}, nullptr,
                            &animation));
    REQUIRE(f.device.vertexUploads.size() == 1);
    f32 vertices[32];
    std::memcpy(vertices, f.device.vertexUploads.back().data(), sizeof(vertices));
    CHECK(vertices[2] == Catch::Approx(0.5f));
    REQUIRE_FALSE(f.scene.GetComponent<SpriteRendererComponent>(f.entity)->texture.IsValid());
    REQUIRE(f.scene.RemoveComponent<SpriteRendererComponent>(f.entity));
    auto canvas = f.scene.CreateEntity("Canvas");
    REQUIRE(f.scene.AddComponent<CanvasComponent>(canvas, {}));
    REQUIRE(f.scene.SetParent(f.entity, canvas));
    UIRectComponent rect;
    rect.offset = {-25, 0};
    rect.size = {100, 80};
    REQUIRE(f.scene.AddComponent<UIRectComponent>(f.entity, rect));
    REQUIRE(f.scene.AddComponent<ImageComponent>(f.entity, {}));
    REQUIRE(renderer.Render(f.scene, f.assets, *f.renderer, {800, 600}, {800, 600}, nullptr,
                            &animation));
    std::memcpy(vertices, f.device.vertexUploads.back().data(), sizeof(vertices));
    CHECK(vertices[0] == 0);
    CHECK(vertices[2] == Catch::Approx(0.625f));
    CHECK_FALSE(f.scene.GetComponent<ImageComponent>(f.entity)->texture.id.IsValid());
}

TEST_CASE("Animation clips enforce resource limits and enabled target constraints", "[animation]")
{
    Fixture f;
    auto json = nlohmann::json::parse(ClipSource);
    json["frames"][0]["duration"] = 60;
    json["frames"] = std::vector<nlohmann::json>(61, json["frames"][0]);
    CHECK_FALSE(AnimationClipLoader::Parse(json.dump()));
    const std::string oversized(1024 * 1024 + 1, ' ');
    CHECK_FALSE(AnimationClipLoader::Parse(oversized));
    REQUIRE(FileSystem::WriteText(f.temp.Path() / "oversized.json", oversized));
    CHECK_FALSE(AnimationClipLoader::Load(f.temp.Path() / "oversized.json"));
    REQUIRE(f.scene.AddComponent<ImageComponent>(f.entity, {}));
    AnimationSystem animation(f.scene, f.assets);
    CHECK_FALSE(animation.Start());
    REQUIRE(f.scene.RemoveComponent<ImageComponent>(f.entity));
    REQUIRE(animation.Start());
    f.scene.GetComponent<AnimatorComponent>(f.entity)->speed = 0;
    REQUIRE(animation.Advance(TimeStep::FromSeconds(100)));
    CHECK(animation.GetPose(f.id)->frameIndex == 0);
    f.scene.GetComponent<AnimatorComponent>(f.entity)->speed = 2;
    REQUIRE(animation.Advance(TimeStep::FromSeconds(0.0625)));
    CHECK(animation.GetPose(f.id)->frameIndex == 1);
    f.scene.GetComponent<AnimatorComponent>(f.entity)->speed =
        std::numeric_limits<f32>::quiet_NaN();
    CHECK_FALSE(animation.Advance(TimeStep{}));
}
