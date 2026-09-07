#include "../Renderer/FakeRenderDevice.h"
#include "ProjectSession.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"
#include "Scene/SceneRenderer.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>

// Fixed work, no timing assertion: Debug/FakeRenderDevice is a CPU regression reference only.
int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: JanusGameSystemsBenchmark <Game directory>\n";
        return 2;
    }
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::ProjectRuntimeConfig config;
    config.root = argv[1];
    config.startupScenePath = "Scenes/Integrated.scene";
    auto opened = Janus::Editor::ProjectSession::Open(config, *renderer);
    if (!opened)
    {
        std::cerr << opened.GetError().message;
        return 2;
    }
    auto project = std::move(opened).Value();
    Janus::InputState input;
    auto started = project->StartRuntime(input);
    if (!started)
    {
        std::cerr << started.GetError().message;
        return 2;
    }
    Janus::SceneRenderer sceneRenderer;
    std::map<std::string, std::vector<double>> samples;
    Janus::u32 maxDraws = 0, maxSprites = 0, maxBinds = 0;
    constexpr int warmup = 180, measured = 720;
    for (int frame = 0; frame < warmup + measured; ++frame)
    {
        input.BeginFrame();
        const int phase = frame % 180;
        const std::map<int, Janus::KeyCode> keys = {
            {0, Janus::KeyCode::Digit1},   {30, Janus::KeyCode::Digit2},
            {31, Janus::KeyCode::Digit4},  {75, Janus::KeyCode::Digit2},
            {76, Janus::KeyCode::Digit4},  {120, Janus::KeyCode::Digit2},
            {121, Janus::KeyCode::Digit4}, {179, Janus::KeyCode::Digit0}};
        if (keys.contains(phase))
        {
            input.Apply(Janus::KeyPressedEvent{keys.at(phase), false});
            input.Apply(Janus::KeyReleasedEvent{keys.at(phase)});
        }
        project->BeginDiagnosticsFrame();
        auto updated = project->UpdateRuntime(Janus::TimeStep::FromSeconds(1.0 / 60.0));
        if (!updated)
        {
            std::cerr << updated.GetError().message;
            return 2;
        }
        auto* runtime = project->GetRuntimeSession();
        {
            Janus::CpuScope render(project->GetProfiler(), "Runtime.Render");
            auto rendered = sceneRenderer.Render(runtime->GetScene(), project->GetAssetService(),
                                                 *renderer, {1280, 720}, {1280, 720},
                                                 &runtime->GetUIState(), &runtime->GetAnimations());
            if (!rendered)
            {
                std::cerr << rendered.GetError().message;
                return 2;
            }
        }
        project->EndDiagnosticsFrame();
        if (frame >= warmup)
        {
            const auto profile = project->GetProfiler().Latest();
            samples["Frame"].push_back(profile->durationMilliseconds);
            for (const auto& scope : profile->scopes)
                samples[scope.name].push_back(scope.durationMilliseconds);
            const auto& stats = renderer->GetStatistics();
            maxDraws = std::max(maxDraws, stats.drawCallCount);
            maxSprites = std::max(maxSprites, stats.spriteCount);
            maxBinds = std::max(maxBinds, stats.textureBindCount);
        }
        // The fake backend records uploads for assertions; do not accumulate them across frames.
        device.drawCommands.clear();
        device.vertexUploads.clear();
        device.drawProjections.clear();
        device.viewProjections.clear();
        device.viewportHistory.clear();
        device.boundFramebuffers.clear();
    }
    nlohmann::json output = {{"warmupFrames", warmup},
                             {"measuredFrames", measured},
                             {"timestepSeconds", 1.0 / 60.0},
                             {"backend", "FakeRenderDevice / CPU only"},
                             {"entities", project->GetEditorScene().GetEntities().size()},
                             {"maxDrawCalls", maxDraws},
                             {"maxSprites", maxSprites},
                             {"maxTextureBinds", maxBinds},
                             {"frameBudgetMilliseconds", 1000.0 / 60.0}};
    for (auto& [name, values] : samples)
    {
        std::sort(values.begin(), values.end());
        output["cpuMilliseconds"][name] = {{"median", values[values.size() / 2]},
                                           {"p95", values[(values.size() * 95 + 99) / 100 - 1]},
                                           {"maximum", values.back()}};
    }
    auto stopped = project->StopRuntime();
    if (!stopped)
    {
        std::cerr << stopped.GetError().message;
        return 2;
    }
    std::cout << output.dump(2) << '\n';
}
