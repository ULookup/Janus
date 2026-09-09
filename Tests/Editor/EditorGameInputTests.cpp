#include "EditorGameInput.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Game image keeps pointer presses and releases while rejecting editor field input",
          "[editor][game-input][d2]")
{
    struct Context
    {
        Context()
        {
            ImGui::CreateContext();
            auto& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.DisplaySize = {800, 600};
            io.DeltaTime = 1.0f / 60;
            io.Fonts->AddFontDefault();
            io.Fonts->Build();
            io.Fonts->SetTexID(ImTextureID{1});
        }
        ~Context()
        {
            ImGui::DestroyContext();
        }
    } context;
    const auto frame = [](bool inspector = false, bool text = false)
    {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({800, 600});
        ImGui::Begin("Game", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoTitleBar);
        ImGui::Image(ImTextureRef{ImTextureID{1}}, {500, 400});
        ImGui::GetIO().WantTextInput = text;
        const bool accepts = Janus::Editor::AcceptGameViewInput(inspector);
        ImGui::SetCursorPos({600, 450});
        ImGui::InvisibleButton("Editor tool", {100, 60});
        ImGui::End();
        ImGui::Render();
        return accepts;
    };
    frame();
    ImGui::GetIO().AddMousePosEvent(100, 100);
    REQUIRE(frame());
    ImGui::GetIO().AddMouseButtonEvent(0, true);
    REQUIRE(frame());
    CHECK(frame()); // ImGui holds a background MoveId even for a NoMove window.
    ImGui::GetIO().AddMouseButtonEvent(0, false);
    CHECK(frame());
    CHECK_FALSE(frame(true));
    CHECK_FALSE(frame(false, true));
    ImGui::GetIO().AddMousePosEvent(650, 480);
    CHECK_FALSE(frame());
    ImGui::GetIO().AddMouseButtonEvent(0, true);
    CHECK_FALSE(frame());
    ImGui::GetIO().AddMousePosEvent(100, 100);
    CHECK_FALSE(frame()); // A captured tool drag must not leak into the Game image.
    ImGui::GetIO().AddMouseButtonEvent(0, false);
    frame();
}
