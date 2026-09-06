#include "McpEditorHost.h"

#include "Core/Input/InputState.h"
#include "Core/Log/LogStore.h"
#include "EditorActions.h"
#include "EditorContext.h"
#include "ProjectSession.h"
#include "RuntimeSession.h"
#include "Scene/Command/EntityCommands.h"

#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"

#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{

std::filesystem::path SandboxProjectRoot()
{
    return std::filesystem::path(JANUS_TEST_SOURCE_DIR)
        .parent_path()
        / "SandboxProject";
}

struct ProjectFixture
{
    Janus::Test::FakeRenderDevice device;
    std::unique_ptr<Janus::Renderer2D> renderer;
    std::unique_ptr<Janus::Editor::ProjectSession> project;

    ProjectFixture()
    {
        renderer =
            Janus::Detail::Renderer2DTestAccess::Create(
                device);

        Janus::ProjectRuntimeConfig config;
        config.root =
            SandboxProjectRoot();

        auto opened =
            Janus::Editor::ProjectSession::Open(
                config,
                *renderer);

        REQUIRE(opened);

        project =
            std::move(opened).Value();
    }
};

Janus::MCP::Json ModernMeta()
{
    return Janus::MCP::Json{
        {std::string{
             Janus::MCP::McpProtocolVersionMetaKey},
         std::string{
             Janus::MCP::McpModernProtocolVersion}}};
}

std::string EncodeRequest(
    Janus::i32 id,
    std::string method,
    Janus::MCP::Json params)
{
    params["_meta"] =
        ModernMeta();

    return Janus::MCP::Json{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", std::move(method)},
        {"params", std::move(params)}}
        .dump()
        + "\n";
}

Janus::Result<void> PumpUntilWorkerStops(
    Janus::Editor::McpEditorHost& host)
{
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::seconds(3);

    while (host.IsRunning()
        && std::chrono::steady_clock::now() < deadline)
    {
        auto pumped =
            host.Pump();

        if (!pumped)
        {
            return Janus::Result<void>::Failure(
                pumped.GetError());
        }

        std::this_thread::yield();
    }

    if (host.IsRunning())
    {
        return Janus::Result<void>::Failure(
            Janus::ErrorCode::InvalidState,
            "Timed out waiting for MCP test worker.");
    }

    host.Stop();
    return Janus::Result<void>::Success();
}

std::vector<Janus::MCP::Json> ParseResponses(
    const std::string& output)
{
    std::vector<Janus::MCP::Json> responses;
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line))
    {
        if (!line.empty())
        {
            responses.push_back(
                Janus::MCP::Json::parse(line));
        }
    }

    return responses;
}

class DenySceneWritesPolicy final
    : public Janus::MCP::IMcpPermissionPolicy
{
public:
    explicit DenySceneWritesPolicy(
        std::thread::id owner)
        : m_Owner(owner)
    {
    }

    [[nodiscard]] Janus::Result<void> Authorize(
        Janus::MCP::McpOperation operation,
        const Janus::MCP::McpRequestContext&) const override
    {
        REQUIRE(
            std::this_thread::get_id()
            == m_Owner);

        if (operation
            == Janus::MCP::McpOperation::SceneWrite)
        {
            return Janus::Result<void>::Failure(
                Janus::ErrorCode::InvalidState,
                "Scene writes denied by test policy.");
        }

        return Janus::Result<void>::Success();
    }

private:
    std::thread::id m_Owner;
};

} // namespace

TEST_CASE(
    "Live Editor MCP write marks dirty and shares Human undo history",
    "[editor][mcp][host][v0.8]")
{
    ProjectFixture fixture;
    auto& project =
        *fixture.project;

    const Janus::usize beforeEntities =
        project.GetEditorScene()
            .GetEntities()
            .size();

    std::istringstream input(
        EncodeRequest(
            1,
            "tools/call",
            Janus::MCP::Json{
                {"name", "scene.create_entity"},
                {"arguments",
                 Janus::MCP::Json{
                     {"name", "AgentCreated"}}}}));
    std::ostringstream output;

    Janus::MCP::AllowAllMcpPermissionPolicy policy;

    auto hostResult =
        Janus::Editor::McpEditorHost::Create(
            project,
            input,
            output,
            policy);

    REQUIRE(hostResult);
    auto host =
        std::move(hostResult).Value();

    REQUIRE(host->Start());
    REQUIRE(PumpUntilWorkerStops(*host));

    const auto responses =
        ParseResponses(
            output.str());

    REQUIRE(responses.size() == 1);
    REQUIRE(
        responses[0]
            .at("result")
            .at("structuredContent")
            .at("ok")
        == true);

    REQUIRE(project.IsDirty());
    REQUIRE(
        project.GetEditorScene()
            .GetEntities()
            .size()
        == beforeEntities + 1);
    REQUIRE(
        project.GetCommandBus()
            .GetHistorySize()
        == 1);

    REQUIRE(
        project.GetCommandBus().Undo());

    REQUIRE(
        project.GetEditorScene()
            .GetEntities()
            .size()
        == beforeEntities);
}

