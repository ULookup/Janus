#include "Renderer/Renderer2D.h"

#include "Backend/OpenGL/OpenGLRenderDevice.h"
#include "Renderer/BatchRenderer.h"
#include "Renderer/RenderDevice.h"
#include "Renderer/RenderQueue.h"

#include <unordered_map>
#include <utility>

namespace Janus
{

namespace
{

struct RenderTargetRecord
{
    FramebufferHandle framebuffer;
    TextureHandle colorTexture;
    Viewport viewport;
};

[[nodiscard]] bool SameViewport(
    Viewport left,
    Viewport right) noexcept
{
    return left.width == right.width
        && left.height == right.height;
}

} // namespace

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

    std::unordered_map<u32, RenderTargetRecord> renderTargets;
    u32 nextRenderTargetHandle = 1;
    RenderTargetHandle activeTarget;
    Color clearColor = Color::White();
};

Renderer2D::Renderer2D(std::unique_ptr<RenderDevice> device)
    : m_Impl(std::make_unique<Impl>(std::move(device)))
{
}

Renderer2D::Renderer2D(RenderDevice& device)
    : m_Impl(std::make_unique<Impl>(device))
{
}

Renderer2D::~Renderer2D()
{
    if (m_Impl == nullptr)
    {
        return;
    }

    if (m_Impl->activeTarget.value != 0)
    {
        m_Impl->devicePtr->BindDefaultFramebuffer();
        m_Impl->activeTarget = {};
    }

    for (const auto& [handle, target] : m_Impl->renderTargets)
    {
        (void)handle;
        m_Impl->devicePtr->DestroyFramebuffer(target.framebuffer);
        m_Impl->devicePtr->DestroyTexture(target.colorTexture);
    }

    m_Impl->renderTargets.clear();
}

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
    if (m_Impl->activeTarget.value != 0)
    {
        return;
    }

    m_Impl->camera = camera;
    m_Impl->clearColor = Color::White();
    m_Impl->queue.Clear();
    m_Impl->statistics.Reset();

    const auto viewProjection =
        camera.ViewProjection(m_Impl->viewport);

    m_Impl->devicePtr->SetViewProjection(viewProjection);
    m_Impl->devicePtr->UseShader(ShaderHandle{1});
}

Result<void> Renderer2D::BeginFrame(
    const RenderFrameDesc& desc)
{
    if (desc.viewport.width == 0 || desc.viewport.height == 0)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Render frame viewport dimensions must be non-zero.");
    }

    if (m_Impl->activeTarget.value != 0)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "An offscreen render target frame is already active.");
    }

    if (desc.target.value != 0)
    {
        const auto iterator =
            m_Impl->renderTargets.find(desc.target.value);

        if (iterator == m_Impl->renderTargets.end())
        {
            return Result<void>::Failure(
                ErrorCode::InvalidArgument,
                "Cannot begin a frame with an unknown render target.");
        }

        if (!SameViewport(iterator->second.viewport, desc.viewport))
        {
            return Result<void>::Failure(
                ErrorCode::InvalidArgument,
                "Render frame viewport must match render target dimensions.");
        }

        auto bound =
            m_Impl->devicePtr->BindFramebuffer(
                iterator->second.framebuffer);
        if (!bound)
        {
            return bound;
        }

        m_Impl->activeTarget = desc.target;
    }
    else
    {
        m_Impl->devicePtr->BindDefaultFramebuffer();
    }

    m_Impl->viewport = desc.viewport;
    m_Impl->camera = desc.camera;
    m_Impl->clearColor = desc.clearColor;
    m_Impl->queue.Clear();
    m_Impl->statistics.Reset();

    m_Impl->devicePtr->SetViewport(desc.viewport);
    m_Impl->devicePtr->SetViewProjection(
        desc.camera.ViewProjection(desc.viewport));
    m_Impl->devicePtr->UseShader(ShaderHandle{1});

    return Result<void>::Success();
}

void Renderer2D::SubmitSprite(const Sprite& sprite)
{
    m_Impl->queue.Submit(sprite);
}

