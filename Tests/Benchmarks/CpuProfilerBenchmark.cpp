#include "Core/Profiling/CpuProfiler.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
int main()
{
    constexpr int frames = 10000, scopes = 20;
    std::array<double, 2> median{};
    for (int enabled = 0; enabled < 2; ++enabled)
    {
        std::array<double, 7> samples{};
        for (auto& sample : samples)
        {
            Janus::CpuProfiler profiler;
            profiler.SetEnabled(enabled != 0);
            const auto start = std::chrono::steady_clock::now();
            for (int frame = 0; frame < frames; ++frame)
            {
                const bool recording = profiler.BeginFrame();
                for (int scope = 0; scope < scopes; ++scope)
                {
                    Janus::CpuScope region(profiler, "Benchmark.Scope");
                }
                if (recording && !profiler.EndFrame())
                    return 1;
            }
            sample =
                std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start)
                    .count() /
                frames;
        }
        std::sort(samples.begin(), samples.end());
        median[enabled] = samples[3];
        std::printf("enabled=%d frames=%d scopes=%d median_us_per_frame=%.3f min=%.3f max=%.3f\n",
                    enabled, frames, scopes, samples[3], samples[0], samples[6]);
    }
    std::printf("incremental_us_per_frame=%.3f\n", median[1] - median[0]);
}
