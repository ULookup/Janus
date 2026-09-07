#include "../Asset/AssetTestUtils.h"
#include "../Renderer/FakeRenderDevice.h"
#include "Asset/AssetRegistry.h"
#include "Asset/AssetService.h"
#include "Core/FileSystem/FileSystem.h"
#include "Renderer/Renderer2D.h"
#include "Runtime/RuntimeExecution.h"
#include "Runtime/RuntimeSession.h"
#include "Scene/Command/EntityCommands.h"
#include "Scene/Scene.h"
#include "Scene/SceneCloner.h"
#include "Scene/SceneDeserializer.h"
#include "Scene/SceneReflection.h"
#include "Scene/SceneRenderer.h"
#include "Scene/SceneSerializer.h"
#include "Scripting/ScriptEngine.h"
#include "UI/UIInteraction.h"
#include <catch2/catch_test_macros.hpp>
#include <cstring>
using namespace Janus;
namespace
{
struct Menu
{
    Scene scene;
    ECS::Entity canvas, first, second;
    usize childCount = 0;
    Menu()
    {
        canvas = scene.CreateEntity("Canvas");
        scene.AddComponent<CanvasComponent>(canvas, {});
        first = Add({0, 0});
        second = Add({120, 0});
    }
    ECS::Entity Add(Vector2 offset)
    {
        auto e = scene.CreateEntity("Button");
        ReparentEntityCommand attach(scene, Id(e), Id(canvas), childCount++);
        REQUIRE(attach.Execute());
        UIRectComponent rect;
        rect.offset = offset;
        rect.size = {100, 60};
        scene.AddComponent<UIRectComponent>(e, rect);
        scene.AddComponent<ButtonComponent>(e, {});
        return e;
    }
    UUID Id(ECS::Entity e)
    {
        return scene.GetComponent<EntityIdentityComponent>(e)->id;
    }
    UIInputResult Frame(UIInteraction& ui, const InputState& input)
    {
        auto layout = UILayout::Build(scene, {800, 600});
        REQUIRE(layout);
        return ui.Process(scene, layout.Value(), input);
    }
};
void Tap(InputState& input, Vector2 p)
{
    input.BeginFrame();
    input.Apply(PointerMovedEvent{p});
    input.Apply(PointerButtonPressedEvent{PointerButton::Left});
    input.Apply(PointerButtonReleasedEvent{PointerButton::Left});
}
} // namespace
TEST_CASE("Button short taps choose topmost and suppress gameplay", "[ui][button]")
{
    Menu m;
    auto top = m.Add({0, 0});
    UIInteraction ui;
    InputState input;
    Tap(input, {20, 20});
    auto frame = m.Frame(ui, input);
    REQUIRE(frame.clicks.size() == 1);
    CHECK(frame.clicks[0] == m.Id(top));
    CHECK_FALSE(frame.gameplay.WasPointerButtonPressed(PointerButton::Left));
    CHECK_FALSE(frame.gameplay.WasPointerButtonReleased(PointerButton::Left));
    m.scene.GetComponent<ButtonComponent>(top)->interactable = false;
    Tap(input, {20, 20});
    CHECK(m.Frame(ui, input).clicks.empty());
}
TEST_CASE("Button capture cancels drag out even when pointer returns within same frame",
          "[ui][button]")
{
    Menu m;
    UIInteraction ui;
    InputState input;
    input.Apply(PointerMovedEvent{{20, 20}});
    input.Apply(PointerButtonPressedEvent{PointerButton::Left});
    CHECK(m.Frame(ui, input).clicks.empty());
    input.BeginFrame();
    input.Apply(PointerMovedEvent{{110, 20}});
    input.Apply(PointerMovedEvent{{20, 20}});
    input.Apply(PointerButtonReleasedEvent{PointerButton::Left});
    CHECK(m.Frame(ui, input).clicks.empty());
    CHECK_FALSE(ui.GetState().pressed.IsValid());
}
TEST_CASE("Button capture is cancelled by hidden deleted and focus lost targets", "[ui][button]")
{
    for (int mode = 0; mode < 3; ++mode)
    {
        Menu m;
        UIInteraction ui;
        InputState input;
        input.Apply(PointerMovedEvent{{20, 20}});
        input.Apply(PointerButtonPressedEvent{PointerButton::Left});
        m.Frame(ui, input);
        input.BeginFrame();
        if (mode == 0)
            m.scene.GetComponent<UIRectComponent>(m.first)->enabled = false;
        if (mode == 1)
            REQUIRE(m.scene.DestroyEntity(m.first));
        if (mode == 2)
            input.Apply(WindowFocusLostEvent{});
        input.Apply(PointerButtonReleasedEvent{PointerButton::Left});
        auto frame = m.Frame(ui, input);
        CHECK(frame.clicks.empty());
        CHECK_FALSE(frame.gameplay.WasPointerButtonReleased(PointerButton::Left));
    }
}
TEST_CASE("Button keyboard focus wraps skips disabled and confirms once on release", "[ui][button]")
{
    Menu m;
    UIInteraction ui;
    InputState input;
    input.Apply(KeyPressedEvent{KeyCode::ArrowDown});
    input.Apply(KeyReleasedEvent{KeyCode::ArrowDown});
    auto frame = m.Frame(ui, input);
    CHECK(ui.GetState().focused == m.Id(m.first));
    CHECK_FALSE(frame.gameplay.WasKeyPressed(KeyCode::ArrowDown));
    input.BeginFrame();
    input.Apply(KeyPressedEvent{KeyCode::Enter});
    CHECK(m.Frame(ui, input).clicks.empty());
    input.BeginFrame();
    input.Apply(KeyPressedEvent{KeyCode::Enter, true});
    input.Apply(KeyReleasedEvent{KeyCode::Enter});
    frame = m.Frame(ui, input);
    REQUIRE(frame.clicks.size() == 1);
    CHECK(frame.clicks[0] == m.Id(m.first));
    CHECK_FALSE(frame.gameplay.WasKeyReleased(KeyCode::Enter));
    m.scene.GetComponent<ButtonComponent>(m.second)->interactable = false;
    input.BeginFrame();
    input.Apply(KeyPressedEvent{KeyCode::ArrowDown});
    m.Frame(ui, input);
    CHECK(ui.GetState().focused == m.Id(m.first));
}
TEST_CASE("World held input entering button retains release continuity", "[ui][button]")
{
    Menu m;
    UIInteraction ui;
    InputState input;
    input.Apply(PointerMovedEvent{{300, 20}});
    input.Apply(PointerButtonPressedEvent{PointerButton::Left});
    CHECK(m.Frame(ui, input).gameplay.IsPointerButtonDown(PointerButton::Left));
    input.BeginFrame();
    input.Apply(PointerMovedEvent{{20, 20}});
    CHECK(m.Frame(ui, input).gameplay.IsPointerButtonDown(PointerButton::Left));
    input.BeginFrame();
    input.Apply(PointerButtonReleasedEvent{PointerButton::Left});
    auto frame = m.Frame(ui, input);
    CHECK(frame.clicks.empty());
    CHECK(frame.gameplay.WasPointerButtonReleased(PointerButton::Left));
}
TEST_CASE("Button reflection persists and clones authoring only", "[ui][button]")
{
    Menu m;
    auto registry = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(registry);
    SceneReflection reflection(registry.Value());
    REQUIRE(reflection.ApplyPropertyMutation(m.scene, m.Id(m.first), MakeComponentTypeId("Button"),
                                             MakePropertyId("Button.interactable"), false));
    auto json = SceneSerializer::Serialize(m.scene, registry.Value());
    REQUIRE(json);
    auto reopened = SceneDeserializer::Deserialize(json.Value(), registry.Value());
    REQUIRE(reopened);
    auto clone = SceneCloner::Clone(*reopened.Value(), registry.Value());
    REQUIRE(clone);
    CHECK_FALSE(clone.Value()
                    ->GetComponent<ButtonComponent>(clone.Value()->FindEntity(m.Id(m.first)))
                    ->interactable);
}

