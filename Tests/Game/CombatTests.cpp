#include "../Renderer/FakeRenderDevice.h"
#include "Animation/AnimationSystem.h"
#include "ProjectSession.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "UI/UIComponents.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace
{
struct CombatFixture
{
    Janus::Test::FakeRenderDevice device;
    std::unique_ptr<Janus::Renderer2D> renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    std::unique_ptr<Janus::Editor::ProjectSession> project;
    Janus::InputState input;

    explicit CombatFixture(bool verification = false)
    {
        Janus::ProjectRuntimeConfig config;
        config.root = std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path() / "Game";
        if (verification)
            config.startupScenePath = "Scenes/CombatVerification.scene";
        auto opened = Janus::Editor::ProjectSession::Open(config, *renderer);
        REQUIRE(opened);
        project = std::move(opened).Value();
        REQUIRE(project->StartRuntime(input, verification));
    }

    Janus::ScriptSnapshot Snapshot() const
    {
        auto snapshot = project->GetRuntimeSession()->GetSnapshot();
        REQUIRE(snapshot);
        return *snapshot;
    }

    void Tick()
    {
        REQUIRE(project->UpdateRuntime(Janus::TimeStep::FromSeconds(1.0 / 60.0)));
    }

    void Click(float x, float y)
    {
        input.BeginFrame();
        input.Apply(Janus::PointerMovedEvent{{x, y}});
        input.Apply(Janus::PointerButtonPressedEvent{Janus::PointerButton::Left});
        input.Apply(Janus::PointerButtonReleasedEvent{Janus::PointerButton::Left});
        Tick();
    }

    void Key(Janus::KeyCode key)
    {
        input.BeginFrame();
        input.Apply(Janus::KeyPressedEvent{key, false});
        input.Apply(Janus::KeyReleasedEvent{key});
        Tick();
    }
};
} // namespace

TEST_CASE("Combat pointer input completes victory defeat and restart without authoring mutation",
          "[combat][snapshot][v0.10]")
{
    CombatFixture fixture;
    const auto authoring = Janus::SceneSerializer::Serialize(
        fixture.project->GetEditorScene(), fixture.project->GetReflectionRegistry());
    REQUIRE(authoring);
    auto fields = [&] { return fixture.Snapshot().fields; };
    REQUIRE(std::get<std::string>(fields().at("phase")) == "menu");
    fixture.Click(940, 530); // Playing a card outside battle is a no-op.
    REQUIRE(std::get<double>(fields().at("turn")) == 0);
    const auto playId = Janus::UUID::Parse("fc200000-0000-4000-8000-000000000024").Value();
    REQUIRE_FALSE(fixture.project->GetRuntimeSession()->GetAnimations().GetPose(playId));
    REQUIRE(std::get<std::string>(fields().at("cardAudioStatus")) == "Stopped");
    fixture.Click(180, 245); // Start.
    fixture.Click(940, 530); // Must select a card before playing.
    REQUIRE(std::get<double>(fields().at("enemyHp")) == 12);
    for (int hp : {8, 4, 0})
    {
        fixture.Click(180, 390); // Strike card.
        REQUIRE(std::get<double>(fields().at("enemyHp")) == hp + 4);
        fixture.Click(940, 530); // Play selected card.
        const auto* pose = fixture.project->GetRuntimeSession()->GetAnimations().GetPose(playId);
        REQUIRE(pose);
        CHECK(pose->frameIndex == 0);
        CHECK(pose->playing);
        CHECK(std::get<std::string>(fields().at("cardAudioStatus")) == "Playing");
        REQUIRE(std::get<double>(fields().at("enemyHp")) == hp);
        REQUIRE(std::get<double>(fields().at("lastDamage")) == 4);
    }
    REQUIRE(std::get<std::string>(fields().at("phase")) == "victory");
    REQUIRE(std::get<double>(fields().at("playerHp")) == 2);
    fixture.Click(940, 530);
    REQUIRE(std::get<double>(fields().at("turn")) == 3);
    fixture.Click(940, 245); // Restart to menu.
    REQUIRE(std::get<double>(fields().at("turn")) == 0);
    REQUIRE(std::get<double>(fields().at("lastDamage")) == 0);
    fixture.Click(180, 245);
    for (int hp : {4, 2, 0})
    {
        fixture.Click(940, 390); // Wait card.
        fixture.Click(940, 530);
        REQUIRE(std::get<double>(fields().at("playerHp")) == hp);
    }
    REQUIRE(std::get<std::string>(fields().at("phase")) == "defeat");
    REQUIRE(std::get<double>(fields().at("enemyHp")) == 12);
    REQUIRE_FALSE(fixture.project->IsDirty());
    REQUIRE(fixture.project->GetCommandBus().GetHistorySize() == 0);
    const auto id = fixture.Snapshot().runtimeId;
    REQUIRE(fixture.project->StopRuntime());
    const auto restored = Janus::SceneSerializer::Serialize(
        fixture.project->GetEditorScene(), fixture.project->GetReflectionRegistry());
    REQUIRE(restored);
    REQUIRE(restored.Value() == authoring.Value());
    REQUIRE(fixture.project->StartRuntime(fixture.input, true));
    REQUIRE(fixture.Snapshot().runtimeId != id);
    REQUIRE(std::get<std::string>(fields().at("phase")) == "menu");
}

