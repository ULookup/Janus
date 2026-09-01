#pragma once

#include "Platform/Graphics/GraphicsContext.h"

struct SDL_Window;
struct SDL_GLContextState;

namespace Janus
{

class SDLOpenGLContext final : public GraphicsContext
{
public:
    SDLOpenGLContext(SDL_Window* window, SDL_GLContextState* context) noexcept;
    ~SDLOpenGLContext() override;

    SDLOpenGLContext(const SDLOpenGLContext&) = delete;
    SDLOpenGLContext& operator=(const SDLOpenGLContext&) = delete;

    [[nodiscard]] Result<void> MakeCurrent() override;
    [[nodiscard]] Result<void> SetSwapInterval(i32 interval) override;
    void Present() noexcept override;

private:
    SDL_Window* m_Window = nullptr;
    SDL_GLContextState* m_Context = nullptr;
};

} // namespace Janus
