#include "Core/Profiling/CpuProfiler.h"
#include <algorithm>
#include <chrono>
namespace Janus
{
CpuProfiler::CpuProfiler(Clock clock, usize frames, usize scopes)
    : m_Clock(std::move(clock)), m_MaxFrames(std::clamp<usize>(frames, 1, 120)),
      m_MaxScopes(std::clamp<usize>(scopes, 1, 4096))
{
    if (!m_Clock)
        m_Clock = []
        {
            return std::chrono::duration<f64, std::milli>(
                       std::chrono::steady_clock::now().time_since_epoch())
                .count();
        };
}
bool CpuProfiler::BeginFrame()
{
    if (std::this_thread::get_id() != m_Owner || m_Active || !m_Enabled)
        return false;
    m_Current = {};
    m_Current.frameId = m_NextFrame++;
    m_Start = m_Clock();
    m_Active = true;
    return true;
}
bool CpuProfiler::EndFrame()
{
    if (!IsRecording() || !m_Stack.empty())
        return false;
    m_Current.durationMilliseconds = std::max(0.0, m_Clock() - m_Start);
    if (m_Frames.size() == m_MaxFrames)
        m_Frames.pop_front();
    m_Frames.push_back(std::move(m_Current));
    m_Active = false;
    return true;
}
void CpuProfiler::SetEnabled(bool enabled) noexcept
{
    if (std::this_thread::get_id() == m_Owner)
        m_Enabled = enabled;
}
i32 CpuProfiler::BeginScope(std::string_view name)
{
    if (!IsRecording())
        return -1;
    if (m_Current.scopes.size() == m_MaxScopes)
    {
        ++m_Current.droppedScopes;
        return -1;
    }
    const auto index = static_cast<i32>(m_Current.scopes.size());
    m_Current.scopes.push_back({std::string(name.substr(0, 96)),
                                m_Stack.empty() ? -1 : m_Stack.back(), m_Clock() - m_Start, 0});
    m_Stack.push_back(index);
    return index;
}
void CpuProfiler::EndScope(i32 index, u64 frame)
{
    if (index < 0 || !IsRecording() || frame != m_Current.frameId || m_Stack.empty() ||
        m_Stack.back() != index)
        return;
    auto& sample = m_Current.scopes[static_cast<usize>(index)];
    sample.durationMilliseconds = std::max(0.0, m_Clock() - m_Start - sample.startMilliseconds);
    m_Stack.pop_back();
}
std::optional<CpuFrame> CpuProfiler::Latest() const
{
    if (std::this_thread::get_id() != m_Owner || m_Frames.empty())
        return {};
    return m_Frames.back();
}
std::optional<CpuFrame> CpuProfiler::Find(u64 id) const
{
    if (std::this_thread::get_id() != m_Owner)
        return {};
    for (const auto& frame : m_Frames)
        if (frame.frameId == id)
            return frame;
    return {};
}
CpuScope::CpuScope(CpuProfiler& profiler, std::string_view name)
    : m_Profiler(profiler), m_Index(profiler.BeginScope(name)),
      m_Frame(m_Index < 0 ? 0 : profiler.m_Current.frameId)
{
}
CpuScope::~CpuScope()
{
    m_Profiler.EndScope(m_Index, m_Frame);
}
} // namespace Janus