TEST_CASE(
    "Live Editor MCP permission policy runs on main thread before handler",
    "[editor][mcp][host][permission][v0.8]")
{
    ProjectFixture fixture;
    auto& project =
        *fixture.project;

    const Janus::usize beforeEntities =
        project.GetEditorScene()
            .GetEntities()
            .size();

    std::istringstream input(
        EncodeRequest(
            2,
            "tools/call",
            Janus::MCP::Json{
                {"name", "scene.create_entity"},
                {"arguments",
                 Janus::MCP::Json{
                     {"name", "Denied"}}}}));
    std::ostringstream output;

    DenySceneWritesPolicy policy(
        std::this_thread::get_id());

    auto hostResult =
        Janus::Editor::McpEditorHost::Create(
            project,
            input,
            output,
            policy);

    REQUIRE(hostResult);
    auto host =
        std::move(hostResult).Value();

    REQUIRE(host->Start());
    REQUIRE(PumpUntilWorkerStops(*host));

    const auto responses =
        ParseResponses(
            output.str());

    REQUIRE(responses.size() == 1);
    REQUIRE(
        responses[0]
            .at("error")
            .at("code")
        == Janus::MCP::McpPermissionDenied);
    REQUIRE(
        project.GetEditorScene()
            .GetEntities()
            .size()
        == beforeEntities);
    REQUIRE(
        project.GetCommandBus()
            .GetHistorySize()
        == 0);
    REQUIRE_FALSE(project.IsDirty());
}

TEST_CASE(
    "Live Editor MCP rejects writes during Play without moving history",
    "[editor][mcp][host][play][v0.8]")
{
    ProjectFixture fixture;
    auto& project =
        *fixture.project;

    Janus::InputState inputState;
    inputState.BeginFrame();

    REQUIRE(
        project.StartRuntime(
            inputState));

    const Janus::usize beforeEntities =
        project.GetEditorScene()
            .GetEntities()
            .size();
    const Janus::usize beforeHistory =
        project.GetCommandBus()
            .GetHistorySize();

    std::istringstream input(
        EncodeRequest(
            3,
            "tools/call",
            Janus::MCP::Json{
                {"name", "scene.create_entity"},
                {"arguments",
                 Janus::MCP::Json{
                     {"name", "PlayDenied"}}}}));
    std::ostringstream output;

    Janus::MCP::AllowAllMcpPermissionPolicy policy;

    auto hostResult =
        Janus::Editor::McpEditorHost::Create(
            project,
            input,
            output,
            policy);

    REQUIRE(hostResult);
    auto host =
        std::move(hostResult).Value();

    REQUIRE(host->Start());
    REQUIRE(PumpUntilWorkerStops(*host));

    const auto responses =
        ParseResponses(
            output.str());

    REQUIRE(responses.size() == 1);
    REQUIRE(
        responses[0]
            .at("result")
            .at("isError")
        == true);
    REQUIRE(
        responses[0]
            .at("result")
            .at("structuredContent")
            .at("ok")
        == false);

    REQUIRE(
        project.GetEditorScene()
            .GetEntities()
            .size()
        == beforeEntities);
    REQUIRE(
        project.GetCommandBus()
            .GetHistorySize()
        == beforeHistory);

    REQUIRE(project.StopRuntime());
}

