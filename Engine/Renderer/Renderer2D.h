#pragma once

#include "Core/Error/Result.h"
#include "Renderer/OrthographicCamera.h"
#include "Renderer/RendererStatistics.h"
#include "Renderer/RendererTypes.h"
#include "Renderer/Sprite.h"

#include <memory>

namespace Janus
{

class RenderDevice;

namespace Detail
{
struct Renderer2DTestAccess;
}

class Renderer2D
{
public:
    [[nodiscard]] static Result<std::unique_ptr<Renderer2D>> Create();

    Renderer2D(const Renderer2D&) = delete;
    Renderer2D& operator=(const Renderer2D&) = delete;

    ~Renderer2D();

    void SetViewport(Viewport viewport);
    void BeginFrame(const OrthographicCamera& camera);
    void SubmitSprite(const Sprite& sprite);

    [[nodiscard]] Result<void> EndFrame();

    [[nodiscard]] const RendererStatistics& GetStatistics() const noexcept;

    [[nodiscard]] Result<TextureHandle> CreateTexture(
        const TextureDesc& desc);

    void DestroyTexture(TextureHandle handle);

private:
    explicit Renderer2D(std::unique_ptr<RenderDevice> device);
    explicit Renderer2D(RenderDevice& device);

    class Impl;
    std::unique_ptr<Impl> m_Impl;

    friend struct Detail::Renderer2DTestAccess;
};

namespace Detail
{

struct Renderer2DTestAccess
{
    static std::unique_ptr<Renderer2D> Create(RenderDevice& device)
    {
        return std::unique_ptr<Renderer2D>(
            new Renderer2D(device));
    }
};

} // namespace Detail

} // namespace Janus
