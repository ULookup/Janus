#include "Log.h"

#include <cstdio>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace Janus
{

    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
    std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

    void Log::Initialize()
    {
        if (s_CoreLogger || s_ClientLogger)
        {
            return;
        }

        try
        {
            spdlog::set_pattern(
                "[%T] [%n] [%^%l%$] %v");

            s_CoreLogger =
                spdlog::stdout_color_mt("Core");

            s_ClientLogger =
                spdlog::stdout_color_mt("Sandbox");

#if defined(JANUS_DEBUG)

            s_CoreLogger->set_level(
                spdlog::level::trace);

            s_ClientLogger->set_level(
                spdlog::level::trace);

#else

            s_CoreLogger->set_level(
                spdlog::level::info);

            s_ClientLogger->set_level(
                spdlog::level::info);

#endif

            s_CoreLogger->flush_on(
                spdlog::level::warn);

            s_ClientLogger->flush_on(
                spdlog::level::warn);

            s_CoreLogger->info(
                "Janus logging initialized.");
        }
        catch (const spdlog::spdlog_ex& exception)
        {
            std::fprintf(
                stderr,
                "Failed to initialize Janus logging: %s\n",
                exception.what());
        }
    }

    void Log::Shutdown()
    {
        if (s_CoreLogger)
        {
            s_CoreLogger->info(
                "Janus logging shutting down.");

            s_CoreLogger->flush();
        }

        if (s_ClientLogger)
        {
            s_ClientLogger->flush();
        }

        s_CoreLogger.reset();
        s_ClientLogger.reset();

        spdlog::drop("Core");
        spdlog::drop("Sandbox");
    }

    std::shared_ptr<spdlog::logger>&
        Log::GetCoreLogger()
    {
        return s_CoreLogger;
    }

    std::shared_ptr<spdlog::logger>&
        Log::GetClientLogger()
    {
        return s_ClientLogger;
    }

} // namespace Janus