TEST_CASE(
    "Live Editor MCP resources remain authoring-only during Play",
    "[editor][mcp][host][play][resource][v0.8]")
{
    ProjectFixture fixture;
    auto& project =
        *fixture.project;

    const Janus::usize authoringCount =
        project.GetEditorScene()
            .GetEntities()
            .size();

    Janus::InputState inputState;
    inputState.BeginFrame();

    REQUIRE(
        project.StartRuntime(
            inputState));

    REQUIRE(
        project.GetRuntimeSession()
        != nullptr);

    project.GetRuntimeSession()
        ->GetScene()
        .CreateEntity(
            "RuntimeOnly");

    REQUIRE(
        project.GetRuntimeSession()
            ->GetScene()
            .GetEntities()
            .size()
        == authoringCount + 1);

    std::istringstream input(
        EncodeRequest(
            4,
            "resources/read",
            Janus::MCP::Json{
                {"uri",
                 "engine://scene/current"}}));
    std::ostringstream output;

    Janus::MCP::AllowAllMcpPermissionPolicy policy;

    auto hostResult =
        Janus::Editor::McpEditorHost::Create(
            project,
            input,
            output,
            policy);

    REQUIRE(hostResult);
    auto host =
        std::move(hostResult).Value();

    REQUIRE(host->Start());
    REQUIRE(PumpUntilWorkerStops(*host));

    const auto responses =
        ParseResponses(
            output.str());

    REQUIRE(responses.size() == 1);

    const std::string payloadText =
        responses[0]
            .at("result")
            .at("contents")
            .at(0)
            .at("text")
            .get<std::string>();

    const auto payload =
        Janus::MCP::Json::parse(
            payloadText);

    REQUIRE(
        payload.at("entityCount")
        == authoringCount);

    REQUIRE(project.StopRuntime());
}

TEST_CASE(
    "Live Editor MCP drains repeated requests through bounded frame pumps",
    "[editor][mcp][host][repeat][v0.8]")
{
    ProjectFixture fixture;
    auto& project =
        *fixture.project;

    std::string messages;
    for (Janus::i32 id = 10;
         id < 13;
         ++id)
    {
        messages +=
            EncodeRequest(
                id,
                "tools/call",
                Janus::MCP::Json{
                    {"name", "scene.create_entity"},
                    {"arguments",
                     Janus::MCP::Json{
                         {"name",
                          "Agent"
                              + std::to_string(id)}}}});
    }

    std::istringstream input(
        messages);
    std::ostringstream output;

    Janus::MCP::AllowAllMcpPermissionPolicy policy;

    auto hostResult =
        Janus::Editor::McpEditorHost::Create(
            project,
            input,
            output,
            policy,
            1);

    REQUIRE(hostResult);
    auto host =
        std::move(hostResult).Value();

    REQUIRE(host->Start());
    REQUIRE(PumpUntilWorkerStops(*host));

    const auto responses =
        ParseResponses(
            output.str());

    REQUIRE(responses.size() == 3);
    REQUIRE(
        project.GetCommandBus()
            .GetHistorySize()
        == 3);
    REQUIRE(project.IsDirty());
}

TEST_CASE(
    "Live Editor MCP stop releases a pending worker without main-thread execution",
    "[editor][mcp][host][shutdown][v0.8]")
{
    ProjectFixture fixture;
    auto& project =
        *fixture.project;

    const Janus::usize beforeEntities =
        project.GetEditorScene()
            .GetEntities()
            .size();

    std::istringstream input(
        EncodeRequest(
            20,
            "tools/call",
            Janus::MCP::Json{
                {"name", "scene.create_entity"},
                {"arguments",
                 Janus::MCP::Json{
                     {"name", "NeverRuns"}}}}));
    std::ostringstream output;

    Janus::MCP::AllowAllMcpPermissionPolicy policy;

    auto hostResult =
        Janus::Editor::McpEditorHost::Create(
            project,
            input,
            output,
            policy);

    REQUIRE(hostResult);
    auto host =
        std::move(hostResult).Value();

    REQUIRE(host->Start());

    host->Stop();

    REQUIRE_FALSE(host->IsRunning());
    REQUIRE(
        project.GetEditorScene()
            .GetEntities()
            .size()
        == beforeEntities);
    REQUIRE(
        project.GetCommandBus()
            .GetHistorySize()
        == 0);
}

