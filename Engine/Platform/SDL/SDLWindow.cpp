#include "Platform/SDL/SDLWindow.h"

#include "Core/Log/Log.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include <limits>
#include <memory>
#include <string>

namespace Janus
{

    // ============================================================
    // Window Factory
    // ============================================================

    Result<std::unique_ptr<Window>>
        Window::Create(const WindowConfig& config)
    {
        if (config.width == 0 || config.height == 0)
        {
            return Result<std::unique_ptr<Window>>::Failure(
                ErrorCode::InvalidArgument,
                "Window width and height must be greater than zero.");
        }

        constexpr auto maxSDLDimension =
            static_cast<u32>(std::numeric_limits<int>::max());

        if (config.width > maxSDLDimension ||
            config.height > maxSDLDimension)
        {
            return Result<std::unique_ptr<Window>>::Failure(
                ErrorCode::InvalidArgument,
                "Window dimensions exceed SDL supported integer range.");
        }

        SDL_WindowFlags flags = 0;

        if (config.resizable)
        {
            flags |= SDL_WINDOW_RESIZABLE;
        }

        SDL_Window* nativeWindow =
            SDL_CreateWindow(
                config.title.c_str(),
                static_cast<int>(config.width),
                static_cast<int>(config.height),
                flags);

        if (nativeWindow == nullptr)
        {
            return Result<std::unique_ptr<Window>>::Failure(
                ErrorCode::WindowCreateFailed,
                std::string("Failed to create SDL window: ")
                + SDL_GetError());
        }

        int actualWidth = static_cast<int>(config.width);
        int actualHeight = static_cast<int>(config.height);

        if (!SDL_GetWindowSize(nativeWindow, &actualWidth, &actualHeight))
        {
            JANUS_CORE_WARN("Failed to query initial window size: {}", SDL_GetError());

            actualWidth =
                static_cast<int>(config.width);

            actualHeight =
                static_cast<int>(config.height);
        }

        auto window =
            std::make_unique<SDLWindow>(
                nativeWindow,
                static_cast<u32>(actualWidth),
                static_cast<u32>(actualHeight));

        JANUS_CORE_INFO("Window created: '{}' ({}x{}).", config.title, actualWidth, actualHeight);

        return Result<std::unique_ptr<Window>>::Success(
            std::move(window));
    }


    // ============================================================
    // SDLWindow
    // ============================================================

    SDLWindow::SDLWindow(
        SDL_Window* window,
        u32 width,
        u32 height) noexcept
        : m_Window(window),
        m_Width(width),
        m_Height(height)
    {
    }


    SDLWindow::~SDLWindow()
    {
        if (m_Window != nullptr)
        {
            SDL_DestroyWindow(m_Window);
            m_Window = nullptr;

            JANUS_CORE_INFO(
                "Window destroyed.");
        }
    }


    // ============================================================
    // Events
    // ============================================================

    void SDLWindow::PollEvents()
    {
        SDL_Event event{};

        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
            {
                RequestClose();
                break;
            }

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            {
                SDL_Window* eventWindow =
                    SDL_GetWindowFromEvent(&event);

                if (eventWindow == m_Window)
                {
                    RequestClose();
                }

                break;
            }

            case SDL_EVENT_WINDOW_RESIZED:
            {
                SDL_Window* eventWindow =
                    SDL_GetWindowFromEvent(&event);

                if (eventWindow == m_Window)
                {
                    m_Width =
                        static_cast<u32>(event.window.data1);

                    m_Height =
                        static_cast<u32>(event.window.data2);
                }

                break;
            }

            default:
                break;
            }
        }
    }


    // ============================================================
    // Window State
    // ============================================================

    void SDLWindow::SetTitle(
        std::string_view title)
    {
        const std::string ownedTitle(title);

        if (!SDL_SetWindowTitle(
            m_Window,
            ownedTitle.c_str()))
        {
            JANUS_CORE_WARN(
                "Failed to set window title: {}",
                SDL_GetError());
        }
    }


    u32 SDLWindow::GetWidth() const noexcept
    {
        return m_Width;
    }


    u32 SDLWindow::GetHeight() const noexcept
    {
        return m_Height;
    }


    bool SDLWindow::ShouldClose() const noexcept
    {
        return m_ShouldClose;
    }


    void SDLWindow::RequestClose() noexcept
    {
        m_ShouldClose = true;
    }


    void* SDLWindow::GetNativeHandle() const noexcept
    {
        return m_Window;
    }

} // namespace Janus