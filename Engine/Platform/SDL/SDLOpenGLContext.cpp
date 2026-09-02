#include "Platform/SDL/SDLOpenGLContext.h"

#include "Core/Log/Log.h"
#include "Platform/Window/Window.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_video.h>

#include <memory>
#include <string>

namespace Janus
{

    Result<std::unique_ptr<GraphicsContext>>
        GraphicsContext::Create(Window& window)
    {
        SDL_Window* nativeWindow =
            static_cast<SDL_Window*>(window.GetNativeHandle());

        if (nativeWindow == nullptr)
        {
            return Result<std::unique_ptr<GraphicsContext>>::Failure(
                ErrorCode::GraphicsContextCreateFailed,
                "Cannot create an OpenGL context without a native window.");
        }

        SDL_GLContext nativeContext =
            SDL_GL_CreateContext(nativeWindow);

        if (nativeContext == nullptr)
        {
            return Result<std::unique_ptr<GraphicsContext>>::Failure(
                ErrorCode::GraphicsContextCreateFailed,
                std::string("Failed to create OpenGL context: ")
                + SDL_GetError());
        }

        auto context =
            std::make_unique<SDLOpenGLContext>(
                nativeWindow,
                nativeContext);

        return Result<std::unique_ptr<GraphicsContext>>::Success(
            std::move(context));
    }

    SDLOpenGLContext::SDLOpenGLContext(
        SDL_Window* window,
        SDL_GLContextState* context) noexcept
        : m_Window(window),
        m_Context(context)
    {
    }

    SDLOpenGLContext::~SDLOpenGLContext()
    {
        if (m_Context != nullptr)
        {
            // The GL context references the native window, so it must be
            // destroyed before the SDL window that owns the drawable surface.
            SDL_GL_DestroyContext(m_Context);
            m_Context = nullptr;
        }
    }

    Result<void> SDLOpenGLContext::MakeCurrent()
    {
        if (!SDL_GL_MakeCurrent(m_Window, m_Context))
        {
            return Result<void>::Failure(
                ErrorCode::GraphicsContextMakeCurrentFailed,
                std::string("Failed to make OpenGL context current: ")
                + SDL_GetError());
        }

        return Result<void>::Success();
    }

    Result<void> SDLOpenGLContext::SetSwapInterval(i32 interval)
    {
        if (!SDL_GL_SetSwapInterval(interval))
        {
            return Result<void>::Failure(
                ErrorCode::SwapIntervalFailed,
                std::string("Failed to set OpenGL swap interval: ")
                + SDL_GetError());
        }

        return Result<void>::Success();
    }

    void SDLOpenGLContext::Present() noexcept
    {
        if (!SDL_GL_SwapWindow(m_Window))
        {
            JANUS_CORE_ERROR("Failed to swap OpenGL window: {}", SDL_GetError());
        }
    }

} // namespace Janus