TEST_CASE("Combat keyboard follows the same rules and neutral step cannot activate buttons",
          "[combat][v0.10]")
{
    CombatFixture fixture;
    fixture.Key(Janus::KeyCode::ArrowDown); // Start.
    fixture.Key(Janus::KeyCode::Enter);
    fixture.Key(Janus::KeyCode::ArrowDown); // Restart.
    fixture.Key(Janus::KeyCode::ArrowDown); // Strike.
    fixture.Key(Janus::KeyCode::Space);
    fixture.Key(Janus::KeyCode::ArrowDown); // Wait.
    fixture.Key(Janus::KeyCode::ArrowDown); // Play.
    fixture.Key(Janus::KeyCode::Enter);
    REQUIRE(std::get<double>(fixture.Snapshot().fields.at("enemyHp")) == 8);
    for (int i = 0; i < 2; ++i)
    {
        fixture.Key(Janus::KeyCode::ArrowUp); // Wait.
        fixture.Key(Janus::KeyCode::ArrowUp); // Strike.
        fixture.Key(Janus::KeyCode::Space);
        fixture.Key(Janus::KeyCode::ArrowDown);
        fixture.Key(Janus::KeyCode::ArrowDown); // Play.
        fixture.Key(Janus::KeyCode::Enter);
    }
    REQUIRE(std::get<std::string>(fixture.Snapshot().fields.at("phase")) == "victory");
    fixture.Key(Janus::KeyCode::ArrowDown); // Wrap to Start.
    fixture.Key(Janus::KeyCode::ArrowDown); // Restart.
    fixture.Key(Janus::KeyCode::Enter);
    REQUIRE(std::get<double>(fixture.Snapshot().fields.at("enemyHp")) == 12);
    fixture.Key(Janus::KeyCode::ArrowUp); // Start.
    fixture.Key(Janus::KeyCode::Enter);
    fixture.Key(Janus::KeyCode::ArrowDown); // Restart.
    fixture.Key(Janus::KeyCode::ArrowDown); // Strike.
    fixture.Key(Janus::KeyCode::ArrowDown); // Wait.
    for (int i = 0; i < 3; ++i)
    {
        fixture.Key(Janus::KeyCode::Space);
        fixture.Key(Janus::KeyCode::ArrowDown); // Play.
        fixture.Key(Janus::KeyCode::Enter);
        if (i < 2)
            fixture.Key(Janus::KeyCode::ArrowUp); // Wait.
    }
    REQUIRE(std::get<std::string>(fixture.Snapshot().fields.at("phase")) == "defeat");
    fixture.Key(Janus::KeyCode::ArrowDown); // Start.
    fixture.Key(Janus::KeyCode::ArrowDown); // Restart would reset the state if replayed.
    REQUIRE(fixture.project->PauseRuntime());
    fixture.input.BeginFrame();
    fixture.input.Apply(Janus::KeyPressedEvent{Janus::KeyCode::Enter, false});
    fixture.input.Apply(Janus::KeyReleasedEvent{Janus::KeyCode::Enter});
    REQUIRE(fixture.project->StepRuntime());
    REQUIRE(std::get<double>(fixture.Snapshot().fields.at("turn")) == 3);
}

TEST_CASE("Game verification scene exports deterministic damage on neutral steps",
          "[combat][snapshot][v0.10]")
{
    CombatFixture fixture(true);
    REQUIRE(fixture.project->StepRuntime()); // Start.
    REQUIRE(fixture.project->StepRuntime()); // Select.
    REQUIRE(fixture.project->StepRuntime()); // Play.
    REQUIRE(std::get<double>(fixture.Snapshot().fields.at("enemyHp")) == 8);
    REQUIRE(fixture.Snapshot().frameIndex == 3);
    for (int i = 0; i < 4; ++i)
        REQUIRE(fixture.project->StepRuntime());
    REQUIRE(std::get<std::string>(fixture.Snapshot().fields.at("phase")) == "victory");
}
