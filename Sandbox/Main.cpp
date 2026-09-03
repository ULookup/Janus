#include "Application/Application.h"
#include "Application/ApplicationClient.h"

#include "Core/Event/Event.h"
#include "Core/Log/Log.h"
#include "Core/Time/TimeStep.h"
#include "Scene/Scene.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <variant>

namespace
{

std::filesystem::path ResolveProjectRoot(int argc, char** argv)
{
    if (argc > 1 && argv[1] != nullptr)
    {
        return std::filesystem::path(argv[1]);
    }

    std::error_code error;
    if (argc > 0 && argv[0] != nullptr)
    {
        const std::filesystem::path executable =
            std::filesystem::absolute(argv[0], error);
        if (!error)
        {
            return executable.parent_path() / "SandboxProject";
        }
    }

    error.clear();
    const std::filesystem::path current =
        std::filesystem::current_path(error);
    return error
        ? std::filesystem::path("SandboxProject")
        : current / "SandboxProject";
}

class SandboxClient final : public Janus::ApplicationClient
{
public:
    Janus::Result<void> OnInitialize(Janus::Application& application) override
    {
        auto& scene = application.GetScene();
        const auto entities = scene.GetEntities();
        JANUS_INFO(
            "Loaded Scene '{}' from disk with {} entities.",
            scene.GetMetadata().name,
            entities.size());
        return Janus::Result<void>::Success();
    }

    void OnEvent(const Janus::Event& event, Janus::Application&) override
    {
        if (const auto* resize = std::get_if<Janus::WindowResizeEvent>(&event))
        {
            JANUS_INFO("Window resized to {}x{}.", resize->width, resize->height);
        }
    }

    void OnUpdate(Janus::TimeStep, Janus::Application& application) override
    {
        if (application.GetInput().WasKeyPressed(Janus::KeyCode::Escape))
        {
            application.RequestExit();
        }
    }
};

} // namespace

int main(int argc, char** argv)
{
    Janus::ApplicationConfig config;
    config.window.title = "Janus Sandbox";
    config.window.width = 1280;
    config.window.height = 720;
    config.window.resizable = true;

    Janus::ProjectRuntimeConfig project;
    project.root = ResolveProjectRoot(argc, argv);
    config.project = std::move(project);

    Janus::Application application(config);
    SandboxClient client;
    const auto result = application.Run(client);

    if (!result)
    {
        std::fprintf(
            stderr,
            "Janus failed: %s\n",
            result.GetError().message.c_str());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
