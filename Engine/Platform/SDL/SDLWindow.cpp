#include "Platform/SDL/SDLWindow.h"

#include "Core/Log/Log.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace Janus
{

    namespace
    {

        // SDL scancodes must terminate here so public window events remain platform-neutral.
        [[nodiscard]] std::optional<KeyCode> TranslateKey(SDL_Scancode key) noexcept
        {
            switch (key)
            {
            case SDL_SCANCODE_ESCAPE: return KeyCode::Escape;
            case SDL_SCANCODE_SPACE: return KeyCode::Space;
            case SDL_SCANCODE_RETURN: return KeyCode::Enter;
            case SDL_SCANCODE_UP: return KeyCode::ArrowUp;
            case SDL_SCANCODE_DOWN: return KeyCode::ArrowDown;
            case SDL_SCANCODE_LEFT: return KeyCode::ArrowLeft;
            case SDL_SCANCODE_RIGHT: return KeyCode::ArrowRight;
            case SDL_SCANCODE_W: return KeyCode::W;
            case SDL_SCANCODE_A: return KeyCode::A;
            case SDL_SCANCODE_S: return KeyCode::S;
            case SDL_SCANCODE_D: return KeyCode::D;
            case SDL_SCANCODE_0: return KeyCode::Digit0;
            case SDL_SCANCODE_1: return KeyCode::Digit1;
            case SDL_SCANCODE_2: return KeyCode::Digit2;
            case SDL_SCANCODE_3: return KeyCode::Digit3;
            case SDL_SCANCODE_4: return KeyCode::Digit4;
            case SDL_SCANCODE_5: return KeyCode::Digit5;
            case SDL_SCANCODE_6: return KeyCode::Digit6;
            case SDL_SCANCODE_7: return KeyCode::Digit7;
            case SDL_SCANCODE_8: return KeyCode::Digit8;
            case SDL_SCANCODE_9: return KeyCode::Digit9;
            default: return std::nullopt;
            }
        }

    } // namespace

    // ============================================================
    // Window Factory
    // ============================================================

    Result<std::unique_ptr<Window>>
        Window::Create(const WindowConfig& config)
    {
        const auto validationResult = ValidateWindowConfig(config);

        if (!validationResult)
        {
            return Result<std::unique_ptr<Window>>::Failure(
                validationResult.GetError());
        }

        if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4) ||
            !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5) ||
            !SDL_GL_SetAttribute(
                SDL_GL_CONTEXT_PROFILE_MASK,
                SDL_GL_CONTEXT_PROFILE_CORE) ||
            !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1))
        {
            return Result<std::unique_ptr<Window>>::Failure(
                ErrorCode::WindowCreateFailed,
                std::string("Failed to configure OpenGL window: ")
                + SDL_GetError());
        }

#if defined(JANUS_DEBUG)
        if (!SDL_GL_SetAttribute(
                SDL_GL_CONTEXT_FLAGS,
                SDL_GL_CONTEXT_DEBUG_FLAG))
        {
            return Result<std::unique_ptr<Window>>::Failure(
                ErrorCode::WindowCreateFailed,
                std::string("Failed to request an OpenGL debug context: ")
                + SDL_GetError());
        }
#endif

        SDL_WindowFlags flags = SDL_WINDOW_OPENGL;

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

    void SDLWindow::PollEvents(const EventCallback& callback)
    {
        SDL_Event event{};

        while (SDL_PollEvent(&event))
        {
            if (m_NativeEventCallback)
            {
                m_NativeEventCallback(&event);
            }

            switch (event.type)
            {
            case SDL_EVENT_QUIT:
            {
                RequestClose();
                callback(WindowCloseEvent{});
                break;
            }

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            {
                SDL_Window* eventWindow =
                    SDL_GetWindowFromEvent(&event);

                if (eventWindow == m_Window)
                {
                    RequestClose();
                    callback(WindowCloseEvent{});
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

                    callback(WindowResizeEvent{m_Width, m_Height});
                }

                break;
            }

            case SDL_EVENT_KEY_DOWN:
            {
                SDL_Window* eventWindow =
                    SDL_GetWindowFromEvent(&event);

                if (eventWindow == m_Window)
                {
                    const auto key = TranslateKey(event.key.scancode);

                    if (key.has_value())
                    {
                        callback(KeyPressedEvent{*key, event.key.repeat});
                    }
                }

                break;
            }

            case SDL_EVENT_KEY_UP:
            {
                SDL_Window* eventWindow =
                    SDL_GetWindowFromEvent(&event);

                if (eventWindow == m_Window)
                {
                    const auto key = TranslateKey(event.key.scancode);

                    if (key.has_value())
                    {
                        callback(KeyReleasedEvent{*key});
                    }
                }

                break;
            }

            default:
                break;
            }
        }
    }


    void SDLWindow::SetNativeEventCallback(
        NativeEventCallback callback)
    {
        m_NativeEventCallback = std::move(callback);
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
