#include "Core/Log/Log.h"

#include "Platform/Platform.h"
#include "Platform/Window/Window.h"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <thread>
#include <utility>

int main()
{
    Janus::Log::Initialize();

    JANUS_CORE_INFO(
        "Janus Engine starting.");

    const auto platformResult =
        Janus::Platform::Initialize();

    if (!platformResult)
    {
        JANUS_CORE_CRITICAL(
            "Platform initialization failed: {}",
            platformResult.GetError().message);

        Janus::Log::Shutdown();

        return EXIT_FAILURE;
    }

    Janus::WindowConfig config;

    config.title = "Janus Sandbox";
    config.width = 1280;
    config.height = 720;
    config.resizable = true;

    auto windowResult =
        Janus::Window::Create(config);

    if (!windowResult)
    {
        JANUS_CORE_CRITICAL(
            "Window creation failed: {}",
            windowResult.GetError().message);

        Janus::Platform::Shutdown();
        Janus::Log::Shutdown();

        return EXIT_FAILURE;
    }

    auto window = std::move(windowResult).Value();

    while (!window->ShouldClose())
    {
        window->PollEvents(
            [](const Janus::Event&)
            {
            });

        std::this_thread::sleep_for(
            std::chrono::milliseconds(1));
    }

    // Window must be destroyed before SDL shuts down.
    window.reset();

    Janus::Platform::Shutdown();

    Janus::Log::Shutdown();

    return EXIT_SUCCESS;
}
