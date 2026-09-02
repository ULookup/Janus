#include "Renderer/Renderer2D.h"

#include "Backend/OpenGL/OpenGLRenderDevice.h"
#include "Renderer/BatchRenderer.h"
#include "Renderer/RenderDevice.h"
#include "Renderer/RenderQueue.h"

namespace Janus
{

class Renderer2D::Impl
{
public:
    explicit Impl(std::unique_ptr<RenderDevice> device)
        : ownedDevice(std::move(device)),
          devicePtr(ownedDevice.get()),
          batchRenderer(*devicePtr)
    {
    }

    explicit Impl(RenderDevice& device)
        : devicePtr(&device),
          batchRenderer(device)
    {
    }

    std::unique_ptr<RenderDevice> ownedDevice;
    RenderDevice* devicePtr = nullptr;
    BatchRenderer batchRenderer;
    RenderQueue queue;
    Viewport viewport;
    OrthographicCamera camera;
    RendererStatistics statistics;
};

Renderer2D::Renderer2D(std::unique_ptr<RenderDevice> device)
    : m_Impl(std::make_unique<Impl>(std::move(device)))
{
}

Renderer2D::Renderer2D(RenderDevice& device)
    : m_Impl(std::make_unique<Impl>(device))
{
}

Renderer2D::~Renderer2D() = default;

Result<std::unique_ptr<Renderer2D>> Renderer2D::Create()
{
    auto deviceResult = OpenGLRenderDevice::Create();

    if (!deviceResult)
    {
        return Result<std::unique_ptr<Renderer2D>>::Failure(
            deviceResult.GetError());
    }

    auto renderer = std::unique_ptr<Renderer2D>(
        new Renderer2D(std::move(deviceResult).Value()));

    return Result<std::unique_ptr<Renderer2D>>::Success(
        std::move(renderer));
}

void Renderer2D::SetViewport(Viewport viewport)
{
    m_Impl->viewport = viewport;
    m_Impl->devicePtr->SetViewport(viewport);
}

void Renderer2D::BeginFrame(const OrthographicCamera& camera)
{
    m_Impl->camera = camera;
    m_Impl->queue.Clear();
    m_Impl->statistics.Reset();

    const auto viewProjection =
        camera.ViewProjection(m_Impl->viewport);

    m_Impl->devicePtr->SetViewProjection(viewProjection);
    m_Impl->devicePtr->UseShader(ShaderHandle{1});
}

void Renderer2D::SubmitSprite(const Sprite& sprite)
{
    m_Impl->queue.Submit(sprite);
}

Result<void> Renderer2D::EndFrame()
{
    m_Impl->devicePtr->Clear(Color::White());

    return m_Impl->batchRenderer.Flush(
        m_Impl->queue.BuildBatches(),
        m_Impl->statistics);
}

const RendererStatistics& Renderer2D::GetStatistics() const noexcept
{
    return m_Impl->statistics;
}

Result<TextureHandle> Renderer2D::CreateTexture(
    const TextureDesc& desc)
{
    return m_Impl->devicePtr->CreateTexture(desc);
}

void Renderer2D::DestroyTexture(TextureHandle handle)
{
    m_Impl->devicePtr->DestroyTexture(handle);
}

} // namespace Janus
