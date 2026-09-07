#include "McpEditorHost.h"

#include "ProjectSession.h"

#include "Resources/DebugResources.h"
#include "Resources/SceneResources.h"
#include "Tools/SceneTools.h"
#include "Tools/TransactionTools.h"

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
    config.version = JANUS_VERSION_STRING;
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
                        MCP::McpProjectReadState{std::move(displayPath), m_Project.IsDirty(),
                                                 m_Project.IsAuthoringReadOnly()});
                }});
    if (!resources)
    {
        return resources;
    }

    auto debug = MCP::RegisterDebugCapabilities(
        m_Tools, m_Resources,
        {[this]() { return m_Project.GetRuntimeStatus(); },
         [this]() -> const Scene*
         {
             auto* runtime = m_Project.GetRuntimeSession();
             return runtime ? &runtime->GetScene() : nullptr;
         },
         [this](std::string_view name, bool paused)
         {
             if (name == "runtime.play")
                 return m_Project.PlayRuntime(paused);
             if (name == "runtime.pause")
                 return m_Project.PauseRuntime();
             if (name == "runtime.step")
                 return m_Project.StepRuntime();
             return m_Project.StopRuntime();
         },
         &m_Project.GetReflectionRegistry(), &m_Project.GetAssetRegistry(), m_Project.GetLogStore(),
         [this](std::optional<u64> id)
         { return id ? m_Project.FindDiagnosticsFrame(*id) : m_Project.GetDiagnosticsFrame(); },
         [this]() { return m_Project.IsAuthoringReadOnly(); },
         [this]() -> std::optional<ScriptSnapshot>
         {
             const auto* runtime = m_Project.GetRuntimeSession();
             return runtime ? runtime->GetSnapshot() : std::nullopt;
         }});
    if (!debug)
        return debug;

    auto transactions = MCP::RegisterTransactionCapabilities(
        m_Tools, m_Resources,
        {&m_Project.GetCommandBus(), [this](std::string label)
         { return m_Project.BeginAuthoringTransaction(m_Owner, std::move(label)); },
         [this](UUID token, bool commit)
         { return m_Project.FinishAuthoringTransaction(token, m_Owner, commit); }});
    if (!transactions)
        return transactions;
    m_SceneRevision = m_Project.GetSceneRevision();
    return MCP::RegisterSceneTools(
        m_Tools,
        MCP::McpSceneToolContext{
            &m_Project.GetEditorScene(), &m_Project.GetReflectionRegistry(),
            &m_Project.GetCommandBus(), &m_Project.GetAssetRegistry(), [this]()
            { return m_Project.SaveCurrentScene(); }, [this]() { m_Project.MarkDirty(); }, [this]()
            { return m_Project.HasRuntime() || m_Project.GetCommandBus().RecoveryRequired(); },
            [this](std::unique_ptr<ICommand> command, UUID token)
            {
                return m_Project.ExecuteAuthoring(std::move(command), CommandActor::Agent, token,
                                                  m_Owner);
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
    if (std::this_thread::get_id() != m_OwnerThread)
        return Result<usize>::Failure(ErrorCode::InvalidState,
                                      "Editor host must pump on its owner thread.");
    m_Project.ExpireAuthoringTransaction();
    if (!m_Running.load())
        m_Project.CancelAuthoringTransaction(m_Owner);
    if (m_SceneRevision != m_Project.GetSceneRevision())
    {
        m_Tools = {};
        m_Resources = {};
        auto registered = RegisterCapabilities();
        if (!registered)
            return Result<usize>::Failure(registered.GetError());
    }
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
    if (std::this_thread::get_id() == m_OwnerThread)
        m_Project.CancelAuthoringTransaction(m_Owner);
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

            const std::string target = MCP::McpRequestTarget(method, params);

            if (operation == MCP::McpOperation::Unclassified)
            {
                m_Project.GetCommandBus().RecordOperation(
                    CommandActor::Agent, target,
                    Result<void>::Failure(ErrorCode::InvalidState,
                                          "Unclassified operation denied."),
                    m_Owner);
                return MCP::McpDispatchResult{MCP::McpDispatchError{
                    MCP::McpPermissionDenied, "Unclassified operation denied.", nullptr}};
            }

            auto authorized = m_PermissionPolicy.Authorize(
                operation, MCP::McpRequestContext{method, target, era});

            if (!authorized)
            {
                m_Project.GetCommandBus().RecordOperation(
                    CommandActor::Agent, "Permission denied: " + target, authorized, m_Owner);
                if (operation == MCP::McpOperation::SceneWrite ||
                    operation == MCP::McpOperation::TransactionControl)
                    AbortOwnedRequest(params);
                return MCP::McpDispatchResult{
                    PermissionDenied(
                        operation,
                        target,
                        authorized.GetError())};
            }

            const auto before = m_Project.GetCommandBus().GetNextActivitySequence();
            auto response = m_Router.HandleRequest(method, params, era);
            auto* value = std::get_if<MCP::Json>(&response);
            const bool failed = !value || value->value("isError", false);
            if (operation == MCP::McpOperation::SceneWrite && failed)
                AbortOwnedRequest(params);
            if (method == "tools/call" &&
                m_Project.GetCommandBus().GetNextActivitySequence() == before)
                m_Project.GetCommandBus().RecordOperation(
                    CommandActor::Agent, target,
                    failed ? Result<void>::Failure(ErrorCode::InvalidState, "MCP operation failed.")
                           : Result<void>::Success(),
                    m_Owner);
            if (operation == MCP::McpOperation::SceneRead && value && value->contains("contents"))
            {
                for (auto& content : (*value)["contents"])
                {
                    auto payload = MCP::Json::parse(content["text"].get<std::string>());
                    payload["authoringTransaction"] =
                        MCP::TransactionStatusJson(m_Project.GetCommandBus());
                    content["text"] = payload.dump();
                }
            }
            if (operation == MCP::McpOperation::SceneWrite && !failed && value &&
                m_Project.GetCommandBus().GetNextActivitySequence() > before)
            {
                const auto& row = m_Project.GetCommandBus().GetActivity().back();
                (*value)["structuredContent"]["receipt"] = {
                    {"sequence", row.sequence},
                    {"commandId", std::to_string(row.commandId)},
                    {"transaction", row.transaction.ToString()}};
                (*value)["content"][0]["text"] = (*value)["structuredContent"].dump();
            }
            return response;
        });
}

void McpEditorHost::AbortOwnedRequest(const MCP::Json& params)
{
    if (!m_Project.GetCommandBus().HasTransaction() || m_Project.GetTransactionOwner() != m_Owner)
        return;
    const auto arguments = params.find("arguments");
    if (arguments == params.end() || !arguments->is_object())
        return;
    const auto value = arguments->find("transaction");
    if (value == arguments->end() || !value->is_string())
        return;
    auto token = UUID::Parse(value->get<std::string>());
    if (token && token.Value() == m_Project.GetCommandBus().GetTransactionId())
        m_Project.CancelAuthoringTransaction(m_Owner);
}

void McpEditorHost::RunWorker() noexcept
{
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
