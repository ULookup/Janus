#pragma once
#include "Core/Types.h"
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>
namespace Janus
{
struct CpuScopeSample
{
    std::string name;
    i32 parent = -1;
    f64 startMilliseconds = 0, durationMilliseconds = 0;
};
struct CpuFrame
{
    u64 frameId = 0;
    f64 durationMilliseconds = 0;
    usize droppedScopes = 0;
    std::vector<CpuScopeSample> scopes;
};
// Owner-thread recorder. Snapshots are value copies and never expose an in-progress frame.
class CpuProfiler final
{
  public:
    using Clock = std::function<f64()>;
    explicit CpuProfiler(Clock clock = {}, usize frames = 120, usize scopes = 4096);
    bool BeginFrame();
    bool EndFrame();
    void SetEnabled(bool enabled) noexcept;
    bool IsEnabled() const noexcept
    {
        return m_Enabled;
    }
    bool IsRecording() const noexcept
    {
        return m_Active && std::this_thread::get_id() == m_Owner;
    }
    std::optional<CpuFrame> Latest() const;
    std::optional<CpuFrame> Find(u64 id) const;

  private:
    friend class CpuScope;
    i32 BeginScope(std::string_view name);
    void EndScope(i32 index, u64 frame);
    Clock m_Clock;
    std::thread::id m_Owner = std::this_thread::get_id();
    usize m_MaxFrames, m_MaxScopes;
    bool m_Enabled = true, m_Active = false;
    u64 m_NextFrame = 1;
    f64 m_Start = 0;
    CpuFrame m_Current;
    std::vector<i32> m_Stack;
    std::deque<CpuFrame> m_Frames;
};
class CpuScope final
{
  public:
    CpuScope(CpuProfiler& profiler, std::string_view name);
    ~CpuScope();
    CpuScope(const CpuScope&) = delete;
    CpuScope& operator=(const CpuScope&) = delete;

  private:
    CpuProfiler& m_Profiler;
    i32 m_Index;
    u64 m_Frame;
};
} // namespace Janus
