#include "McpEditorHost.h"

#include "ProjectSession.h"

#include "Resources/SceneResources.h"
#include "Tools/SceneTools.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
#endif

namespace Janus::Editor
{
namespace
{

MCP::McpServerConfig BuildServerConfig()
{
    MCP::McpServerConfig config;
    config.name = "JanusEditor";
    config.version = "0.8.0";
    config.instructions =
        "Read and author the live Janus EditorScene using stable UUIDs, "
        "Reflection metadata, and command-backed tools.";
    config.capabilities = {
        {"tools",
         MCP::Json{
             {"listChanged", false}}},
        {"resources",
         MCP::Json{
             {"subscribe", false},
             {"listChanged", false}}}};

    return config;
}

MCP::McpDispatchError PermissionDenied(
    MCP::McpOperation operation,
    std::string target,
    const Error& error)
{
    return MCP::McpDispatchError{
        MCP::McpPermissionDenied,
        error.message.empty()
            ? "MCP operation denied by host permission policy."
            : error.message,
        MCP::Json{
            {"operation",
             static_cast<i32>(operation)},
            {"target",
             std::move(target)}}};
}

} // namespace

Result<std::unique_ptr<McpEditorHost>> McpEditorHost::Create(
    ProjectSession& project,
    std::istream& input,
    std::ostream& output,
    const MCP::IMcpPermissionPolicy& permissionPolicy,
    usize maxRequestsPerPump)
{
    auto host =
        std::unique_ptr<McpEditorHost>(
            new McpEditorHost(
                project,
                input,
                output,
                permissionPolicy,
                maxRequestsPerPump));

    auto registered =
        host->RegisterCapabilities();
    if (!registered)
    {
        return Result<std::unique_ptr<McpEditorHost>>::Failure(
            registered.GetError());
    }

    return Result<std::unique_ptr<McpEditorHost>>::Success(
        std::move(host));
}

McpEditorHost::McpEditorHost(
    ProjectSession& project,
    std::istream& input,
    std::ostream& output,
    const MCP::IMcpPermissionPolicy& permissionPolicy,
    usize maxRequestsPerPump)
    : m_Project(project),
      m_PermissionPolicy(permissionPolicy),
      m_Router(m_Tools, m_Resources),
      m_Dispatcher(maxRequestsPerPump),
      m_Protocol(BuildServerConfig()),
      m_Transport(input, output)
{
    m_Protocol.SetRequestHandler(
        [this](
            std::string_view method,
            const MCP::Json& params,
            MCP::McpProtocolEra era)
        {
            return DispatchRequest(
                method,
                params,
                era);
        });
}

McpEditorHost::~McpEditorHost()
{
    Stop();
}

Result<void> McpEditorHost::RegisterCapabilities()
{
    auto resources =
        MCP::RegisterSceneResources(
            m_Resources,
            MCP::McpSceneResourceContext{
                &m_Project.GetEditorScene(),
                &m_Project.GetReflectionRegistry(),
                &m_Project.GetAssetRegistry(),
                [this]()
                {
                    std::string displayPath =
                        m_Project.GetProjectRoot()
                            .filename()
                            .generic_string();

                    if (displayPath.empty())
                    {
                        displayPath = ".";
                    }

                    return Result<MCP::McpProjectReadState>::Success(
                        MCP::McpProjectReadState{
                            std::move(displayPath),
                            m_Project.IsDirty(),
                            m_Project.IsPlaying()});
                }});
    if (!resources)
    {
        return resources;
    }

    return MCP::RegisterSceneTools(
        m_Tools,
        MCP::McpSceneToolContext{
            &m_Project.GetEditorScene(),
            &m_Project.GetReflectionRegistry(),
            &m_Project.GetCommandBus(),
            &m_Project.GetAssetRegistry(),
            [this]()
            {
                return m_Project.SaveCurrentScene();
            },
            [this]()
            {
                m_Project.MarkDirty();
            },
            [this]()
            {
                return m_Project.IsPlaying();
            }});
}

Result<void> McpEditorHost::Start()
{
    if (m_Worker.joinable()
        || m_Running.load())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Janus MCP Editor host is already running.");
    }

