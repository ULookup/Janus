#include "Platform/Platform.h"

#include "Core/Log/Log.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>

#include <string>

namespace Janus::Platform
{

    namespace
    {

        bool g_Initialized = false;

    } // namespace


    Result<void> Initialize()
    {
        if (g_Initialized)
        {
            JANUS_CORE_WARN("Platform is already initialized.");

            return Result<void>::Success();
        }

        const SDL_InitFlags flags = SDL_INIT_VIDEO | SDL_INIT_EVENTS;

        if (!SDL_Init(flags))
        {
            const std::string errorMessage = std::string("Failed to initialize SDL3: ") + SDL_GetError();

            return Result<void>::Failure(ErrorCode::PlatformInitFailed, errorMessage);
        }

        g_Initialized = true;

        JANUS_CORE_INFO("Platform initialized successfully.");

        return Result<void>::Success();
    }


    void Shutdown() noexcept
    {
        if (!g_Initialized)
        {
            return;
        }

        SDL_Quit();

        g_Initialized = false;

        JANUS_CORE_INFO("Platform shut down successfully.");
    }

} // namespace Janus::Platform