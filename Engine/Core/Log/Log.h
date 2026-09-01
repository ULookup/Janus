#pragma once

#include <memory>

#include <spdlog/logger.h>

namespace Janus
{

    class Log final
    {
    public:
        Log() = delete;

        static void Initialize();
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
// Core Logger
// ============================================================

#define JANUS_CORE_TRACE(...)    \
    ::Janus::Log::GetCoreLogger()->trace(__VA_ARGS__)

#define JANUS_CORE_INFO(...)     \
    ::Janus::Log::GetCoreLogger()->info(__VA_ARGS__)

#define JANUS_CORE_WARN(...)     \
    ::Janus::Log::GetCoreLogger()->warn(__VA_ARGS__)

#define JANUS_CORE_ERROR(...)    \
    ::Janus::Log::GetCoreLogger()->error(__VA_ARGS__)

#define JANUS_CORE_CRITICAL(...) \
    ::Janus::Log::GetCoreLogger()->critical(__VA_ARGS__)


// ============================================================
// Client Logger
// ============================================================

#define JANUS_TRACE(...)    \
    ::Janus::Log::GetClientLogger()->trace(__VA_ARGS__)

#define JANUS_INFO(...)     \
    ::Janus::Log::GetClientLogger()->info(__VA_ARGS__)

#define JANUS_WARN(...)     \
    ::Janus::Log::GetClientLogger()->warn(__VA_ARGS__)

#define JANUS_ERROR(...)    \
    ::Janus::Log::GetClientLogger()->error(__VA_ARGS__)

#define JANUS_CRITICAL(...) \
    ::Janus::Log::GetClientLogger()->critical(__VA_ARGS__)