Result<void> Renderer2D::EndFrame()
{
    m_Impl->devicePtr->Clear(m_Impl->clearColor);

    auto flushed = m_Impl->batchRenderer.Flush(
        m_Impl->queue.BuildBatches(),
        m_Impl->statistics);

    if (m_Impl->activeTarget.value != 0)
    {
        m_Impl->devicePtr->BindDefaultFramebuffer();
        m_Impl->activeTarget = {};
    }

    return flushed;
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

Result<RenderTargetHandle> Renderer2D::CreateRenderTarget(
    const RenderTargetDesc& desc)
{
    if (desc.width == 0 || desc.height == 0)
    {
        return Result<RenderTargetHandle>::Failure(
            ErrorCode::InvalidArgument,
            "Render target dimensions must be non-zero.");
    }

    TextureDesc textureDesc;
    textureDesc.width = desc.width;
    textureDesc.height = desc.height;

    auto texture =
        m_Impl->devicePtr->CreateTexture(textureDesc);
    if (!texture)
    {
        return Result<RenderTargetHandle>::Failure(
            texture.GetError());
    }

    FramebufferDesc framebufferDesc;
    framebufferDesc.colorTexture = texture.Value();
    framebufferDesc.width = desc.width;
    framebufferDesc.height = desc.height;

    auto framebuffer =
        m_Impl->devicePtr->CreateFramebuffer(framebufferDesc);
    if (!framebuffer)
    {
        m_Impl->devicePtr->DestroyTexture(texture.Value());
        return Result<RenderTargetHandle>::Failure(
            framebuffer.GetError());
    }

    const RenderTargetHandle handle{
        m_Impl->nextRenderTargetHandle++};

    m_Impl->renderTargets.emplace(
        handle.value,
        RenderTargetRecord{
            framebuffer.Value(),
            texture.Value(),
            Viewport{desc.width, desc.height}});

    return Result<RenderTargetHandle>::Success(handle);
}

Result<void> Renderer2D::ResizeRenderTarget(
    RenderTargetHandle handle,
    u32 width,
    u32 height)
{
    const auto iterator =
        m_Impl->renderTargets.find(handle.value);

    if (iterator == m_Impl->renderTargets.end())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Cannot resize an unknown render target.");
    }

    if (width == 0 || height == 0)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Render target dimensions must be non-zero.");
    }

    if (m_Impl->activeTarget.value == handle.value)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Cannot resize an active render target.");
    }

    const Viewport replacementViewport{width, height};
    if (SameViewport(iterator->second.viewport, replacementViewport))
    {
        return Result<void>::Success();
    }

    TextureDesc textureDesc;
    textureDesc.width = width;
    textureDesc.height = height;

    auto replacementTexture =
        m_Impl->devicePtr->CreateTexture(textureDesc);
    if (!replacementTexture)
    {
        return Result<void>::Failure(
            replacementTexture.GetError());
    }

    FramebufferDesc framebufferDesc;
    framebufferDesc.colorTexture = replacementTexture.Value();
    framebufferDesc.width = width;
    framebufferDesc.height = height;

    auto replacementFramebuffer =
        m_Impl->devicePtr->CreateFramebuffer(framebufferDesc);
    if (!replacementFramebuffer)
    {
        m_Impl->devicePtr->DestroyTexture(
            replacementTexture.Value());
        return Result<void>::Failure(
            replacementFramebuffer.GetError());
    }

    const FramebufferHandle oldFramebuffer =
        iterator->second.framebuffer;
    const TextureHandle oldTexture =
        iterator->second.colorTexture;

    iterator->second = RenderTargetRecord{
        replacementFramebuffer.Value(),
        replacementTexture.Value(),
        replacementViewport};

    m_Impl->devicePtr->DestroyFramebuffer(oldFramebuffer);
    m_Impl->devicePtr->DestroyTexture(oldTexture);

    return Result<void>::Success();
}

Result<void> Renderer2D::DestroyRenderTarget(
    RenderTargetHandle handle)
{
    const auto iterator =
        m_Impl->renderTargets.find(handle.value);

    if (iterator == m_Impl->renderTargets.end())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Cannot destroy an unknown render target.");
    }

    if (m_Impl->activeTarget.value == handle.value)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Cannot destroy an active render target.");
    }

    m_Impl->devicePtr->DestroyFramebuffer(
        iterator->second.framebuffer);
    m_Impl->devicePtr->DestroyTexture(
        iterator->second.colorTexture);
    m_Impl->renderTargets.erase(iterator);

    return Result<void>::Success();
}

Result<TextureHandle> Renderer2D::GetRenderTargetColorTexture(
    RenderTargetHandle handle) const
{
    const auto iterator =
        m_Impl->renderTargets.find(handle.value);

    if (iterator == m_Impl->renderTargets.end())
    {
        return Result<TextureHandle>::Failure(
            ErrorCode::InvalidArgument,
            "Cannot resolve an unknown render target.");
    }

    return Result<TextureHandle>::Success(
        iterator->second.colorTexture);
}

Result<TexturePresentationHandle>
Renderer2D::GetRenderTargetPresentationHandle(
    RenderTargetHandle handle) const
{
    const auto texture =
        GetRenderTargetColorTexture(handle);
    if (!texture)
    {
        return Result<TexturePresentationHandle>::Failure(
            texture.GetError());
    }

    return m_Impl->devicePtr->GetTexturePresentationHandle(
        texture.Value());
}

} // namespace Janus
