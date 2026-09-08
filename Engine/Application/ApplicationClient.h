#pragma once

#include "Core/Error/Result.h"
#include "Core/Event/Event.h"
#include "Core/Time/TimeStep.h"

namespace Janus
{

class Application;

enum class CloseDecision
{
    Accept,
    Defer
};

class ApplicationClient
{
public:
    virtual ~ApplicationClient() = default;

    [[nodiscard]]
    virtual Result<void> OnInitialize(Application&)
    {
        return Result<void>::Success();
    }

    virtual void OnEvent(const Event&, Application&)
    {
    }

    // Interactive clients may defer a native close and later call RequestExit after confirmation.
    virtual CloseDecision OnCloseRequested(Application&)
    {
        return CloseDecision::Accept;
    }

    virtual void OnUpdate(TimeStep timeStep, Application& application) = 0;

    virtual void OnShutdown(Application&) noexcept
    {
    }
};

} // namespace Janus