TEST_CASE("MCP runtime controls and logs share live Engine state", "[mcp-debug][v0.9]")
{
    using namespace Janus;
    using namespace Janus::MCP;
    ProjectFixture fixture;
    fixture.project->GetLogStore()->Append(LogLevel::Warning, "Test", "visible");
    std::string requests;
    requests += EncodeRequest(1, "tools/call",
                              {{"name", "runtime.play"}, {"arguments", {{"startPaused", true}}}});
    requests +=
        EncodeRequest(2, "tools/call", {{"name", "runtime.step"}, {"arguments", Json::object()}});
    requests += EncodeRequest(3, "resources/read", {{"uri", "engine://runtime/status"}});
    requests +=
        EncodeRequest(4, "resources/read", {{"uri", "engine://logs/recent?level=Warning&limit=1"}});
    requests +=
        EncodeRequest(5, "tools/call", {{"name", "runtime.stop"}, {"arguments", Json::object()}});
    requests +=
        EncodeRequest(6, "tools/call", {{"name", "runtime.step"}, {"arguments", Json::object()}});
    std::istringstream input(requests);
    std::ostringstream output;
    AllowAllMcpPermissionPolicy policy;
    auto created = Editor::McpEditorHost::Create(*fixture.project, input, output, policy);
    REQUIRE(created);
    auto host = std::move(created).Value();
    REQUIRE(host->Start());
    REQUIRE(PumpUntilWorkerStops(*host));
    const auto responses = ParseResponses(output.str());
    REQUIRE(responses.size() == 6);
    REQUIRE(responses[0].contains("result"));
    REQUIRE(responses[0]["result"]["structuredContent"]["ok"] == true);
    const auto status =
        Json::parse(responses[2]["result"]["contents"][0]["text"].get<std::string>());
    REQUIRE(status["state"] == "Paused");
    REQUIRE(status["frameIndex"] == 1);
    const auto logs = Json::parse(responses[3]["result"]["contents"][0]["text"].get<std::string>());
    REQUIRE(logs["entries"].size() == 1);
    REQUIRE(logs["entries"][0]["message"] == "visible");
    REQUIRE(responses[5]["result"]["isError"] == true);
    REQUIRE_FALSE(fixture.project->HasRuntime());
}

TEST_CASE("Profiler snapshots retain independent render passes", "[profiler][v0.9]")
{
    ProjectFixture fixture;
    auto& project = *fixture.project;
    REQUIRE(project.BeginDiagnosticsFrame());
    Janus::RendererStatistics stats;
    stats.spriteCount = 7;
    stats.drawCallCount = 2;
    project.CaptureRenderPass(false, stats, 9);
    stats.spriteCount = 3;
    project.CaptureRenderPass(true, stats, 4);
    project.EndDiagnosticsFrame();
    auto frame = project.GetDiagnosticsFrame();
    REQUIRE(frame);
    REQUIRE(frame->sceneView->statistics.spriteCount == 7);
    REQUIRE(frame->gameView->statistics.spriteCount == 3);
    REQUIRE(project.BeginDiagnosticsFrame());
    project.EndDiagnosticsFrame();
    frame = project.GetDiagnosticsFrame();
    REQUIRE_FALSE(frame->sceneView);
    REQUIRE_FALSE(frame->gameView);
}

TEST_CASE("Project transactions coordinate owner timeout dirty and human undo",
          "[host-transaction][v0.9]")
{
    using namespace Janus;
    ProjectFixture fixture;
    auto& project = *fixture.project;
    Editor::EditorContext context;
    context.project = &project;
    Editor::EditorActions human(context);
    const auto owner = UUID::Random();
    const auto entity = UUID::Random();
    auto tx = project.BeginAuthoringTransaction(owner);
    REQUIRE(tx);
    REQUIRE(project.ExecuteAuthoring(
        std::make_unique<CreateEntityCommand>(project.GetEditorScene(), entity, "Pending"),
        CommandActor::Agent, tx.Value(), owner));
    REQUIRE(project.IsDirty());
    REQUIRE_FALSE(human.RenameEntity(entity, "Human"));
    REQUIRE_FALSE(project.SaveCurrentScene());
    REQUIRE_FALSE(project.PlayRuntime());
    REQUIRE_FALSE(project.FinishAuthoringTransaction(tx.Value(), UUID::Random(), true));
    REQUIRE(project.GetCommandBus().HasTransaction());
    REQUIRE(project.FinishAuthoringTransaction(tx.Value(), owner, false));
    REQUIRE_FALSE(project.IsDirty());
    REQUIRE_FALSE(project.GetEditorScene().FindEntity(entity).IsValid());
    tx = project.BeginAuthoringTransaction(owner);
    REQUIRE(tx);
    REQUIRE(project.ExecuteAuthoring(
        std::make_unique<CreateEntityCommand>(project.GetEditorScene(), entity, "Committed"),
        CommandActor::Agent, tx.Value(), owner));
    REQUIRE(project.FinishAuthoringTransaction(tx.Value(), owner, true));
    REQUIRE(project.FinishAuthoringTransaction(tx.Value(), owner, true));
    REQUIRE(human.Undo());
    REQUIRE_FALSE(project.GetEditorScene().FindEntity(entity).IsValid());
    REQUIRE(human.Redo());
    tx = project.BeginAuthoringTransaction(owner);
    REQUIRE(tx);
    REQUIRE(project.ExecuteAuthoring(
        std::make_unique<RenameEntityCommand>(project.GetEditorScene(), entity, "Timeout"),
        CommandActor::Agent, tx.Value(), owner));
    project.ExpireAuthoringTransaction(std::chrono::steady_clock::now() + std::chrono::seconds(61));
    REQUIRE_FALSE(project.GetCommandBus().HasTransaction());
    REQUIRE(project.GetEditorScene()
                .GetComponent<EntityIdentityComponent>(project.GetEditorScene().FindEntity(entity))
                ->name == "Committed");
}

