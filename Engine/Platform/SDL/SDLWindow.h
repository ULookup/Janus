#pragma once

#include "Platform/Window/Window.h"

struct SDL_Window;

namespace Janus
{

    class SDLWindow final : public Window
    {
    public:
        SDLWindow(SDL_Window* window, u32 width, u32 height) noexcept;

        ~SDLWindow() override;

        SDLWindow(const SDLWindow&) = delete;
        SDLWindow& operator=(const SDLWindow&) = delete;

        SDLWindow(SDLWindow&&) = delete;
        SDLWindow& operator=(SDLWindow&&) = delete;

        // --------------------------------------------------------
        // Event Pump
        // --------------------------------------------------------

        void PollEvents() override;

        // --------------------------------------------------------
        // Window State
        // --------------------------------------------------------

        void SetTitle(std::string_view title) override;

        [[nodiscard]]
        u32 GetWidth() const noexcept override;

        [[nodiscard]]
        u32 GetHeight() const noexcept override;

        [[nodiscard]]
        bool ShouldClose() const noexcept override;

        void RequestClose() noexcept override;

        // --------------------------------------------------------
        // Native Handle
        // --------------------------------------------------------

        [[nodiscard]]
        void* GetNativeHandle() const noexcept override;

    private:
        SDL_Window* m_Window = nullptr;

        u32 m_Width = 0;
        u32 m_Height = 0;

        bool m_ShouldClose = false;
    };

} // namespace Janus