    if (m_Dispatcher.IsStopped())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Janus MCP Editor host cannot restart after shutdown.");
    }

    {
        std::lock_guard lock(
            m_ErrorMutex);
        m_WorkerError.reset();
    }

    m_Stopping.store(false);
    m_Running.store(true);

    try
    {
        m_Worker =
            std::thread(
                [this]()
                {
                    RunWorker();
                });
    }
    catch (...)
    {
        m_Running.store(false);
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Failed to start Janus MCP stdio worker thread.");
    }

    return Result<void>::Success();
}

Result<usize> McpEditorHost::Pump()
{
    auto pumped =
        m_Dispatcher.Pump();
    if (!pumped)
    {
        return pumped;
    }

    if (const auto workerError =
            GetWorkerError();
        workerError.has_value())
    {
        return Result<usize>::Failure(
            *workerError);
    }

    return pumped;
}

void McpEditorHost::Stop() noexcept
{
    if (m_Stopping.exchange(true))
    {
        if (m_Worker.joinable())
        {
            m_Worker.join();
        }
        return;
    }

    m_Dispatcher.Stop();
    InterruptWorkerRead();

    if (m_Worker.joinable())
    {
        m_Worker.join();
    }

    m_Running.store(false);
}

bool McpEditorHost::IsRunning() const noexcept
{
    return m_Running.load();
}

std::optional<Error> McpEditorHost::GetWorkerError() const
{
    std::lock_guard lock(
        m_ErrorMutex);

    return m_WorkerError;
}

MCP::McpDispatchResult McpEditorHost::DispatchRequest(
    std::string_view method,
    const MCP::Json& params,
    MCP::McpProtocolEra era)
{
    const std::string ownedMethod{
        method};
    const MCP::Json ownedParams =
        params;

    return m_Dispatcher.Invoke(
        [this,
         method = ownedMethod,
         params = ownedParams,
         era]()
        {
            const MCP::McpOperation operation =
                MCP::ClassifyMcpOperation(
                    method,
                    params);

            const std::string target =
                MCP::McpRequestTarget(
                    method,
                    params);

            auto authorized =
                m_PermissionPolicy.Authorize(
                    operation,
                    MCP::McpRequestContext{
                        method,
                        target,
                        era});

            if (!authorized)
            {
                return MCP::McpDispatchResult{
                    PermissionDenied(
                        operation,
                        target,
                        authorized.GetError())};
            }

            return m_Router.HandleRequest(
                method,
                params,
                era);
        });
}

void McpEditorHost::RunWorker() noexcept
{
    if (std::getenv("JANUS_MCP_STARTUP_TRACE") != nullptr)
    {
        std::fprintf(
            stderr,
            "[Janus MCP startup] stdio worker entered\n");
        std::fflush(stderr);
    }

    auto served =
        MCP::ServeStdioProtocol(
            m_Transport,
            m_Protocol);

    if (!served
        && !m_Stopping.load())
    {
        RecordWorkerError(
            served.GetError());
    }

    m_Running.store(false);
}

void McpEditorHost::InterruptWorkerRead() noexcept
{
#if defined(_WIN32)
    if (!m_Worker.joinable()
        || !m_Running.load())
    {
        return;
    }

    const HANDLE threadHandle =
        static_cast<HANDLE>(
            m_Worker.native_handle());

    if (threadHandle != nullptr)
    {
        static_cast<void>(
            ::CancelSynchronousIo(
                threadHandle));
    }
#endif
}

void McpEditorHost::RecordWorkerError(
    Error error) noexcept
{
    try
    {
        std::lock_guard lock(
            m_ErrorMutex);
        m_WorkerError =
            std::move(error);
    }
    catch (...)
    {
    }
}

} // namespace Janus::Editor
