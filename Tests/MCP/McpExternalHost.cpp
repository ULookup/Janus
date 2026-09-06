#include "McpEditorHost.h"
#include "ProjectSession.h"

#include "Host/McpPermissionPolicy.h"
#include "Renderer/Renderer2D.h"

#include "FakeRenderDevice.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <thread>

int main(int argc, char** argv)
{
    if (argc != 3
        || std::string_view{argv[1]} != "--project")
    {
        std::fprintf(
            stderr,
            "Usage: JanusMcpExternalHost --project <project-root>\n");
        return EXIT_FAILURE;
    }

    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(
            device);

    Janus::ProjectRuntimeConfig projectConfig;
    projectConfig.root =
        std::filesystem::path{argv[2]};

    auto project =
        Janus::Editor::ProjectSession::Open(
            projectConfig,
            *renderer);
    if (!project)
    {
        std::fprintf(
            stderr,
            "Failed to open Janus project for external MCP E2E: %s\n",
            project.GetError().message.c_str());
        return EXIT_FAILURE;
    }

    Janus::MCP::AllowAllMcpPermissionPolicy permissions;

    auto host =
        Janus::Editor::McpEditorHost::Create(
            *project.Value(),
            std::cin,
            std::cout,
            permissions);
    if (!host)
    {
        std::fprintf(
            stderr,
            "Failed to create Janus MCP external host: %s\n",
            host.GetError().message.c_str());
        return EXIT_FAILURE;
    }

    auto started =
        host.Value()->Start();
    if (!started)
    {
        std::fprintf(
            stderr,
            "Failed to start Janus MCP external host: %s\n",
            started.GetError().message.c_str());
        return EXIT_FAILURE;
    }

    using namespace std::chrono_literals;

    while (host.Value()->IsRunning())
    {
        auto pumped =
            host.Value()->Pump();
        if (!pumped)
        {
            std::fprintf(
                stderr,
                "Janus MCP external host pump failed: %s\n",
                pumped.GetError().message.c_str());
            host.Value()->Stop();
            return EXIT_FAILURE;
        }

        if (project.Value()->GetRuntimeState() == Janus::RuntimeState::Playing)
            static_cast<void>(
                project.Value()->UpdateRuntime(Janus::TimeStep::FromSeconds(1.0 / 60.0)));
        std::this_thread::sleep_for(1ms);
    }

    const auto workerError =
        host.Value()->GetWorkerError();

    host.Value()->Stop();

    if (project.Value()->GetCommandBus().HasTransaction() ||
        project.Value()->GetCommandBus().RecoveryRequired())
    {
        std::fprintf(stderr, "Disconnect did not clean authoring transaction.\n");
        return EXIT_FAILURE;
    }

    if (workerError.has_value())
    {
        std::fprintf(
            stderr,
            "Janus MCP external host worker failed: %s\n",
            workerError->message.c_str());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
