#include "Application/Application.h"
#include "Application/ApplicationClient.h"

#include "Core/Event/Event.h"
#include "Core/Log/Log.h"
#include "Core/Time/TimeStep.h"

#include <cstdio>
#include <cstdlib>
#include <variant>

class SandboxClient final : public Janus::ApplicationClient
{
public:
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

int main()
{
    Janus::ApplicationConfig config;
    config.window.title = "Janus Sandbox";
    config.window.width = 1280;
    config.window.height = 720;
    config.window.resizable = true;

    Janus::Application application(config);
    SandboxClient client;
    const auto result = application.Run(client);

    if (!result)
    {
        std::fprintf(stderr, "Janus failed: %s\n", result.GetError().message.c_str());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
