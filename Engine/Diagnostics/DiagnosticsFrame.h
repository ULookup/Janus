#pragma once
#include "Core/Profiling/CpuProfiler.h"
#include "Core/UUID/UUID.h"
#include "Renderer/RendererStatistics.h"
namespace Janus
{
struct RenderPassSnapshot
{
    RendererStatistics statistics;
    usize entityCount = 0;
};
struct DiagnosticsFrame
{
    CpuFrame cpu;
    UUID runtimeId;
    u64 simulationFrame = 0;
    std::optional<RenderPassSnapshot> sceneView, gameView;
};
} // namespace Janus