TEST_CASE("Button sample shares execution callbacks with runtime clone and neutral Step",
          "[ui][button][runtime]")
{
    const auto project =
        std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path() / "SandboxProject";
    auto reflection = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(reflection);
    auto registry = AssetRegistry::Load(project / "Config/AssetRegistry.json");
    REQUIRE(registry);
    auto authoring =
        SceneDeserializer::Load(project / "Scenes/ButtonShowcase.scene", reflection.Value());
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
    auto runtime = RuntimeSession::Start(*authoring.Value(), reflection.Value(), assets, input);
    REQUIRE(runtime);
    const auto id = UUID::Parse("fb200000-0000-4000-8000-000000000004").Value();
    auto value = [&](const Scene& scene)
    { return scene.GetComponent<TextComponent>(scene.FindEntity(id))->content; };
    for (int i = 1; i <= 3; ++i)
    {
        Tap(input, {100, 330});
        REQUIRE(execution.Value()->Advance(TimeStep::FromSeconds(.016), input));
        REQUIRE(runtime.Value()->Update(TimeStep::FromSeconds(.016)));
        CHECK(value(*independent.Value()) == "CLICKS " + std::to_string(i));
        CHECK(value(runtime.Value()->GetScene()) == value(*independent.Value()));
    }
    SceneRenderer sceneRenderer;
    REQUIRE(sceneRenderer.Render(runtime.Value()->GetScene(), assets, *renderer, {640, 360},
                                 {1280, 720}, &runtime.Value()->GetUIState()));
    input.BeginFrame();
    input.Apply(PointerButtonPressedEvent{PointerButton::Left});
    REQUIRE(runtime.Value()->Update(TimeStep::FromSeconds(.016)));
    REQUIRE(runtime.Value()->Pause());
    input.BeginFrame();
    input.Apply(PointerButtonReleasedEvent{PointerButton::Left});
    REQUIRE(runtime.Value()->Step());
    CHECK(value(runtime.Value()->GetScene()) == "CLICKS 3");
    REQUIRE(runtime.Value()->Resume());
    REQUIRE(runtime.Value()->Update(TimeStep::FromSeconds(.016)));
    CHECK(value(runtime.Value()->GetScene()) == "CLICKS 3");
    REQUIRE(runtime.Value()->Stop());
    REQUIRE(execution.Value()->Stop());
    CHECK(value(*authoring.Value()) == "CLICKS 0");
}
TEST_CASE("Button event journal maps all positions and fails closed on overflow",
          "[ui][button][input]")
{
    Menu m;
    UIInteraction ui;
    InputState input;
    input.Apply(PointerMovedEvent{{400, 100}});
    input.Apply(PointerButtonPressedEvent{PointerButton::Left});
    input.Apply(PointerMovedEvent{{120, 100}});
    input.Apply(PointerButtonReleasedEvent{PointerButton::Left});
    auto mapped = input.ForViewport({100, 80}, {800, 600}, {800, 600}, true);
    CHECK(m.Frame(ui, mapped).clicks.empty()); // Press began outside button, final point is inside.
    Tap(input, {120, 100});
    mapped = input.ForViewport({100, 80}, {800, 600}, {800, 600}, true);
    REQUIRE(m.Frame(ui, mapped).clicks.size() == 1);
    input.BeginFrame();
    for (int i = 0; i < 300; ++i)
        input.Apply(PointerMovedEvent{{20, 20}});
    input.Apply(PointerButtonPressedEvent{PointerButton::Left});
    input.Apply(PointerButtonReleasedEvent{PointerButton::Left});
    CHECK(input.EventsOverflowed());
    CHECK(m.Frame(ui, input).clicks.empty());
}
TEST_CASE("Button callbacks precede Update and consume script input", "[ui][button][runtime]")
{
    Test::AssetTempDirectory temp;
    REQUIRE(FileSystem::WriteText(temp.Path() / "button.lua", R"(
 local S={}
 function S.OnClick(self) self.clicked=true end
 function S.OnUpdate(self,dt)
  if self.clicked then
   if Input.is_key_down("Enter") or Input.was_key_pressed("Enter") then error("input leaked") end
   self.entity:set_text("updated after click")
  end
 end
 return S
 )"));
    Menu m;
    m.scene.AddComponent<TextComponent>(m.first, {});
    AssetRegistry registry;
    auto script = registry.Register(AssetType::LuaScript, "button.lua");
    REQUIRE(script);
    m.scene.AddComponent<LuaScriptComponent>(m.first, LuaScriptComponent{script.Value(), true});
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    AssetService assets(temp.Path(), registry, *renderer);
    InputState input;
    auto execution = RuntimeExecution::Create(m.scene, assets, input);
    REQUIRE(execution);
    REQUIRE(execution.Value()->Start());
    input.Apply(KeyPressedEvent{KeyCode::ArrowDown});
    input.Apply(KeyReleasedEvent{KeyCode::ArrowDown});
    input.Apply(KeyPressedEvent{KeyCode::Enter});
    input.Apply(KeyReleasedEvent{KeyCode::Enter});
    REQUIRE(execution.Value()->Advance(TimeStep::FromSeconds(.016), input));
    CHECK(m.scene.GetComponent<TextComponent>(m.first)->content == "updated after click");
    REQUIRE(execution.Value()->Stop());
}

TEST_CASE("UI capture remains consumed across Game View loss and held reentry", "[ui][button]")
{
    Menu m;
    UIInteraction ui;
    InputState input;
    input.Apply(PointerMovedEvent{{20, 20}});
    input.Apply(PointerButtonPressedEvent{PointerButton::Left});
    m.Frame(ui, input);
    InputState lost;
    lost.Apply(WindowFocusLostEvent{});
    CHECK(m.Frame(ui, lost).clicks.empty());
    input.BeginFrame();
    input.Apply(PointerMovedEvent{{20, 20}});
    auto frame = m.Frame(ui, input);
    CHECK(frame.clicks.empty());
    CHECK_FALSE(frame.gameplay.IsPointerButtonDown(PointerButton::Left));
    input.BeginFrame();
    input.Apply(PointerButtonReleasedEvent{PointerButton::Left});
    frame = m.Frame(ui, input);
    CHECK(frame.clicks.empty());
    CHECK_FALSE(frame.gameplay.WasPointerButtonReleased(PointerButton::Left));
    Tap(input, {20, 20});
    CHECK(m.Frame(ui, input).clicks.size() == 1);
}
TEST_CASE("Button children resolve ownership while unrelated overlays block clicks", "[ui][button]")
{
    Menu m;
    UIInteraction ui;
    InputState input;
    auto label = m.scene.CreateEntity();
    REQUIRE(m.scene.SetParent(label, m.first));
    m.scene.AddComponent<UIRectComponent>(label, {});
    m.scene.AddComponent<PanelComponent>(label, {});
    Tap(input, {20, 20});
    auto frame = m.Frame(ui, input);
    REQUIRE(frame.clicks.size() == 1);
    CHECK(frame.clicks[0] == m.Id(m.first));
    auto overlay = m.Add({0, 0});
    REQUIRE(m.scene.RemoveComponent<ButtonComponent>(overlay));
    m.scene.AddComponent<PanelComponent>(overlay, {});
    Tap(input, {20, 20});
    CHECK(m.Frame(ui, input).clicks.empty());
}
TEST_CASE("OnClick failures are protected and stale script targets are skipped",
          "[ui][button][runtime]")
{
    Test::AssetTempDirectory temp;
    REQUIRE(FileSystem::WriteText(temp.Path() / "bad.lua",
                                  "return {OnClick=function(self) error('click failed') end}"));
    Menu m;
    AssetRegistry registry;
    auto handle = registry.Register(AssetType::LuaScript, "bad.lua");
    REQUIRE(handle);
    m.scene.AddComponent<LuaScriptComponent>(m.first, LuaScriptComponent{handle.Value(), true});
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    AssetService assets(temp.Path(), registry, *renderer);
    InputState input;
    auto reflection = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(reflection);
    auto runtime = RuntimeSession::Start(m.scene, reflection.Value(), assets, input);
    REQUIRE(runtime);
    Tap(input, {20, 20});
    auto failed = runtime.Value()->Update(TimeStep::FromSeconds(.016));
    CHECK_FALSE(failed);
    CHECK(runtime.Value()->GetState() == RuntimeState::Faulted);
    REQUIRE(runtime.Value()->GetStatus().lastError);
    CHECK(runtime.Value()->GetStatus().lastError->message.find("OnClick") != std::string::npos);
    CHECK(runtime.Value()->GetStatus().lastError->message.find("click failed") !=
          std::string::npos);
    REQUIRE(runtime.Value()->Stop());
    auto scripts = ScriptEngine::Create(m.scene, assets, input);
    REQUIRE(scripts);
    REQUIRE(scripts.Value()->Start());
    auto id = m.Id(m.first);
    m.scene.GetComponent<LuaScriptComponent>(m.first)->enabled = false;
    REQUIRE(scripts.Value()->DispatchButtonClick(id));
    REQUIRE(m.scene.DestroyEntity(m.first));
    REQUIRE(scripts.Value()->DispatchButtonClick(id));
    REQUIRE(scripts.Value()->Stop());
}

TEST_CASE("Keyboard focus traverses all buttons in paint order and wraps both directions",
          "[ui][button]")
{
    Menu m;
    UIInteraction ui;
    InputState input;
    auto key = [&](KeyCode key)
    {
        input.BeginFrame();
        input.Apply(KeyPressedEvent{key});
        input.Apply(KeyReleasedEvent{key});
        return m.Frame(ui, input);
    };
    key(KeyCode::ArrowDown);
    CHECK(ui.GetState().focused == m.Id(m.first));
    key(KeyCode::ArrowDown);
    CHECK(ui.GetState().focused == m.Id(m.second));
    key(KeyCode::ArrowDown);
    CHECK(ui.GetState().focused == m.Id(m.first));
    key(KeyCode::ArrowUp);
    CHECK(ui.GetState().focused == m.Id(m.second));
    auto frame = key(KeyCode::Space);
    REQUIRE(frame.clicks.size() == 1);
    CHECK(frame.clicks[0] == m.Id(m.second));
}
TEST_CASE("Primed input never replays startup or paused clicks", "[ui][button]")
{
    Menu m;
    UIInteraction ui;
    InputState input;
    Tap(input, {20, 20});
    ui.Prime(input);
    CHECK(m.Frame(ui, input).clicks.empty());
    Tap(input, {20, 20});
    CHECK(m.Frame(ui, input).clicks.size() == 1);
    ui.Cancel();
    Tap(input, {20, 20});
    ui.Prime(input);
    CHECK(m.Frame(ui, input).clicks.empty());
}

TEST_CASE("Game View filters presses originating in tools even when frame ends inside",
          "[ui][button][input]")
{
    InputState input;
    input.Apply(PointerMovedEvent{{20, 20}});
    input.Apply(KeyPressedEvent{KeyCode::Enter});
    input.Apply(PointerButtonPressedEvent{PointerButton::Left});
    input.Apply(PointerMovedEvent{{120, 120}});
    const auto routed = input.ForViewport({100, 100}, {800, 600}, {800, 600}, true);
    CHECK_FALSE(routed.IsKeyDown(KeyCode::Enter));
    CHECK_FALSE(routed.WasKeyPressed(KeyCode::Enter));
    CHECK_FALSE(routed.IsPointerButtonDown(PointerButton::Left));
    for (const auto& event : routed.GetEvents())
    {
        CHECK_FALSE(std::holds_alternative<KeyPressedEvent>(event));
        CHECK_FALSE(std::holds_alternative<PointerButtonPressedEvent>(event));
    }
}

TEST_CASE("Button renderer reads transient focus pressed and disabled colors",
          "[ui][button][render]")
{
    Menu m;
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    AssetRegistry registry;
    AssetService assets(".", registry, *renderer);
    SceneRenderer sceneRenderer;
    auto* button = m.scene.GetComponent<ButtonComponent>(m.first);
    button->color = {1, 0, 0, 1};
    button->focusedColor = {0, 1, 0, 1};
    button->pressedColor = {0, 0, 1, 1};
    button->disabledColor = {.5f, .5f, .5f, 1};
    UIInteractionState state;
    auto check = [&](ColorValue color)
    {
        device.vertexUploads.clear();
        REQUIRE(sceneRenderer.Render(m.scene, assets, *renderer, {800, 600}, {800, 600}, &state));
        REQUIRE_FALSE(device.vertexUploads.empty());
        REQUIRE(device.vertexUploads[0].size() >= sizeof(f32) * 8);
        f32 vertex[8];
        std::memcpy(vertex, device.vertexUploads[0].data(), sizeof(vertex));
        CHECK(vertex[4] == color.r);
        CHECK(vertex[5] == color.g);
        CHECK(vertex[6] == color.b);
    };
    check(button->color);
    state.focused = m.Id(m.first);
    check(button->focusedColor);
    state.pressed = m.Id(m.first);
    check(button->pressedColor);
    button->interactable = false;
    check(button->disabledColor);
}
