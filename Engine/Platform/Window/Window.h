#pragma once

#include "Core/Error/Result.h"
#include "Core/Event/Event.h"
#include "Core/Types.h"
#include "Platform/Window/WindowConfig.h"

#include <functional>
#include <memory>
#include <string_view>

namespace Janus
{

    class Window
    {
    public:
        using EventCallback = std::function<void(const Event&)>;
        using NativeEventCallback = std::function<void(const void*)>;

        Window() = default;

        virtual ~Window() = default;

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        Window(Window&&) = delete;
        Window& operator=(Window&&) = delete;

        // --------------------------------------------------------
        // Factory
        // --------------------------------------------------------

        [[nodiscard]]
        static Result<std::unique_ptr<Window>> Create(const WindowConfig& config);

        // --------------------------------------------------------
        // Event Pump
        // --------------------------------------------------------

        virtual void PollEvents(const EventCallback& callback) = 0;

        // Optional backend event tap for integration layers such as Dear ImGui.
        // The pointer is only valid for the duration of the callback.
        virtual void SetNativeEventCallback(NativeEventCallback callback)
        {
            (void)callback;
        }

        // --------------------------------------------------------
        // Window State
        // --------------------------------------------------------

        virtual void SetTitle(std::string_view title) = 0;

        [[nodiscard]]
        virtual u32 GetWidth() const noexcept = 0;

        [[nodiscard]]
        virtual u32 GetHeight() const noexcept = 0;

        [[nodiscard]]
        virtual bool ShouldClose() const noexcept = 0;

        virtual void RequestClose() noexcept = 0;

        // --------------------------------------------------------
        // Backend Escape Hatch
        // --------------------------------------------------------

        [[nodiscard]]
        virtual void* GetNativeHandle() const noexcept = 0;
    };

} // namespace Janus