TEST_CASE("Editor host recovery rebinds resources and rejects foreign-thread cleanup",
          "[host-transaction][v0.9]")
{
    using namespace Janus;
    ProjectFixture fixture;
    auto& project = *fixture.project;
    const auto owner = UUID::Random();
    auto tx = project.BeginAuthoringTransaction(owner);
    REQUIRE(tx);
    MCP::AllowAllMcpPermissionPolicy policy;
    std::istringstream input(
        EncodeRequest(1, "resources/read", {{"uri", "engine://scene/current"}}));
    std::ostringstream output;
    auto host = Editor::McpEditorHost::Create(project, input, output, policy);
    REQUIRE(host);
    bool rejected = false;
    std::thread worker([&] { rejected = !host.Value()->Pump(); });
    worker.join();
    REQUIRE(rejected);
    REQUIRE(project.GetCommandBus().HasTransaction());
    REQUIRE(project.DiscardUnsavedAndReload());
    REQUIRE(project.GetSceneRevision() == 1);
    REQUIRE(host.Value()->Start());
    REQUIRE(PumpUntilWorkerStops(*host.Value()));
    const auto responses = ParseResponses(output.str());
    REQUIRE(responses.size() == 1);
    REQUIRE(responses[0].contains("result"));
}

TEST_CASE("Diagnostic resource pages stay below stdio frame limits after nested escaping",
          "[mcp-debug][v0.9]")
{
    using namespace Janus;
    ProjectFixture fixture;
    for (int i = 0; i < 200; ++i)
        fixture.project->GetLogStore()->Append(LogLevel::Info, "Bounds", std::string(8192, '\x01'));
    REQUIRE(fixture.project->BeginDiagnosticsFrame());
    for (int i = 0; i < 4096; ++i)
    {
        CpuScope scope(fixture.project->GetProfiler(), std::string(96, 'p'));
    }
    fixture.project->EndDiagnosticsFrame();
    std::istringstream input(
        EncodeRequest(1, "resources/read", {{"uri", "engine://logs/recent?after=0&limit=200"}}) +
        EncodeRequest(2, "resources/read", {{"uri", "engine://profiler/latest-frame"}}));
    std::ostringstream output;
    MCP::AllowAllMcpPermissionPolicy policy;
    auto host = Editor::McpEditorHost::Create(*fixture.project, input, output, policy);
    REQUIRE(host);
    REQUIRE(host.Value()->Start());
    REQUIRE(PumpUntilWorkerStops(*host.Value()));
    auto responses = ParseResponses(output.str());
    REQUIRE(responses.size() == 2);
    for (const auto& response : responses)
    {
        REQUIRE(response.contains("result"));
        REQUIRE(response.dump().size() < 1024 * 1024);
    }
    auto profile =
        MCP::Json::parse(responses[1]["result"]["contents"][0]["text"].get<std::string>());
    REQUIRE(profile["omittedScopes"].get<usize>() > 0);
}

TEST_CASE("Unclassified MCP denials are visible in Activity without executing", "[mcp-debug][v0.9]")
{
    ProjectFixture fixture;
    std::istringstream input(EncodeRequest(
        1, "tools/call", {{"name", "unknown.write"}, {"arguments", Janus::MCP::Json::object()}}));
    std::ostringstream output;
    Janus::MCP::AllowAllMcpPermissionPolicy policy;
    auto host = Janus::Editor::McpEditorHost::Create(*fixture.project, input, output, policy);
    REQUIRE(host);
    REQUIRE(host.Value()->Start());
    REQUIRE(PumpUntilWorkerStops(*host.Value()));
    REQUIRE(ParseResponses(output.str())[0].contains("error"));
    const auto& activity = fixture.project->GetCommandBus().GetActivity();
    REQUIRE(activity.size() == 1);
    REQUIRE(activity.back().outcome == Janus::CommandOutcome::Failed);
    REQUIRE_FALSE(fixture.project->IsDirty());
}
