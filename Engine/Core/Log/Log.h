#pragma once

#include "Core/Log/LogOutput.h"

#include <memory>

#include <spdlog/logger.h>

namespace Janus
{

    class Log final
    {
    public:
        Log() = delete;

        static void Initialize(
            LogOutput output = LogOutput::StandardOutput);
        static void Shutdown();

        [[nodiscard]]
        static std::shared_ptr<spdlog::logger>& GetCoreLogger();

        [[nodiscard]]
        static std::shared_ptr<spdlog::logger>& GetClientLogger();

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };

} // namespace Janus


// ============================================================
// Logging macros
// ============================================================

// A single statement-safe guard keeps every public macro null-safe without
// duplicating the lifetime check, so logging remains a no-op before
// initialization or after shutdown.
#define JANUS_DETAIL_LOG(LoggerGetter, Level, ...)                       \
    do                                                                   \
    {                                                                    \
        if (const auto& janusDetailLogger = (LoggerGetter))              \
        {                                                                \
            janusDetailLogger->Level(__VA_ARGS__);                       \
        }                                                                \
    } while (false)

#define JANUS_CORE_TRACE(...) \
    JANUS_DETAIL_LOG(::Janus::Log::GetCoreLogger(), trace, __VA_ARGS__)

#define JANUS_CORE_INFO(...) \
    JANUS_DETAIL_LOG(::Janus::Log::GetCoreLogger(), info, __VA_ARGS__)

#define JANUS_CORE_WARN(...) \
    JANUS_DETAIL_LOG(::Janus::Log::GetCoreLogger(), warn, __VA_ARGS__)

#define JANUS_CORE_ERROR(...) \
    JANUS_DETAIL_LOG(::Janus::Log::GetCoreLogger(), error, __VA_ARGS__)

#define JANUS_CORE_CRITICAL(...) \
    JANUS_DETAIL_LOG(::Janus::Log::GetCoreLogger(), critical, __VA_ARGS__)

#define JANUS_TRACE(...) \
    JANUS_DETAIL_LOG(::Janus::Log::GetClientLogger(), trace, __VA_ARGS__)

#define JANUS_INFO(...) \
    JANUS_DETAIL_LOG(::Janus::Log::GetClientLogger(), info, __VA_ARGS__)

#define JANUS_WARN(...) \
    JANUS_DETAIL_LOG(::Janus::Log::GetClientLogger(), warn, __VA_ARGS__)

#define JANUS_ERROR(...) \
    JANUS_DETAIL_LOG(::Janus::Log::GetClientLogger(), error, __VA_ARGS__)

#define JANUS_CRITICAL(...) \
    JANUS_DETAIL_LOG(::Janus::Log::GetClientLogger(), critical, __VA_ARGS__)
