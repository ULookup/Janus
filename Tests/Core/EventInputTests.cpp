#include "Core/Event/Event.h"
#include "Core/Input/InputState.h"

#include <catch2/catch_test_macros.hpp>

#include <variant>

TEST_CASE("Event variant preserves resize data", "[core][event]")
{
    const Janus::Event event = Janus::WindowResizeEvent{1280, 720};
    const auto& resize = std::get<Janus::WindowResizeEvent>(event);

    REQUIRE(resize.width == 1280);
    REQUIRE(resize.height == 720);
}

TEST_CASE("InputState separates held and per-frame key state", "[core][input]")
{
    Janus::InputState input;

    input.BeginFrame();
    input.Apply(Janus::KeyPressedEvent{Janus::KeyCode::W, false});
    REQUIRE(input.IsKeyDown(Janus::KeyCode::W));
    REQUIRE(input.WasKeyPressed(Janus::KeyCode::W));

    input.BeginFrame();
    REQUIRE(input.IsKeyDown(Janus::KeyCode::W));
    REQUIRE_FALSE(input.WasKeyPressed(Janus::KeyCode::W));

    input.Apply(Janus::KeyPressedEvent{Janus::KeyCode::W, true});
    REQUIRE_FALSE(input.WasKeyPressed(Janus::KeyCode::W));

    input.Apply(Janus::KeyReleasedEvent{Janus::KeyCode::W});
    REQUIRE_FALSE(input.IsKeyDown(Janus::KeyCode::W));
    REQUIRE(input.WasKeyReleased(Janus::KeyCode::W));
}

TEST_CASE("InputState ignores window events", "[core][input]")
{
    Janus::InputState input;
    input.Apply(Janus::WindowResizeEvent{1280, 720});
    input.Apply(Janus::WindowCloseEvent{});

    REQUIRE_FALSE(input.IsKeyDown(Janus::KeyCode::Escape));
    REQUIRE_FALSE(input.WasKeyPressed(Janus::KeyCode::Escape));
    REQUIRE_FALSE(input.WasKeyReleased(Janus::KeyCode::Escape));
}

TEST_CASE("InputState does not report an initial repeated key press", "[core][input]")
{
    Janus::InputState input;
    input.Apply(Janus::KeyPressedEvent{Janus::KeyCode::D, true});

    REQUIRE(input.IsKeyDown(Janus::KeyCode::D));
    REQUIRE_FALSE(input.WasKeyPressed(Janus::KeyCode::D));
}

TEST_CASE("InputState preserves held state and clears transient state at frame start", "[core][input]")
{
    Janus::InputState input;
    input.Apply(Janus::KeyPressedEvent{Janus::KeyCode::Digit9, false});
    input.Apply(Janus::KeyReleasedEvent{Janus::KeyCode::Digit9});
    REQUIRE(input.WasKeyPressed(Janus::KeyCode::Digit9));
    REQUIRE(input.WasKeyReleased(Janus::KeyCode::Digit9));

    input.Apply(Janus::KeyPressedEvent{Janus::KeyCode::Digit9, false});
    input.BeginFrame();

    REQUIRE(input.IsKeyDown(Janus::KeyCode::Digit9));
    REQUIRE_FALSE(input.WasKeyPressed(Janus::KeyCode::Digit9));
    REQUIRE_FALSE(input.WasKeyReleased(Janus::KeyCode::Digit9));
}

TEST_CASE("KeyCode count is the exclusive bitset upper bound", "[core][input]")
{
    REQUIRE(static_cast<Janus::usize>(Janus::KeyCode::Count) == 21);
}
