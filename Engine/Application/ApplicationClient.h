#pragma once

#include "Core/Error/Result.h"
#include "Core/Event/Event.h"
#include "Core/Time/TimeStep.h"

namespace Janus
{

class Application;

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

    virtual void OnUpdate(TimeStep timeStep, Application& application) = 0;

    virtual void OnShutdown(Application&) noexcept
    {
    }
};

} // namespace Janus
