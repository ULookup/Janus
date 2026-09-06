#include "Log.h"
#include "Core/Log/LogStore.h"

#include <cstdio>

#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace Janus
{
namespace
{
class StoreSink final : public spdlog::sinks::base_sink<std::mutex>
{
  public:
    explicit StoreSink(std::shared_ptr<LogStore> store) : m_Store(std::move(store)) {}

  private:
    void sink_it_(const spdlog::details::log_msg& message) override
    {
        const auto level = message.level >= spdlog::level::err    ? LogLevel::Error
                           : message.level == spdlog::level::warn ? LogLevel::Warning
                                                                  : LogLevel::Info;
        m_Store->Append(level, std::string(message.logger_name.data(), message.logger_name.size()),
                        std::string(message.payload.data(), message.payload.size()));
    }
    void flush_() override {}
    std::shared_ptr<LogStore> m_Store;
};
} // namespace

    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
    std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

    void Log::Initialize(LogOutput output, std::shared_ptr<LogStore> store)
    {
        if (s_CoreLogger || s_ClientLogger)
        {
            return;
        }

        try
        {
            spdlog::set_pattern(
                "[%T] [%n] [%^%l%$] %v");

            if (output == LogOutput::StandardError)
            {
                s_CoreLogger =
                    spdlog::stderr_color_mt("Core");
                s_ClientLogger =
                    spdlog::stderr_color_mt("Sandbox");
            }
            else
            {
                s_CoreLogger =
                    spdlog::stdout_color_mt("Core");
                s_ClientLogger =
                    spdlog::stdout_color_mt("Sandbox");
            }

            if (store)
            {
                // Attach before publishing loggers to worker threads; shutdown owns detachment.
                auto sink = std::make_shared<StoreSink>(std::move(store));
                s_CoreLogger->sinks().push_back(sink);
                s_ClientLogger->sinks().push_back(std::move(sink));
            }
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
